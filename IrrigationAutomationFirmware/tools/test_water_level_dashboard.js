// Structural and behavioral checks for water_level_dashboard_flow.json.
//
//     node tools/test_water_level_dashboard.js
//
// The decode function is executed with the real FPort 40 uplink recorded on
// 2026-09-29, both through the ChirpStack codec object and through the raw
// ten-byte fallback. Both paths must agree.
const fs = require('fs');
const path = require('path');

const file = path.join(__dirname, '..', 'examples', 'WaterLevel', 'include',
  'water_level_dashboard_flow.json');
const flow = JSON.parse(fs.readFileSync(file, 'utf8'));

const APP_ID = '12345678-1234-1234-1234-123456789abc';
const DEV_EUI = 'e41d8667e81335d0';
const ENV = { IRRIGATION_APP_ID: APP_ID, WATER_DEV_EUI: DEV_EUI };

let failures = 0;
let sectionStart = 0;
const fail = m => { failures++; console.log('FAIL ' + m); };
const ok = m => console.log('ok   ' + m);
// Report a section as passing only when it added no failures, so a failure
// never also prints an "ok" line for the same work.
const done = m => { if (failures === sectionStart) ok(m); sectionStart = failures; };

// --- structure ------------------------------------------------------------
const byId = new Map(flow.map(n => [n.id, n]));
const ids = flow.map(n => n.id);
if (new Set(ids).size !== ids.length) fail('duplicate node ids');
else ok('node ids unique (' + ids.length + ' nodes)');

for (const n of flow) {
  for (const wire of n.wires || []) {
    if (!Array.isArray(wire)) continue;
    for (const target of wire) {
      if (!byId.has(target)) fail(n.id + ' wires to missing node ' + target);
    }
  }
}
done('every wire target exists');

// Every ui widget must sit in a ui-group that resolves to the page and base.
const page = byId.get(byId.get(byId.get('sc_wl_ui').group).page);
const base = byId.get(page.ui);
for (const n of flow.filter(x => /^ui-(template|chart|button|text)$/.test(x.type))) {
  const group = byId.get(n.group);
  if (!group) { fail(n.name + ' has no resolvable ui-group'); continue; }
  if (group.page !== page.id) fail(n.name + ' is in a group on a different page');
  if (n.g !== 'sc_wl_group_flow') fail(n.name + ' is outside the flow canvas group');
}
if (!byId.has(page.theme) || byId.get(page.theme).type !== 'ui-theme') {
  fail('ui-page theme does not resolve to a ui-theme');
}
ok('ui chain: widgets -> Water Level / Water Level history -> ' + page.path +
  ' -> ' + base.name);

// The page and its groups are the only config nodes the flow genuinely adds, so
// their ids are the whole question of whether an import creates them or reuses
// the copies the workspace already has. Node-RED preserves incoming ids and
// raises an import conflict when one is taken - a clashing *config* node starts
// unticked in that dialog, so the existing page wins and nothing is duplicated.
// A builder-minted id matches nothing and is imported as-is, which puts a second
// "Water Level" page and a second pair of groups on the canvas every time. The
// user's instance owns 334b707b21a5ce0a / 0dcf231c222554fc / 18fb832a66d8775e.
for (const n of flow.filter(x => x.type === 'ui-page' || x.type === 'ui-group')) {
  if (/^sc_wl[_-]/.test(n.id)) {
    fail(n.type + ' ' + n.id + ' is builder-minted, so importing duplicates "' +
      (n.name || n.id) + '" instead of reusing the existing one');
  }
}
done('ui-page and ui-groups carry workspace ids, so imports reuse them');

