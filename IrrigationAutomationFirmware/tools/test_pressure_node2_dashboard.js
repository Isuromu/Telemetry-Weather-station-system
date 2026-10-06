// Checks for pressure_node2_dashboard_flow.json (valve_2).
//
//     node tools/test_pressure_node2_dashboard.js
//
// Regenerate the flow with tools/build_pressure_node2_dashboard.py after
// changing the builder. valve_2 has no flow meter, so the charts carry pressure
// and battery only; this runs the tracker, the command builder and the chart
// path with a fake Node-RED environment.
const c = require('./flow_test_common.js');

const flow = c.loadFlow('PressureControlNode2/include/pressure_node2_dashboard_flow.json');
const byId = new Map(flow.map(n => [n.id, n]));
const APP = c.APP_ID;
const EUI = '0000000000000005';
const ENV = { IRRIGATION_APP_ID: APP, VALVE2_DEV_EUI: EUI };
const TOPIC = 'application/' + APP + '/device/' + EUI + '/event/up';

const TRACK = byId.get('d7155969941e41f6');
const SEND = byId.get('aa6da601debd0c9b');
const CHARTS = byId.get('fc0338054ac37d9f');
const ZOOM = byId.get('feebcae906e80b11');

c.checkImportInvariants(flow, { name: 'valve_2' });

// The pressure-scale flag is a build-time constant in PressureNode2Config.h, not
// a live reading, so the card must not present it as one.
for (const node of flow.filter(n => n.type === 'ui-template')) {
  if (/Sensor scale|not validated/.test(node.format || '')) {
    c.fail('the card still presents the build-time scale flag: ' + node.id);
  }
}
c.done('valve_2: the card does not present the build-time scale flag');

function statusEvent(object, extra) {
  return Object.assign({
    fPort: 31, fCnt: 9, dr: 5, adr: true, devAddr: '00aabbcc',
    time: '2026-10-05T13:26:58.759892249+00:00',
    deviceInfo: { deviceClassEnabled: 'CLASS_A' },
    rxInfo: [{ rssi: -74, snr: 6, gatewayId: 'aabbccddeeff0011' }],
    txInfo: { frequency: 867900000, modulation: { lora: { spreadingFactor: 7, bandwidth: 125000 } } },
    object,
  }, extra);
}

// The tracker answers on msg.payload, so read its single output once: a second
// run over the same message would decode the state wrapper it just wrote.
const stateOf = msg => {
  const first = c.messages(c.runNode(TRACK.func, { env: ENV, msg }).result)[0];
  return first && first.payload ? first.payload.state : undefined;
};

// --- FPort 31 status -------------------------------------------------------
const object = {
  protocol_version: 2, runtime_mode: 'class_a', status_reason: 'scheduled',
  pcv_last_commanded: 'open', pcv_position_verified: true, sleep_seconds: 900,
  last_command_id: 12, battery_voltage_v: 3.62, battery_soc_percent: 71,
  upstream_pressure_bar: 1.84, upstream_temperature_c: 19.4, upstream_scale_validated: true,
  downstream_pressure_bar: 1.79, downstream_temperature_c: 19.6, downstream_scale_validated: true,
};
const state = stateOf({ topic: TOPIC, payload: statusEvent(object) });
if (!state) c.fail('the FPort 31 status did not reach the card');
else {
  const expected = {
    runtime_mode: 'class_a', status_reason: 'scheduled', pcv_last_commanded: 'open',
    pcv_position_verified: true, sleep_seconds: 900, last_command_id: 12,
    battery_voltage_v: 3.62, battery_soc_percent: 71, upstream_pressure_bar: 1.84,
    upstream_temperature_c: 19.4, downstream_pressure_bar: 1.79, downstream_temperature_c: 19.6,
  };
  for (const [key, value] of Object.entries(expected)) {
    if (state[key] !== value) c.fail('state.' + key + ' is ' + JSON.stringify(state[key]) + ', expected ' + JSON.stringify(value));
  }
  if (state.decode_error !== '') c.fail('a valid status set decode_error: ' + state.decode_error);
  if (state.uplink_count !== 1) c.fail('uplink_count did not count the uplink');
}
c.done('valve_2: the FPort 31 status object reaches the card');

if (!/valve_2 status is FPort 31/.test((stateOf({ topic: TOPIC,
  payload: statusEvent(undefined, { fPort: 20 }) }) || {}).decode_error || '')) {
  c.fail('an uplink on the wrong FPort is not explained');
}
if (!/codec/i.test((stateOf({ topic: TOPIC,
  payload: statusEvent(undefined) }) || {}).decode_error || '')) {
  c.fail('a status without a decoded object does not mention the codec');
}
c.done('valve_2: wrong FPort and missing codec object are both explained');

