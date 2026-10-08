"""Generate examples/WaterLevel/include/water_level_dashboard_flow.json.

Telemetry-only Node-RED Dashboard 2.0 flow for the WaterLevel Class A node.
Follows the valve1 (MainValve) flow structure: one ChirpStack event input, one
filter/decode function, one ui-template card. Water depth and pressure history
are added as two ui-chart nodes in their own group. No downlink path is
generated because the WaterLevel firmware deliberately ignores application
downlinks.

The ChirpStack application ID and DevEUI are read from the same Node-RED
environment variables the IntegratedDashboard uses, `IRRIGATION_APP_ID` and
`WATER_DEV_EUI`, so one settings.js block configures both flows. The MQTT input
therefore uses dynamic subscription, driven by the subscribe function.

Run after changing the decode rules or card markup:

    python tools/build_water_level_dashboard.py
    node tools/test_water_level_dashboard.js
"""

import json
import pathlib

from water_level_interval import FLOOR_SECONDS, MULTIPLIER, stale_after_seconds

# The tab id is NOT builder-minted, for the same reason UI_GROUP/UI_PAGE are not:
# it must name the "Water Level Node" tab the workspace already owns. Ship a made-up
# id here and every import spawns a second, identical tab beside the real one.
TAB = "e618dcf23e3fb563"    # workspace tab "Water Level Node"
FLOW_GROUP = "sc_wl_group_flow"
# These three are NOT builder-minted ids. They must match the page and groups
# the workspace already owns, because Node-RED reuses a config node whose id it
# recognises and creates a second one otherwise. Ship fresh ids here and every
# import adds another "Water Level" page and another pair of groups to delete by
# hand. Same mechanism that keeps ui-base and ui-theme singletons below.
UI_GROUP = "0dcf231c222554fc"    # workspace group "Water Level"
GRAPH_GROUP = "18fb832a66d8775e"  # workspace group "Water Level history"
UI_PAGE = "334b707b21a5ce0a"     # workspace page "Water Level"
UI_BASE = "f53e93e9ba219e63"   # shared "My Dashboard" base
UI_THEME = "e49416861823a329"  # shared "MyTheme"
BROKER = "ae0178f3742ff530"    # shared "chirpstack_mosquito", as MainValve uses

# Shared with examples/IntegratedDashboard/irrigation_dashboard_flow.json.
APP_ENV = "IRRIGATION_APP_ID"
EUI_ENV = "WATER_DEV_EUI"

SUBSCRIBE = """// Subscribe to the WaterLevel node's ChirpStack events. The application ID and
// DevEUI come from the same environment variables the IntegratedDashboard uses,
// so one settings.js block configures both flows.
const app = String(env.get('""" + APP_ENV + """') || '').toLowerCase();
const eui = String(env.get('""" + EUI_ENV + """') || '').toLowerCase();
const state = Object.assign({}, flow.get('water_state') || {});
const snapshot = {payload:{kind:'state', state:state}};
if (!/^[a-f0-9-]{36}$/.test(app) || !/^[a-f0-9]{16}$/.test(eui)) {
  node.status({fill:'red',shape:'ring',text:'set """ + APP_ENV + """ and """ + EUI_ENV + """'});
  state.config_error = 'Set """ + APP_ENV + """ and """ + EUI_ENV + """ in the Node-RED environment, then redeploy.';
  flow.set('water_state', state);
  return [null, {payload:{kind:'state', state:state}}];
}
const topic = 'application/' + app + '/device/' + eui + '/event/+';
state.config_error = '';
flow.set('water_state', state);
// No context guard: it would survive a redeploy, match, and leave the mqtt in
// node unsubscribed. The once-per-deploy inject already handles idempotence.
node.status({fill:'green',shape:'dot',text:topic});
return [{action:'subscribe', topic:topic, qos:0}, {payload:{kind:'state', state:state}}];"""