// --- env-var configuration ------------------------------------------------
const subscribe = byId.get('sc_wl_subscribe');
if (!subscribe) fail('subscribe function is missing');
for (const [name, fn] of [['subscribe', subscribe], ['decode', byId.get('sc_wl_decode')]]) {
  for (const key of ['IRRIGATION_APP_ID', 'WATER_DEV_EUI']) {
    if (!fn.func.includes('env.get(\'' + key + '\')')) {
      fail(name + ' function does not read ' + key + ' from the environment');
    }
  }
}
if (/f21e23b5|e41d8667/.test(JSON.stringify(byId.get('sc_wl_subscribe').func))) {
  fail('subscribe function must not hardcode the application ID or DevEUI');
}
done('both functions read IRRIGATION_APP_ID and WATER_DEV_EUI; nothing hardcoded');

// Dynamic subscription: the input must take its topic from the subscribe node.
// Import must drop into an existing flow tab. A "tab" node in the file makes
// Node-RED create a new, identical tab on every import instead, so the nodes
// reference a tab id that is deliberately absent, as MainValve and PumpControl
// do. The dashboard page is attached to the shared base, not to a Node-RED tab.
const tabs = flow.filter(n => n.type === 'tab');
if (tabs.length) {
  fail('flow must not ship a tab node, or importing creates a duplicate tab: ' +
    tabs.map(t => t.label || t.id).join(', '));
} else {
  const referenced = [...new Set(flow.map(n => n.z).filter(Boolean))];
  if (referenced.length !== 1) {
    fail('nodes span several flows: ' + referenced.join(', '));
  } else if (byId.has(referenced[0])) {
    fail('nodes point at a tab id that is present in the file');
  } else {
    ok('no tab node; the single z ' + referenced[0] + ' lets Node-RED import ' +
      'into the flow you choose');
  }
}

const beforeSubscription = failures;
const mqttIn = byId.get('sc_wl_mqtt_in');
if (mqttIn.topic !== '') fail('dynamic-subscription mqtt in must have an empty topic');
if (mqttIn.inputs !== 1) fail('mqtt in must accept dynamic subscription input (inputs: 1)');
// Dynamic subscription flows the other way: the function writes the topic to
// the mqtt in node's input, so the subscribe output must reach the mqtt in.
if (!(subscribe.wires[0] || []).includes('sc_wl_mqtt_in')) {
  fail('subscribe function does not drive the mqtt in node');
}
if ((mqttIn.wires[0] || []).includes('sc_wl_subscribe')) {
  fail('mqtt in must not feed the subscribe function');
}
if (!(subscribe.wires[1] || []).includes('sc_wl_ui')) {
  fail('subscribe function does not refresh the card on a configuration error');
}
if (!/action\s*:\s*'subscribe'/.test(subscribe.func)) {
  fail('subscribe function does not emit a subscribe action');
}
if (failures === beforeSubscription) {
  ok('mqtt in uses dynamic subscription driven by the subscribe function');
}

// --- charts ---------------------------------------------------------------
const charts = Object.fromEntries(
  flow.filter(n => n.type === 'ui-chart').map(n => [n.id, n]));
if (Object.keys(charts).length !== 2) {
  fail('expected 2 history charts, found ' + Object.keys(charts).length);
}
for (const [id, label, yProperty] of [
  ['sc_wl_chart_depth', 'Water depth (cm)', 'payload'],
  ['sc_wl_chart_pressure', 'Pressure (bar)', 'payload']]) {
  const c = charts[id];
  if (!c) { fail('missing chart ' + id); continue; }
  if (c.label !== label) fail(id + ' label is ' + c.label);
  if (c.yAxisProperty !== yProperty) fail(id + ' yAxisProperty is ' + c.yAxisProperty);
  if (c.xAxisType !== 'time') fail(id + ' xAxisType must be time');
  if (c.action !== 'append') fail(id + ' must append points');
}
// Each chart must be fed by its own decode output, through the zoom function.
const decodeWires = byId.get('sc_wl_decode').wires;
if (decodeWires.length !== 3) fail('decode must have exactly 3 outputs');
for (const [out, chartId] of [[1, 'sc_wl_chart_depth'], [2, 'sc_wl_chart_pressure']]) {
  if (!(decodeWires[out] || []).includes('sc_wl_zoom')) {
    fail('decode output ' + out + ' does not feed the zoom function');
  }
  if ((decodeWires[out] || []).includes(chartId)) {
    fail('decode output ' + out + ' bypasses the zoom function');
  }
}
done('water depth and pressure charts are wired through their own decode outputs');