// --- command builder -------------------------------------------------------
function send(action, state) {
  return c.runNode(SEND.func, {
    env: ENV,
    flow: { pn2_state: state || { last_command_id: 12 }, pn2_next_cmd_id: 13 },
    msg: { payload: action },
  });
}
const sent = c.messages(send({ kind: 'command', pcv: 'close', sleep_seconds: 600 }).result)
  .find(m => typeof m.topic === 'string' && m.topic.endsWith('/command/down'));
if (!sent) c.fail('the valve command built no downlink');
else {
  if (sent.topic !== 'application/' + APP + '/device/' + EUI + '/command/down') {
    c.fail('downlink topic built from the wrong identifiers: ' + sent.topic);
  }
  if (sent.payload.fPort !== 30) c.fail('downlink is not FPort 30: ' + sent.payload.fPort);
  if (sent.payload.devEui !== EUI) c.fail('downlink devEui is ' + sent.payload.devEui);
  // The field names are the codec's encodeDownlink inputs.
  if (sent.payload.object.pcv !== 'close') c.fail('downlink pcv is ' + sent.payload.object.pcv);
  if (sent.payload.object.sleep_seconds !== 600) c.fail('downlink sleep is ' + sent.payload.object.sleep_seconds);
  if (sent.payload.object.command_id !== 13) c.fail('downlink command id is ' + sent.payload.object.command_id);
}
c.done('valve_2: the command builds an FPort 30 downlink with the codec field names');

const badSleep = send({ kind: 'command', sleep_seconds: 5 });
if (c.messages(badSleep.result).some(m => m.topic)) c.fail('a five-second interval was sent');
if (!/10 to 86400/.test((badSleep.store.f_pn2_state || {}).ui_error || '')) {
  c.fail('an out-of-range interval did not explain itself');
}
const empty = send({ kind: 'command' });
if (c.messages(empty.result).some(m => m.topic)) c.fail('an empty command was sent');
if (!/Choose a valve action/.test((empty.store.f_pn2_state || {}).ui_error || '')) {
  c.fail('an empty command did not explain itself');
}
c.done('valve_2: an out-of-range or empty command is refused with a message');

// --- chart path ------------------------------------------------------------
const charts = c.runNode(CHARTS.func, { env: ENV,
  msg: { topic: TOPIC, payload: statusEvent(object) } });
const outputs = Array.isArray(charts.result) ? charts.result : [charts.result];
const pressure = (outputs[0] || []).map(m => m.topic);
if (pressure.join(',') !== 'Upstream,Downstream') {
  c.fail('the pressure chart got ' + JSON.stringify(pressure));
}
if (!outputs[0] || outputs[0][0].pn2Metric !== 'pressure' || outputs[0][0].payload !== 1.84) {
  c.fail('the pressure series is not labelled for the chart');
}
if (!outputs[1] || outputs[1].pn2Metric !== 'battery' || outputs[1].payload !== 3.62) {
  c.fail('the battery series is not labelled for the chart');
}
if (c.messages(c.runNode(CHARTS.func, { env: ENV,
  msg: { topic: TOPIC, payload: statusEvent(undefined, { fPort: 20 }) } }).result).length) {
  c.fail('a non-status uplink reached the charts');
}
c.done('valve_2: the chart inputs carry pressure and battery and nothing else');

const zoom = action => {
  const out = c.runNode(ZOOM.func, { env: ENV, flow: { pn2_zoom_battery: 3 },
    msg: action });
  return Array.isArray(out.result) ? out.result : [out.result];
};
const pressurePoint = zoom({ pn2Metric: 'pressure', topic: 'Upstream', payload: 1.84 });
if (!Array.isArray(pressurePoint[0]) || pressurePoint[0].length !== 2) {
  c.fail('a pressure point did not produce the axis update and the point');
} else if (!pressurePoint[0][0].ui_update || pressurePoint[0][1].payload !== 1.84) {
  c.fail('the pressure output is not an axis update followed by the point');
}
if (pressurePoint[1] !== null) c.fail('a pressure point also wrote the battery chart');
const batteryPoint = zoom({ pn2Metric: 'battery', topic: 'Battery', payload: 3.62 });
if (!Array.isArray(batteryPoint[1]) || batteryPoint[1][1].topic !== 'Battery') {
  c.fail('a battery point did not land on the battery chart');
}
// The default window is 6 h; one step shorter is 3 h, and only the pressure
// output carries the new label.
const control = zoom({ payload: { kind: 'zoom', direction: 1, metric: 'pressure' } });
if (!control[2] || control[2].payload.label !== '3 h') {
  c.fail('the zoom control did not move the pressure window to 3 h: ' +
    JSON.stringify(control[2] && control[2].payload));
}
if (control[3] !== null) c.fail('a pressure zoom also relabelled the battery chart');
c.done('valve_2: each chart keeps its own zoom window');

c.finish();