DECODE = """// WaterLevel Class A reservoir node, telemetry only.
// FPort 40, protocol v1, ten bytes. The firmware deliberately ignores
// application downlinks, so this flow never publishes one.
// Outputs: 0 state card, 1 water depth chart, 2 pressure chart.
const app = String(env.get('""" + APP_ENV + """') || '').toLowerCase();
const eui = String(env.get('""" + EUI_ENV + """') || '').toLowerCase();
const parts = String(msg.topic || '').toLowerCase().split('/');
if (parts.length !== 6 || parts[0] !== 'application' || parts[2] !== 'device' ||
    parts[4] !== 'event' || parts[1] !== app || parts[3] !== eui) return null;
let event = msg.payload;
try {
  if (Buffer.isBuffer(event)) event = JSON.parse(event.toString('utf8'));
  else if (typeof event === 'string') event = JSON.parse(event);
} catch (err) { node.warn('Invalid ChirpStack JSON: ' + err.message); return null; }
if (!event || typeof event !== 'object') return null;
const kind = parts[5];
const num = v => typeof v === 'number' && Number.isFinite(v) ? v : null;
const arrivedAt = Date.now();
const s = Object.assign({}, flow.get('water_state') || {});
s.last_event = kind;
s.last_event_time = new Date().toISOString();
const card = () => [{payload:{kind:'state', state:s}}, null, null];
if (kind === 'up') {
  if (event.fPort !== 40) return null;
  let d = event.object;
  let fromRaw = false;
  if (!d || typeof d !== 'object' ||
      !Object.prototype.hasOwnProperty.call(d, 'pressure_valid')) {
    // The ChirpStack codec is not installed or returned an error; decode the
    // documented ten-byte v1 payload instead.
    fromRaw = true;
    let b = null;
    try { b = Buffer.from(event.data || '', 'base64'); } catch (_) { b = null; }
    if (!b || b.length !== 10 || b[0] !== 1) {
      s.decode_error = 'FPort 40 needs the WaterLevel codec object or a ten-byte v1 payload.';
      return card();
    }
    const valid = !!(b[1] & 0x01);
    d = {
      protocol_version: b[0],
      pressure_valid: valid,
      load_on: !!(b[1] & 0x02),
      battery_voltage_v: b.readUInt16BE(2) / 1000,
      pressure_bar: valid ? b.readInt16BE(4) / 1000 : null,
      depth_m: valid ? b.readUInt16BE(6) / 1000 : null,
      water_level_percent: valid ? b.readUInt16BE(8) / 10 : null
    };
  }
  s.decode_error = '';
  s.config_error = '';
  // Surface which path decoded this uplink: "codec" is normal, anything else
  // means the ChirpStack device profile is not decoding for us.
  s.decoded_from = fromRaw ? 'raw payload' : 'codec';
  s.protocol_version = num(d.protocol_version);
  s.pressure_valid = d.pressure_valid === true;
  s.load_on = d.load_on === true;
  s.battery_voltage_v = num(d.battery_voltage_v);
  // Depth, level and pressure are only meaningful while the sensor reports valid.
  s.pressure_bar = s.pressure_valid ? num(d.pressure_bar) : null;
  s.depth_m = s.pressure_valid ? num(d.depth_m) : null;
  s.water_level_percent = s.pressure_valid ? num(d.water_level_percent) : null;
  s.depth_cm = s.depth_m === null ? null : s.depth_m * 100;
  s.last_seen = event.time || new Date(arrivedAt).toISOString();
  // Only the telemetry uplink counts as a report. A join, log or status event
  // proves the radio is up but is not the reading this page waits for.
  s.last_seen_ms = arrivedAt;
  s.stale = false;
  s.f_port = event.fPort;
  s.f_cnt = event.fCnt == null ? null : event.fCnt;
  s.uplink_count = (s.uplink_count || 0) + 1;
  const rx = Array.isArray(event.rxInfo) ? event.rxInfo : [];
  if (rx.length) {
    const best = rx.reduce((a, b) => Number(b.snr) > Number(a.snr) ? b : a);
    s.rssi = num(best.rssi);
    s.snr = num(best.snr);
    s.gateway_id = best.gatewayId || '';
  }
  const lora = (((event.txInfo || {}).modulation || {}).lora || {});
  s.sf = num(lora.spreadingFactor);
  s.dr = num(event.dr);
  s.frequency_hz = num((event.txInfo || {}).frequency);
  node.status({fill: s.pressure_valid ? 'green' : 'yellow', shape:'dot',
    text: s.pressure_valid ? 'level ' + s.water_level_percent + ' %' : 'level sensor invalid'});
  flow.set('water_state', s);
  // Only a valid reading is charted. A null point would plot a gap the sensor
  // never reported, so gaps stay gaps. waterMetric routes the point to the
  // right chart through the chart time zoom function.
  const point = (metric, series, value) =>
    value === null ? null : {waterMetric:metric, topic:series, payload:value};
  return [
    {payload:{kind:'state', state:s}},
    point('depth', 'Water depth', s.depth_cm),
    point('pressure', 'Pressure', s.pressure_bar)
  ];
} else if (kind === 'join') { s.network_join = s.last_event_time; }
else if (kind === 'txack') { s.network_txack = 'Gateway transmission acknowledged'; }
else if (kind === 'ack') { s.network_ack = event.acknowledged === true ? 'Confirmed uplink ACK' : 'Uplink not acknowledged'; }
else if (kind === 'log') { s.network_log = event.description || event.code || 'Device log'; }
else if (kind === 'status') { s.network_status = event.battery == null ? 'Device status received' : 'Network battery ' + event.battery; }
else return null;
flow.set('water_state', s);
return card();"""