// --- zoom buttons ---------------------------------------------------------
const zoomFn = byId.get('sc_wl_zoom');
if (!zoomFn) fail('chart time zoom function is missing');
else if (zoomFn.outputs !== 4) fail('zoom function must have 4 outputs, has ' + zoomFn.outputs);
// out 0..1 feed the charts, out 2..3 feed the labels.
for (const [out, target] of [
  [0, 'sc_wl_chart_depth'], [1, 'sc_wl_chart_pressure'],
  [2, 'sc_wl_zoom_depth'], [3, 'sc_wl_zoom_pressure']]) {
  if (!(zoomFn.wires[out] || []).includes(target)) {
    fail('zoom output ' + out + ' does not feed ' + target);
  }
}
// Each zoom template must send back into the zoom function, stacked vertically.
const zooms = [['sc_wl_zoom_depth', 'depth', 'sc_wl_chart_depth'],
  ['sc_wl_zoom_pressure', 'pressure', 'sc_wl_chart_pressure']];
for (const [id, metric, chartId] of zooms) {
  const z = byId.get(id);
  if (!z) { fail('missing zoom template ' + id); continue; }
  if (z.group !== charts[chartId].group) fail(id + ' is not in the chart group');
  if (z.height !== charts[chartId].height) {
    fail(id + ' height ' + z.height + ' must match its chart height ' + charts[chartId].height);
  }
  if (!(z.wires[0] || []).includes('sc_wl_zoom')) fail(id + ' does not send into the zoom function');
  if (!z.format.includes('metric:"' + metric + '"')) {
    fail(id + ' does not send metric ' + metric);
  }
  if (!/flex-direction:column/.test(z.format)) fail(id + ' buttons are not stacked vertically');
  if (!/@click="zoom\(1\)"/.test(z.format) || !/@click="zoom\(-1\)"/.test(z.format)) {
    fail(id + ' is missing its + or - button');
  }
}
// The label must default to the window the function defaults to (index 3, 6 h).
for (const [id] of zooms) {
  const z = byId.get(id);
  if (!/zoom_state'?\s*\?\s*msg\.payload\.label\s*:\s*'6 h'/.test(z.format.replace(/\s+/g, ' '))) {
    fail(id + ' default label does not match the 6 h default window');
  }
}
done('each chart has stacked + / - zoom buttons that report back');