# Per-chart time window control, following the SoilNode flow. + shows a shorter
# span, - a longer one, each chart independently.
ZOOM_FN = """// Per-chart time window. + shows a shorter span; - shows a longer span.
const windows = [
  {label:'30 min',ms:1800000}, {label:'1 h',ms:3600000},
  {label:'3 h',ms:10800000}, {label:'6 h',ms:21600000},
  {label:'12 h',ms:43200000}, {label:'24 h',ms:86400000}
];
const metrics = {depth:0, pressure:1};
const metric = msg.waterMetric || (msg.payload && msg.payload.metric);
if (!Object.prototype.hasOwnProperty.call(metrics, metric)) return null;
const slot = metrics[metric];
const key = 'water_zoom_' + metric;
let index = Number(flow.get(key));
if (!Number.isInteger(index) || index < 0 || index >= windows.length) index = 3;
const control = msg.payload && msg.payload.kind === 'zoom';
if (control) {
  if (msg.payload.direction !== 1 && msg.payload.direction !== -1) return null;
  index = Math.max(0, Math.min(windows.length - 1, index - msg.payload.direction));
  flow.set(key, index);
} else if (typeof msg.payload !== 'number' || !Number.isFinite(msg.payload)) return null;
const now = Date.now();
const axis = {ui_update:{chartOptions:{xAxis:{min:now - windows[index].ms, max:now}}}};
const out = Array(4).fill(null);
out[slot] = control ? axis : [axis, {topic:msg.topic, payload:msg.payload}];
if (control) out[slot + 2] = {payload:{kind:'zoom_state', label:windows[index].label}};
return out;"""

ZOOM_TEMPLATE = """<template>
<div class="wl-zoom">
  <span>View {{ msg && msg.payload && msg.payload.kind === 'zoom_state' ? msg.payload.label : '6 h' }}</span>
  <button type="button" title="Zoom in: shorter time range" aria-label="Zoom in" @click="zoom(1)">+</button>
  <button type="button" title="Zoom out: longer time range" aria-label="Zoom out" @click="zoom(-1)">&minus;</button>
</div>
</template>
<script>
export default {
  methods: {
    zoom(direction) {
      this.send({payload:{kind:"zoom",metric:"$METRIC$",direction}});
    }
  }
}
</script>
<style>
.wl-zoom{height:100%;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:5px;color:#566;font-size:.72rem}
.wl-zoom span{text-align:center;white-space:nowrap}
.wl-zoom button{width:27px;height:25px;padding:0;border:1px solid #b7c4ce;border-radius:5px;background:#f5f8fa;color:#203447;font-size:17px;line-height:1;cursor:pointer}
.wl-zoom button:hover{background:#e3edf4}
</style>"""

# Staleness watchdog, following the PumpControl flow's "Check stale status".
#
# The limit is derived from the firmware's sleep interval -- see
# tools/water_level_interval.py for why a hardcoded number is wrong at both ends
# of this project, and why the rule lives in one shared place.
STALE_FN = (
    "// The node reports once per wake cycle and then deep-sleeps, so age is\n"
    "// measured from when an uplink reached Node-RED, not from the gateway\n"
    "// timestamp. The limit tracks the firmware's sleep interval (%d x it, floor\n"
    "// %d s), so it tightens when the interval is short and never trips on one\n"
    "// lost uplink.\n"
    "const STALE_AFTER_SECONDS = %d;"
    % (MULTIPLIER, FLOOR_SECONDS, stale_after_seconds())
) + """
const s = Object.assign({}, flow.get('water_state') || {});
s.stale_after_seconds = STALE_AFTER_SECONDS;
s.stale = !s.last_seen_ms || Date.now() - s.last_seen_ms > STALE_AFTER_SECONDS * 1000;
flow.set('water_state', s);
msg.payload = {kind:'state', state:s};
return msg;"""