// The zoom function must window the axis and keep per-chart state.
const zoomRun = (msg, key) => {
  let stored = key;
  const ctx = {
    flow: { get: () => stored, set: (k, v) => { stored = v; } },
    node: { status: () => {}, warn: () => {} }
  };
  const fn = new Function('msg', 'flow', 'node', 'Buffer', zoomFn.func);
  return { out: fn(msg, ctx.flow, ctx.node, Buffer), stored };
};
const pointOut = zoomRun({ waterMetric: 'depth', topic: 'Water depth', payload: 45.5 });
if (!Array.isArray(pointOut.out) || pointOut.out.length !== 4) {
  fail('zoom function must return 4 outputs, got ' + JSON.stringify(pointOut.out));
} else {
  const sent = pointOut.out[0];
  if (!Array.isArray(sent) || sent.length !== 2) {
    fail('a chart point must send the axis update and the point, got ' + JSON.stringify(sent));
  } else if (sent[1].payload !== 45.5) fail('zoom function altered the point value');
  if (pointOut.out[1] !== null) fail('a depth point must not reach the pressure chart');
}
const zoomIn = zoomRun({ payload: { kind: 'zoom', metric: 'pressure', direction: 1 } }, 3);
if (zoomIn.stored !== 2) fail('+ must shorten the window (index 3 -> 2), got ' + zoomIn.stored);
if (!zoomIn.out[1] || !zoomIn.out[1].ui_update) fail('+ must send an axis update to its chart');
if (zoomIn.out[0] !== null) fail('a pressure zoom must not reach the depth chart');
if (!zoomIn.out[3] || !zoomIn.out[3].payload.label) fail('+ must update its own label');
if (zoomIn.out[2] !== null) fail('a pressure zoom must not update the depth label');
const zoomOut = zoomRun({ payload: { kind: 'zoom', metric: 'depth', direction: -1 } }, 3);
if (zoomOut.stored !== 4) fail('- must lengthen the window (index 3 -> 4), got ' + zoomOut.stored);
if (zoomOut.out[2].payload.label !== '12 h') fail('- label is ' + zoomOut.out[2].payload.label);
const clamped = zoomRun({ payload: { kind: 'zoom', metric: 'depth', direction: -1 } }, 5);
if (clamped.stored !== 5) fail('zoom must clamp at the longest window, got ' + clamped.stored);
if (zoomRun({ waterMetric: 'bogus', payload: 1 }).out !== null) fail('unknown metric must be ignored');
if (zoomRun({ payload: 'x' }).out !== null) fail('non-numeric payload must be ignored');
done('zoom function windows the axis per chart, clamps, and ignores bad input');