TEMPLATE = """<template>
<div class="wl">
  <div class="wl-head">
    <div><strong>Water Level Node</strong><small>Class A reservoir monitor &middot; FPort 40</small></div>
    <span class="wl-pill" :class="pill.cls">{{ pill.text }}</span>
  </div>
  <div class="wl-stale" v-if="state.stale && state.last_seen">
    No uplink for {{ ageText }} (limit {{ show(state.stale_after_seconds, ' s') }}). Last report {{ local(state.last_seen) }}.
    The node sleeps between reports, so a long gap can be normal &mdash; check the device, its sleep interval and ChirpStack before treating the readings below as current.
  </div>
  <div class="wl-grid" :class="{stale: state.stale}">
    <div><small>Water level</small><b>{{ show(state.water_level_percent, ' %') }}</b></div>
    <div><small>Depth</small><b>{{ show(state.depth_cm, ' cm') }}</b></div>
    <div><small>Pressure</small><b>{{ show(state.pressure_bar, ' bar') }}</b></div>
    <div><small>Battery</small><b>{{ show(state.battery_voltage_v, ' V') }}</b></div>
    <div><small>Load output</small><b>{{ state.load_on === undefined ? '-' : state.load_on ? 'ON' : 'OFF' }}</b></div>
    <div><small>Signal</small><b>{{ signalText }}</b></div>
  </div>
  <div class="wl-alert" v-if="state.pressure_valid === false && !state.stale">Level sensor reported invalid; depth, water level and pressure are not published. The charts show no new points for this uplink.</div>
  <div class="wl-note">Sensor range 0&ndash;5 m. The load output currently follows battery only: ON at {{ batteryMinimum }} V or more. The level threshold is commented out in the firmware.</div>
  <div class="wl-footer">
    <div><b>Last uplink:</b> {{ local(state.last_seen) }} <template v-if="state.last_seen_ms">({{ ageText }} ago)</template> | <b>Freshness limit:</b> {{ show(state.stale_after_seconds, ' s') }} | <b>Uplinks:</b> {{ show(state.uplink_count) }} | <b>FCnt:</b> {{ show(state.f_cnt) }} | <b>DR:</b> {{ show(state.dr) }} | <b>SF:</b> {{ show(state.sf) }}</div>
    <div><b>Gateway:</b> {{ state.gateway_id || '-' }} | <b>Frequency:</b> {{ show(state.frequency_hz, ' Hz') }} | <b>Protocol:</b> {{ show(state.protocol_version) }} | <b>Decoded by:</b> {{ state.decoded_from || '-' }}</div>
    <div><b>Last event:</b> {{ state.last_event || '-' }} &middot; {{ local(state.last_event_time) }}</div>
    <div v-if="state.network_join"><b>Join:</b> {{ local(state.network_join) }}</div>
    <div v-if="state.network_txack">{{ state.network_txack }}</div>
    <div v-if="state.network_ack">{{ state.network_ack }}</div>
    <div v-if="state.network_log"><b>Log:</b> {{ state.network_log }}</div>
    <div v-if="state.network_status">{{ state.network_status }}</div>
  </div>
  <div class="wl-alert" v-if="state.config_error">{{ state.config_error }}</div>
  <div class="wl-alert" v-if="state.decode_error">{{ state.decode_error }}</div>
</div>
</template>
<script>
export default {
  data() { return {state:{stale:true},batteryMinimum:11.5,now:Date.now(),timer:null}; },
  computed: {
    ageSeconds() {
      if (!this.state.last_seen_ms) return null;
      return Math.max(0, Math.round((this.now - this.state.last_seen_ms) / 1000));
    },
    ageText() {
      const s = this.ageSeconds;
      if (s === null) return '-';
      if (s < 60) return s + ' s';
      const m = Math.floor(s / 60);
      if (m < 60) return m + ' min';
      return Math.floor(m / 60) + ' h ' + (m % 60) + ' min';
    },
    // Link quality is a live property: a reading from before the gap says
    // nothing about the radio now.
    signalText() {
      if (this.state.stale) return 'Unknown';
      return this.show(this.state.rssi, ' dBm') + ' / ' + this.show(this.state.snr, ' dB');
    },
    pill() {
      if (this.state.config_error) return {cls:'bad',text:'Not configured'};
      if (!this.state.last_seen) return {cls:'stale',text:'Waiting for uplink'};
      if (this.state.stale) return {cls:'stale',text:'No recent uplink'};
      if (this.state.pressure_valid !== true) return {cls:'bad',text:'Level sensor invalid'};
      return {cls:'ok',text:'Level ' + this.show(this.state.water_level_percent, ' %')};
    }
  },
  watch: {msg: {handler(m) { if (m && m.payload && m.payload.kind === 'state') this.state = m.payload.state || {}; }, immediate:true}},
  // Re-render every second so the age grows between the watchdog's pushes.
  mounted() { this.timer = setInterval(() => { this.now = Date.now(); }, 1000); },
  beforeUnmount() { if (this.timer) clearInterval(this.timer); },
  methods: {
    show(v,suffix='') { return v === null || v === undefined || v === '' ? '-' : String(v) + suffix; },
    local(iso) { if (!iso) return '-'; const d = new Date(iso); return isNaN(d.getTime()) ? '-' : d.toLocaleString(); }
  }
}
</script>
<style>
.wl{padding:8px}.wl-head{display:flex;justify-content:space-between;align-items:center;gap:12px;font-size:1.15rem}.wl-head small{display:block;color:#667;font-size:.8rem}.wl-pill{padding:6px 12px;border-radius:20px;background:#e5e7eb;font-size:.85rem;white-space:nowrap}.wl-pill.ok{background:#dcfce7;color:#166534}.wl-pill.bad{background:#fecaca;color:#991b1b}.wl-pill.stale{background:#fef3c7;color:#92400e}.wl-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(145px,1fr));gap:10px;margin:16px 0}.wl-grid>div{padding:12px;border:1px solid #ccd7df;border-radius:8px}.wl-grid.stale{opacity:.5}.wl-grid small{display:block;color:#566}.wl-grid b{font-size:1.3rem}.wl-note{border-top:1px solid #ccd7df;padding-top:10px;color:#566;font-size:.82rem}.wl-footer{font-size:.84rem;line-height:1.7;border-top:1px solid #ccd7df;margin-top:12px;padding-top:10px}.wl-alert{background:#b3261e;color:#fff;border-radius:5px;padding:8px;margin:8px 0}.wl-stale{background:#fef3c7;color:#92400e;border:1px solid #fcd34d;border-radius:5px;padding:8px;margin:8px 0;font-size:.85rem}
</style>"""


def zoom(chart_id, name, metric, order, y):
    """The stacked + / - time-window buttons shown to the right of a chart."""
    return {
        "id": chart_id,
        "type": "ui-template",
        "z": TAB,
        "g": FLOW_GROUP,
        "group": GRAPH_GROUP,
        "page": "",
        "ui": "",
        "name": name,
        "order": order,
        "width": "1",
        "height": "4",
        "format": ZOOM_TEMPLATE.replace("$METRIC$", metric),
        "head": "",
        "storeOutMessages": False,
        "passthru": False,
        "resendOnRefresh": True,
        "templateScope": "local",
        "className": "",
        "x": 1560,
        "y": y,
        "wires": [["sc_wl_zoom"]],
    }


def chart(chart_id, name, label, order, x, y):
    """A history chart matching the SoilNode chart configuration."""
    return {
        "id": chart_id,
        "type": "ui-chart",
        "z": TAB,
        "g": FLOW_GROUP,
        "group": GRAPH_GROUP,
        "name": name,
        "label": label,
        "order": order,
        "chartType": "line",
        "category": "topic",
        "categoryType": "msg",
        "xAxisLabel": "",
        "xAxisProperty": "",
        "xAxisPropertyType": "timestamp",
        "xAxisType": "time",
        "xAxisFormat": "",
        "xAxisFormatType": "{HH}:{mm}",
        "xmin": "",
        "xmax": "",
        "yAxisLabel": "",
        "yAxisProperty": "payload",
        "yAxisPropertyType": "msg",
        "ymin": "",
        "ymax": "",
        "bins": "",
        "action": "append",
        "stackSeries": False,
        "pointShape": "circle",
        "pointRadius": "",
        "showLegend": False,
        "removeOlder": "24",
        "removeOlderUnit": "60",
        "removeOlderPoints": "",
        "colors": ["#1f77b4"],
        "textColor": ["#666666"],
        "textColorDefault": True,
        "gridColor": ["#e5e5e5"],
        "gridColorDefault": True,
        "width": "3",
        "height": "4",
        "className": "",
        "interpolation": "linear",
        "x": x,
        "y": y,
        "wires": [[]],
    }