// --- card -----------------------------------------------------------------
const format = byId.get('sc_wl_ui').format;
if (typeof format !== 'string' || !format.trim()) fail('ui-template has no format string');
else if (!/^<template>[\s\S]*<\/template>[\s\S]*<script>[\s\S]*<\/script>/.test(format)) {
  fail('ui-template format is missing its template/script blocks');
} else ok('ui-template carries template and script blocks');
const openTags = (format.match(/<div\b/g) || []).length;
const closeTags = (format.match(/<\/div>/g) || []).length;
if (openTags !== closeTags) fail('unbalanced <div>: ' + openTags + ' open, ' + closeTags + ' close');
else ok('card markup has balanced div tags (' + openTags + ')');
for (const field of ['water_level_percent', 'depth_cm', 'pressure_bar', 'battery_voltage_v',
  'load_on', 'pressure_valid', 'rssi', 'snr', 'last_seen', 'uplink_count', 'f_cnt',
  'dr', 'sf', 'gateway_id', 'frequency_hz', 'decoded_from', 'config_error', 'decode_error']) {
  if (!format.includes(field)) fail('card does not render decoder field ' + field);
}
// Dashboard 2.0 reserves `value` as a component method; shadowing it blanks the page.
if (/^\s*value\s*\(/m.test(format)) fail('card must not declare a method named value');
ok('card renders all decoder fields and avoids the reserved value method');

// --- staleness ------------------------------------------------------------
const staleInject = byId.get('sc_wl_stale_inject');
const staleFn = byId.get('sc_wl_stale');
if (!staleInject) fail('staleness watchdog inject is missing');
else {
  if (!(staleInject.wires[0] || []).includes('sc_wl_stale')) {
    fail('staleness inject does not drive the check function');
  }
  if (!(Number(staleInject.repeat) > 0)) fail('staleness inject must repeat');
}
if (!staleFn) fail('Check stale status function is missing');
else {
  if (!(staleFn.wires[0] || []).includes('sc_wl_ui')) {
    fail('stale check does not refresh the card');
  }
  // The watchdog must use arrival time, not the gateway clock.
  if (!/last_seen_ms/.test(staleFn.func)) fail('stale check must use last_seen_ms');
  // The limit is only meaningful against the firmware's sleep interval: the node
  // sends one uplink per cycle, so the question is how many cycles of silence
  // mean "gone" rather than "late". Bounded from both sides -- too tight and a
  // single lost uplink flags a healthy node, too slack and nobody hears about a
  // node that has stopped.
  const m = /STALE_AFTER_SECONDS\s*=\s*(\d+)/.exec(staleFn.func);
  if (!m) fail('stale check has no STALE_AFTER_SECONDS constant');
  else {
    const limit = Number(m[1]);
    // An uncommented build flag wins; otherwise the firmware sleeps for the
    // header default. Read both, and fail loudly when neither is found, so the
    // comparison can never pass by reading nothing.
    const ini = fs.readFileSync(path.join(__dirname, '..', 'platformio.ini'), 'utf8');
    const header = fs.readFileSync(path.join(__dirname, '..', 'examples', 'WaterLevel',
      'src', 'WaterLevelConfig.h'), 'utf8');
    const fromIni = /^\s*-D\s*WATER_LEVEL_SLEEP_SECONDS=(\d+)/m.exec(ini);
    const fromHeader = /#define\s+WATER_LEVEL_SLEEP_SECONDS\s+(\d+)/.exec(header);
    const sleep = fromIni ? fromIni[1] : fromHeader ? fromHeader[1] : null;
    const repeat = staleInject ? Number(staleInject.repeat) || 0 : 0;
    if (!sleep) {
      fail('cannot find WATER_LEVEL_SLEEP_SECONDS in platformio.ini or WaterLevelConfig.h');
    } else if (limit < 2 * Number(sleep)) {
      // Class A uplinks here are unconfirmed, so a lost report is normal
      // operation. One of them must not paint a live node amber.
      fail('staleness limit ' + limit + ' s does not clear one whole ' + sleep +
        ' s cycle, so a single lost uplink flags a healthy node as stale');
    } else if (limit > Math.max(3 * Number(sleep), 120)) {
      // Too slack is how the old flat 1200 s behaved at the bench: a node that
      // stopped was reported fresh for 120 missed cycles.
      fail('staleness limit ' + limit + ' s lets a stopped node read fresh for ' +
        Math.round(limit / Number(sleep)) + ' missed ' + sleep + ' s cycles');
    } else if (!repeat) {
      fail('cannot read the staleness watchdog period from the inject');
    } else if (2 * repeat > limit) {
      fail('staleness watchdog runs every ' + repeat + ' s against a ' + limit +
        ' s limit, so detection can lag a whole cycle');
    } else {
      ok('staleness limit ' + limit + ' s against the ' + sleep + ' s sleep interval (' +
        (fromIni ? 'platformio.ini' : 'WaterLevelConfig.h') + '); a stopped node reads ' +
        'stale within ' + (limit + repeat) + ' s');
    }
  }
}
// The card must show staleness and must not present stale data as current.
for (const [needle, why] of [
  ['state.stale', 'the card never reads the stale flag'],
  ['ageText', 'the card never shows how old the reading is'],
  ['last_seen_ms', 'the card cannot compute age without last_seen_ms'],
  ['stale_after_seconds', 'the card never shows the freshness limit'],
  ['wl-stale', 'the card has no stale banner'],
  ['wl-pill.stale', 'the card has no stale pill style'],
  ['signalText', 'the card exposes signal without a staleness guard']]) {
  if (!format.includes(needle)) fail(why);
}
// signalText must short-circuit to Unknown on stale before touching rssi/snr.
if (!/signalText\s*\(\)\s*\{\s*if\s*\(this\.state\.stale\)\s*return\s*'Unknown'/.test(
  format.replace(/\s+/g, ' '))) {
  fail('signalText must return Unknown while stale');
}
if (!/stale:\s*true/.test(format)) fail('card must start in the stale state');

// The watchdog must actually flip the flag, and a fresh uplink must clear it.
const staleRun = (msAgo) => {
  let stored = msAgo === null ? {} : { last_seen_ms: Date.now() - msAgo };
  const ctx = {
    flow: { get: () => stored, set: (k, v) => { stored = Object.assign({}, v); } },
    node: { status: () => {}, warn: () => {} }
  };
  const fn = new Function('msg', 'flow', 'node', 'Buffer', staleFn.func);
  const out = fn({ payload: 'tick' }, ctx.flow, ctx.node, Buffer);
  return out.payload.state;
};
const limit = Number(/STALE_AFTER_SECONDS\s*=\s*(\d+)/.exec(staleFn.func)[1]);
if (staleRun(null).stale !== true) fail('no telemetry yet must read stale');
if (staleRun(0).stale !== false) fail('a just-arrived uplink must not read stale');
if (staleRun(limit * 1000 - 1000).stale !== false) fail('before the limit must not read stale');
if (staleRun(limit * 1000 + 1000).stale !== true) fail('past the limit must read stale');
if (staleRun(limit * 1000 + 1000).stale_after_seconds !== limit) {
  fail('stale check must publish stale_after_seconds for the card');
}
done('staleness flips past the limit, clears on a new uplink, and reaches the card');

// Telemetry-only: no downlink path may exist.
if (flow.some(n => n.type === 'mqtt out')) fail('telemetry-only flow must not contain an mqtt out node');
else ok('no mqtt out node (telemetry only)');

// The mqtt in must borrow the workspace's shared ChirpStack broker. Shipping
// our own broker config node arrives on import as a second server to delete by
// hand, and if it reused the shared id it could overwrite the existing host,
// port and TLS settings instead. MainValve is the reference for the shared id.
const mainValve = JSON.parse(fs.readFileSync(path.join(__dirname, '..', 'examples',
  'MainValve', 'include', 'main_valve_dashboard2_updated_flow.json'), 'utf8'));
const sharedBroker = (mainValve.find(n => n.type === 'mqtt in') || {}).broker;
if (!sharedBroker) fail('cannot read the shared broker id from MainValve');
else if (flow.some(n => n.type === 'mqtt-broker')) {
  fail('flow must not ship an mqtt-broker config node; it duplicates the shared server');
} else if (mqttIn.broker !== sharedBroker) {
  fail('mqtt in references ' + mqttIn.broker + ' but the shared broker is ' + sharedBroker);
} else {
  ok('mqtt in borrows the shared broker ' + sharedBroker + ' and ships no broker node');
}

// --- decode function behavior --------------------------------------------
const decode = byId.get('sc_wl_decode').func;
const run = (topic, payload, seed, env) => {
  const state = Object.assign({}, seed);
  const ctx = {
    flow: { get: () => state, set: (k, v) => Object.assign(state, v) },
    node: { warn: m => console.log('warn ' + m), status: () => {} }
  };
  const fn = new Function('msg', 'flow', 'node', 'env', 'Buffer', decode);
  const out = fn({ topic, payload }, ctx.flow, ctx.node,
    { get: k => (env === undefined ? ENV : env)[k] }, Buffer);
  return { out, state };
};

const SAMPLE = {
  deduplicationId: 'ca5ee939-0ce2-46e3-892c-ba1dca1f3ed3',
  time: '2026-09-29T11:34:03.261368269+00:00',
  fPort: 40, fCnt: 10, dr: 5,
  data: 'AQMxNQAtAccAWw==',
  object: {
    protocol_version: 1, water_level_percent: 9.1, battery_voltage_v: 12.597,
    runtime_mode: 'water_level_class_a', depth_m: 0.455, pressure_valid: true,
    pressure_bar: 0.045, load_on: true
  },
  rxInfo: [{ gatewayId: 'e45f01fffea6f91c', rssi: -66, snr: 9.75 }],
  txInfo: { frequency: 868300000, modulation: { lora: { spreadingFactor: 7 } } }
};
const TOPIC = 'application/' + APP_ID + '/device/' + DEV_EUI + '/event/up';

const viaObject = run(TOPIC, JSON.stringify(SAMPLE));
const s1 = viaObject.out[0].payload.state;
const expect = {
  water_level_percent: 9.1, depth_m: 0.455, depth_cm: 45.5,
  battery_voltage_v: 12.597, pressure_bar: 0.045, pressure_valid: true,
  load_on: true, protocol_version: 1, f_cnt: 10, dr: 5, sf: 7,
  rssi: -66, snr: 9.75, gateway_id: 'e45f01fffea6f91c',
  frequency_hz: 868300000, uplink_count: 1
};
for (const [k, v] of Object.entries(expect)) {
  if (s1[k] !== v) fail('codec path ' + k + ' = ' + JSON.stringify(s1[k]) + ', expected ' + JSON.stringify(v));
}
if (s1.last_seen !== SAMPLE.time) fail('codec path last_seen not set from event.time');
if (s1.decode_error !== '') fail('codec path should clear decode_error');
if (viaObject.out[0].payload.kind !== 'state') fail('codec path payload.kind is not state');
done('codec-object uplink decodes to the expected state (9.1 %, 45.5 cm, 12.597 V)');

// The chart points must carry the same numbers the card shows.
if (viaObject.out[1].payload !== 45.5 || viaObject.out[1].topic !== 'Water depth') {
  fail('depth chart point is ' + JSON.stringify(viaObject.out[1]));
}
if (viaObject.out[2].payload !== 0.045 || viaObject.out[2].topic !== 'Pressure') {
  fail('pressure chart point is ' + JSON.stringify(viaObject.out[2]));
}
done('chart points match the card: 45.5 cm depth, 0.045 bar pressure');

// Base64 fallback must agree with the codec object byte for byte.
const noObject = Object.assign({}, SAMPLE);
delete noObject.object;
const viaRaw = run(TOPIC, JSON.stringify(noObject)).out;
for (const k of ['water_level_percent', 'depth_m', 'depth_cm', 'battery_voltage_v',
  'pressure_bar', 'pressure_valid', 'load_on', 'protocol_version']) {
  if (viaRaw[0].payload.state[k] !== s1[k]) {
    fail('raw fallback disagrees with codec on ' + k + ': ' +
      JSON.stringify(viaRaw[0].payload.state[k]) + ' vs ' + JSON.stringify(s1[k]));
  }
}
if (viaRaw[1].payload !== 45.5 || viaRaw[2].payload !== 0.045) {
  fail('raw fallback chart points disagree: ' + JSON.stringify([viaRaw[1].payload, viaRaw[2].payload]));
}
// The card must say which path decoded the uplink, so a missing ChirpStack
// codec is visible instead of silently equivalent.
if (s1.decoded_from !== 'codec') fail('codec path decoded_from is ' + s1.decoded_from);
if (viaRaw[0].payload.state.decoded_from !== 'raw payload') {
  fail('raw fallback decoded_from is ' + viaRaw[0].payload.state.decoded_from);
}
done('ten-byte base64 fallback matches the codec object exactly and reports its path');

// Invalid sensor: level fields must not survive from previous state, and the
// charts must receive no point rather than a null one.
const invalid = Object.assign({}, SAMPLE, {
  object: {
    protocol_version: 1, water_level_percent: null, battery_voltage_v: 12.1,
    runtime_mode: 'water_level_class_a', depth_m: null, pressure_valid: false,
    pressure_bar: null, load_on: false
  }
});
const inv = run(TOPIC, JSON.stringify(invalid), s1).out;
const s2 = inv[0].payload.state;
if (s2.pressure_valid !== false) fail('invalid path pressure_valid');
for (const k of ['water_level_percent', 'depth_m', 'depth_cm', 'pressure_bar']) {
  if (s2[k] !== null) fail('invalid sensor must null ' + k + ', got ' + JSON.stringify(s2[k]));
}
if (s2.battery_voltage_v !== 12.1) fail('invalid sensor must still report battery');
if (s2.uplink_count !== 2) fail('uplink_count must increment, got ' + s2.uplink_count);
if (inv[1] !== null) fail('invalid sensor must not send a depth chart point, got ' + JSON.stringify(inv[1]));
if (inv[2] !== null) fail('invalid sensor must not send a pressure chart point, got ' + JSON.stringify(inv[2]));
done('pressure_valid false clears level fields, keeps battery, charts no point');

// Rejections: wrong EUI, wrong application, wrong FPort, non-up kinds.
const other = 'application/' + APP_ID + '/device/0000000000000000/event/up';
if (run(other, JSON.stringify(SAMPLE)).out !== null) fail('foreign DevEUI must be rejected');
const wrongApp = 'application/00000000-0000-0000-0000-000000000000/device/' + DEV_EUI + '/event/up';
if (run(wrongApp, JSON.stringify(SAMPLE)).out !== null) fail('foreign application must be rejected');
const wrongPort = Object.assign({}, SAMPLE, { fPort: 31 });
if (run(TOPIC, JSON.stringify(wrongPort)).out !== null) fail('non-FPort 40 uplink must be rejected');
if (run(TOPIC, 'not json').out !== null) fail('malformed JSON must be rejected');
if (run(TOPIC, JSON.stringify(SAMPLE), undefined, {}).out !== null) {
  fail('unset environment variables must not match any topic');
}
const join = run(TOPIC.replace('/up', '/join'), JSON.stringify({ time: SAMPLE.time })).out;
if (!join || !join[0].payload.state.network_join) fail('join event must record network_join');
else if (join[1] !== null || join[2] !== null) fail('non-up events must not send chart points');
else ok('foreign EUI/application, FPort 40 guard, bad JSON, unset env and join events behave');

// Truncated raw payload must surface decode_error, not invent values.
const truncated = Object.assign({}, SAMPLE, { data: 'AQMxNQAt' });
delete truncated.object;
const bad = run(TOPIC, JSON.stringify(truncated)).out;
const s3 = bad[0].payload.state;
if (!s3.decode_error) fail('truncated payload must set decode_error');
else if (bad[1] !== null || bad[2] !== null) fail('decode failure must not send chart points');
else ok('truncated payload sets decode_error and sends no chart point');

// The subscribe function must surface missing configuration on the card.
const subFn = subscribe.func;
const subRun = (env) => {
  let stored = {};
  const ctx = {
    flow: { get: () => stored, set: (k, v) => { stored = Object.assign({}, v); } },
    node: { status: () => {}, warn: () => {} }
  };
  const fn = new Function('msg', 'flow', 'node', 'env', 'context', 'Buffer', subFn);
  return fn({ payload: 'x' }, ctx.flow, ctx.node,
    { get: k => env[k] }, { get: () => null, set: () => {} }, Buffer);
};
const missing = subRun({});
if (!missing || !missing[1] || !missing[1].payload.state.config_error) {
  fail('missing env vars must produce a card config_error');
} else if (missing[0] !== null) {
  fail('missing env vars must not emit a subscribe action');
} else if (!/IRRIGATION_APP_ID/.test(missing[1].payload.state.config_error)) {
  fail('config_error must name the missing variable');
} else ok('missing environment variables surface a config_error on the card');

const subOk = subRun(ENV);
const expectedTopic = 'application/' + APP_ID + '/device/' + DEV_EUI + '/event/+';
if (!subOk || !subOk[0] || subOk[0].action !== 'subscribe' ||
    subOk[0].topic !== expectedTopic) {
  fail('configured env vars must subscribe to ' + expectedTopic + ', got ' +
    JSON.stringify(subOk && subOk[0]));
} else if (subOk[1] && subOk[1].payload.state.config_error) {
  fail('configured subscribe must clear config_error');
} else ok('configured environment variables subscribe to ' + expectedTopic);

console.log(failures ? '\n' + failures + ' failure(s)' : '\nall checks passed');
process.exit(failures ? 1 : 0);