# No "tab" node on purpose, matching MainValve and PumpControl: shipping one makes
# every import create a second, identical tab. The nodes carry z = TAB instead, and
# TAB above names the tab the workspace already has, so an import places them
# inside it.
nodes = [
    {
        "id": FLOW_GROUP,
        "type": "group",
        "z": TAB,
        "style": {
            "stroke": "#999999",
            "stroke-opacity": "1",
            "fill": "none",
            "fill-opacity": "1",
            "label": True,
            "label-position": "nw",
            "color": "#767676",
        },
        "nodes": [
            "sc_wl_inject",
            "sc_wl_subscribe",
            "sc_wl_mqtt_in",
            "sc_wl_decode",
            "sc_wl_ui",
            "sc_wl_stale_inject",
            "sc_wl_stale",
            "sc_wl_zoom",
            "sc_wl_chart_depth",
            "sc_wl_chart_pressure",
            "sc_wl_zoom_depth",
            "sc_wl_zoom_pressure",
        ],
        "x": 44,
        "y": 39,
        "w": 1652,
        "h": 482,
    },
    {
        "id": "sc_wl_inject",
        "type": "inject",
        "z": TAB,
        "g": FLOW_GROUP,
        "name": "Refresh subscription",
        "props": [{"p": "payload"}],
        # Once per deploy only. A repeat here would re-subscribe on a timer for
        # no gain: the card is already refreshed every 15 s by sc_wl_stale, which
        # re-emits the same {kind:"state"} payload, and the convention test forbids
        # a repeating subscribe inject.
        "repeat": "",
        "crontab": "",
        "once": True,
        "onceDelay": "1",
        "topic": "",
        "payload": "",
        "payloadType": "date",
        "x": 170,
        "y": 160,
        "wires": [["sc_wl_subscribe"]],
    },
    {
        "id": "sc_wl_subscribe",
        "type": "function",
        "z": TAB,
        "g": FLOW_GROUP,
        "name": "Subscribe to configured ChirpStack application",
        "func": SUBSCRIBE,
        "outputs": 2,
        "timeout": 0,
        "noerr": 0,
        "initialize": "",
        "finalize": "",
        "libs": [],
        "x": 450,
        "y": 160,
        "wires": [["sc_wl_mqtt_in"], ["sc_wl_ui"]],
    },
    {
        "id": "sc_wl_mqtt_in",
        "type": "mqtt in",
        "z": TAB,
        "g": FLOW_GROUP,
        "name": "ChirpStack events",
        "topic": "",
        "qos": "0",
        "datatype": "auto-detect",
        # Share the workspace's existing ChirpStack broker rather than shipping
        # another config node, which would land as a duplicate server on import.
        "broker": BROKER,
        "nl": False,
        "rap": True,
        "rh": 0,
        "inputs": 1,
        "x": 760,
        "y": 100,
        "wires": [["sc_wl_decode"]],
    },
    {
        "id": "sc_wl_decode",
        "type": "function",
        "z": TAB,
        "g": FLOW_GROUP,
        "name": "Filter and decode WaterLevel events",
        "func": DECODE,
        "outputs": 3,
        "timeout": 0,
        "noerr": 0,
        "initialize": "",
        "finalize": "",
        "libs": [],
        "x": 1000,
        "y": 180,
        "wires": [["sc_wl_ui"], ["sc_wl_zoom"], ["sc_wl_zoom"]],
    },
    {
        "id": "sc_wl_stale_inject",
        "type": "inject",
        "z": TAB,
        "g": FLOW_GROUP,
        "name": "Refresh and check staleness",
        "props": [{"p": "payload"}],
        "repeat": "15",
        "crontab": "",
        "once": True,
        "onceDelay": "2",
        "topic": "",
        "payload": "",
        "payloadType": "date",
        "x": 190,
        "y": 300,
        "wires": [["sc_wl_stale"]],
    },
    {
        "id": "sc_wl_stale",
        "type": "function",
        "z": TAB,
        "g": FLOW_GROUP,
        "name": "Check stale status",
        "func": STALE_FN,
        "outputs": 1,
        "timeout": 0,
        "noerr": 0,
        "initialize": "",
        "finalize": "",
        "libs": [],
        "x": 430,
        "y": 300,
        "wires": [["sc_wl_ui"]],
    },
    {
        "id": "sc_wl_zoom",
        "type": "function",
        "z": TAB,
        "g": FLOW_GROUP,
        "name": "WaterLevel chart time zoom",
        "func": ZOOM_FN,
        "outputs": 4,
        "timeout": 0,
        "noerr": 0,
        "initialize": "",
        "finalize": "",
        "libs": [],
        "x": 1250,
        "y": 420,
        "wires": [
            ["sc_wl_chart_depth"],
            ["sc_wl_chart_pressure"],
            ["sc_wl_zoom_depth"],
            ["sc_wl_zoom_pressure"],
        ],
    },
    {
        "id": "sc_wl_ui",
        "type": "ui-template",
        "z": TAB,
        "g": FLOW_GROUP,
        "group": UI_GROUP,
        "page": "",
        "ui": "",
        "name": "WaterLevel telemetry",
        "order": 1,
        "width": "8",
        "height": "9",
        "format": TEMPLATE,
        "head": "",
        "storeOutMessages": False,
        "passthru": False,
        "resendOnRefresh": True,
        "templateScope": "local",
        "className": "",
        "x": 1250,
        "y": 180,
        "wires": [[]],
    },
    chart("sc_wl_chart_depth", "Water depth history", "Water depth (cm)", 1, 1250, 340),
    zoom("sc_wl_zoom_depth", "DEPTH chart zoom", "depth", 2, 340),
    chart("sc_wl_chart_pressure", "Pressure history", "Pressure (bar)", 3, 1250, 500),
    zoom("sc_wl_zoom_pressure", "PRESSURE chart zoom", "pressure", 4, 500),
    {
        "id": UI_GROUP,
        "type": "ui-group",
        "z": TAB,
        "name": "Water Level",
        "page": UI_PAGE,
        "width": "8",
        "height": 8,
        "order": 1,
        "showTitle": True,
        "className": "",
        "visible": "true",
        "disabled": "false",
        "groupType": "default",
    },
    {
        "id": GRAPH_GROUP,
        "type": "ui-group",
        "z": TAB,
        "name": "Water Level history",
        "page": UI_PAGE,
        "width": "4",
        "height": 8,
        "order": 2,
        "showTitle": True,
        "className": "",
        "visible": "true",
        "disabled": "false",
        "groupType": "default",
    },
    {
        "id": UI_PAGE,
        "type": "ui-page",
        "z": TAB,
        "name": "Water Level",
        "ui": UI_BASE,
        "path": "/water-level",
        "icon": "mdi-water-percent",
        "layout": "grid",
        "theme": UI_THEME,
        "breakpoints": [
            {"name": "Default", "px": "0", "cols": "3"},
            {"name": "Tablet", "px": "576", "cols": "6"},
            {"name": "Small Desktop", "px": "768", "cols": "9"},
            {"name": "Desktop", "px": "1024", "cols": "12"},
        ],
        "order": 2,
        "className": "",
        "visible": "true",
        "disabled": "false",
    },
    {
        "id": UI_BASE,
        "type": "ui-base",
        "name": "My Dashboard",
        "path": "/dashboard",
        "appIcon": "",
        "includeClientData": True,
        "acceptsClientConfig": ["ui-notification", "ui-control"],
        "showPathInSidebar": False,
        "headerContent": "page",
        "navigationStyle": "fixed",
        "titleBarStyle": "default",
        "showReconnectNotification": True,
        "notificationDisplayTime": 1,
        "showDisconnectNotification": True,
        "allowInstall": True,
    },
    {
        "id": UI_THEME,
        "type": "ui-theme",
        "name": "MyTheme",
        "colors": {
            "surface": "#ffffff",
            "primary": "#0094ce",
            "bgPage": "#eeeeee",
            "groupBg": "#ffffff",
            "groupOutline": "#cccccc",
        },
        "sizes": {
            "density": "default",
            "pagePadding": "12px",
            "groupGap": "12px",
            "groupBorderRadius": "4px",
            "widgetGap": "12px",
        },
    },
    {
        "id": "sc_wl_global_config",
        "type": "global-config",
        "env": [],
        "modules": {"@flowfuse/node-red-dashboard": "1.30.2"},
    },
]

out = (
    pathlib.Path(__file__).resolve().parent.parent
    / "examples"
    / "WaterLevel"
    / "include"
    / "water_level_dashboard_flow.json"
)
out.write_text(json.dumps(nodes, indent=4) + "\n", encoding="utf-8")
print("wrote", out, len(nodes), "nodes")
