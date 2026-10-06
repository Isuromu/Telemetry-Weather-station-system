"""The SoilNode dashboard: one compact status card, the metric history charts and the sleep-interval control.

The imported JSON, examples/SoilNode/include/soil_node_dashboard_compact_flow.json, is a build artifact -- change this
builder and re-run it rather than editing that file:

    python tools/build_soil_node_dashboard.py

The flow carries no ChirpStack identifier: it subscribes dynamically and reads
IRRIGATION_APP_ID and SOIL_DEV_EUI from the Node-RED environment. write_flow refuses a flow that
breaks that rule, tools/test_flow_ids.js checks it across every flow, and
tools/test_soil_node_dashboard.js covers this flow's import
invariants and decoded output.
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from flow_common import write_flow  # noqa: E402

OUTPUT = "examples/SoilNode/include/soil_node_dashboard_compact_flow.json"
DEVICE_ENV = "SOIL_DEV_EUI"

NODES = [{'id': '6e97c77aaa496d62',
  'type': 'group',
  'z': 'soil_dashboard_tab',
  'style': {'stroke': '#999999',
            'stroke-opacity': '1',
            'fill': 'none',
            'fill-opacity': '1',
            'label': True,
            'label-position': 'nw',
            'color': '#767676'},
  'nodes': ['dd28f5e8f447dd88',
            '0bd20843acf477b3',
            'f87d4c5c13817987',
            'f5cfea6505836f56',
            '6e088ee63985651d',
            '5fbd5cbbff7bdeb2',
            'fbff2aaecefad608',
            '96da356ad3e51a8f',
            '19db8cbead5deb76',
            '9dff79b98691a3cc',
            '9b09b458f6a92cb2',
            'd91b8a1b25c2c954',
            '8956e58ce9c79806',
            'a15396ed4d992509',
            'dbb636acc2ac9826',
            'e64987630f834543'],
  'x': 14,
  'y': 19,
  'w': 1812,
  'h': 432},
 {'id': 'dd28f5e8f447dd88',
  'type': 'inject',
  'z': 'soil_dashboard_tab',
  'g': '6e97c77aaa496d62',
  'name': 'Refresh SoilNode subscription',
  'props': [{'p': 'payload'}],
  'repeat': '',
  'crontab': '',
  'once': True,
  'onceDelay': '0.5',
  'topic': '',
  'payload': '',
  'payloadType': 'date',
  'x': 190,
  'y': 60,
  'wires': [['0bd20843acf477b3']]},
 {'id': '0bd20843acf477b3',
  'type': 'function',
  'z': 'soil_dashboard_tab',
  'g': '6e97c77aaa496d62',
  'name': 'Subscribe SoilNode events',
  'func': '// Subscribe dynamically so the application ID and DevEUI stay out of the flow export.\n'
          "// Set IRRIGATION_APP_ID and SOIL_DEV_EUI in this node's Environment tab,\n"
          '// or once in process env for the Node-RED service.\n'
          'const app = String(env.get("IRRIGATION_APP_ID") || "").toLowerCase();\n'
          'const eui = String(env.get("SOIL_DEV_EUI") || "").toLowerCase();\n'
          'if (!/^[a-f0-9-]{36}$/.test(app) || !/^[a-f0-9]{16}$/.test(eui)) {\n'
          '    node.status({ fill: "red", shape: "ring", text: "IRRIGATION_APP_ID / SOIL_DEV_EUI missing" '
          '});\n'
          '    return null;\n'
          '}\n'
          'const topic = "application/" + app + "/device/" + eui + "/event/+";\n'
          'node.status({ fill: "green", shape: "dot", text: topic });\n'
          'return { action: "subscribe", topic: topic, qos: 0 };',
  'outputs': 1,
  'timeout': 0,
  'noerr': 0,
  'initialize': '',
  'finalize': '',
  'libs': [],
  'x': 190,
  'y': 110,
  'wires': [['f87d4c5c13817987']]},
 {'id': 'f87d4c5c13817987',
  'type': 'mqtt in',
  'z': 'soil_dashboard_tab',
  'g': '6e97c77aaa496d62',
  'name': 'ChirpStack events',
  'topic': '',
  'qos': '0',
  'datatype': 'json',
  'broker': 'ae0178f3742ff530',
  'nl': False,
  'rap': True,
  'rh': 0,
  'inputs': 1,
  'x': 590,
  'y': 110,
  'wires': [['f5cfea6505836f56']]},
 {'id': 'f5cfea6505836f56',
  'type': 'function',
  'z': 'soil_dashboard_tab',
  'g': '6e97c77aaa496d62',
  'name': 'Filter and decode SoilNode',
  'func': 'const app = String(env.get("IRRIGATION_APP_ID") || "").toLowerCase();\n'
          'const eui = String(env.get("SOIL_DEV_EUI") || "").toLowerCase();\n'
          'if (!/^[a-f0-9-]{36}$/.test(app) || !/^[a-f0-9]{16}$/.test(eui)) {\n'
          '  node.status({fill:"red",shape:"ring",text:"set IRRIGATION_APP_ID and SOIL_DEV_EUI"});\n'
          '  return null;\n'
          '}\n'
          'const parts = String(msg.topic || "").toLowerCase().split("/");\n'
          'if (parts.length !== 6 || parts[0] !== "application" || parts[1] !== app ||\n'
          '    parts[2] !== "device" || parts[3] !== eui || parts[4] !== "event") return null;\n'
          'let event = msg.payload;\n'
          'try {\n'
          '  if (Buffer.isBuffer(event)) event = JSON.parse(event.toString("utf8"));\n'
          '  else if (typeof event === "string") event = JSON.parse(event);\n'
          '} catch (err) { node.warn("Invalid ChirpStack JSON: " + err.message); return null; }\n'
          'if (!event || typeof event !== "object") return null;\n'
          'const kind = parts[5];\n'
          'const s = Object.assign({}, flow.get("soil_state") || {});\n'
          's.last_event = kind;\n'
          's.last_event_time = new Date().toISOString();\n'
          'const num = v => typeof v === "number" && Number.isFinite(v) ? v : null;\n'
          'if (kind === "up") {\n'
          '  if (event.fPort === 11) {\n'
          '    let result = event.object;\n'
          '    if (!result || result.message_type !== "command_ack") {\n'
          '      let b;\n'
          '      try { b = Buffer.from(event.data || "", "base64"); } catch (_) { b = null; }\n'
          '      if (!b || b.length !== 8 || b[0] !== 1 || b[3] > 2) {\n'
          '        s.decode_error = "Invalid FPort 11 command result. Check SoilNode codec.";\n'
          '        flow.set("soil_state",s);\n'
          '        return [{payload:{kind:"state",state:s}},null,null,null,null];\n'
          '      }\n'
          '      result = {\n'
          '        command_id:b.readUInt16BE(1) === 65535 ? null : b.readUInt16BE(1),\n'
          '        command_status:["applied","invalid","storage_failed"][b[3]],\n'
          '        active_sleep_seconds:b.readUInt32BE(4)\n'
          '      };\n'
          '    }\n'
          '    s.last_ack_at = event.time || new Date().toISOString();\n'
          '    s.last_ack_id = Number.isInteger(result.command_id) ? result.command_id : null;\n'
          '    s.last_ack_status = result.command_status;\n'
          '    s.active_sleep_seconds = num(result.active_sleep_seconds);\n'
          '    s.decode_error = "";\n'
          '    if (s.pending_command_id != null && s.last_ack_id === s.pending_command_id) {\n'
          '      s.command_state = result.command_status === "applied"\n'
          '        ? "Applied by node; active sleep " + s.active_sleep_seconds + " s."\n'
          '        : "Node rejected command: " + result.command_status + ". Active sleep " +\n'
          '          s.active_sleep_seconds + " s.";\n'
          '      s.pending_command_id = null;\n'
          '    }\n'
          '    flow.set("soil_state",s);\n'
          '    node.status({fill:result.command_status === "applied" ? "green" : "red",\n'
          '      shape:"dot",text:"command result " + result.command_status});\n'
          '    return [{payload:{kind:"state",state:s}},null,null,null,null];\n'
          '  }\n'
          '  if (event.fPort !== 10) return null;\n'
          '  let d = event.object;\n'
          '  if (!d || typeof d !== "object" || typeof d.sensor_valid !== "boolean") {\n'
          '    let b;\n'
          '    try { b = Buffer.from(event.data || "", "base64"); } catch (_) { b = null; }\n'
          '    if (!b || b.length !== 8) {\n'
          '      s.decode_error = "FPort 10 needs decoded object or eight raw bytes. Check SoilNode '
          'codec.";\n'
          '      flow.set("soil_state", s);\n'
          '      return [{payload:{kind:"state",state:s}},null,null,null,null];\n'
          '    }\n'
          '    const valid = !!(b[0] & 1);\n'
          '    d = {\n'
          '      sensor_valid: valid,\n'
          '      temperature_c: valid ? b.readInt16BE(1) / 100 : null,\n'
          '      vwc_percent: valid ? b.readUInt16BE(3) / 100 : null,\n'
          '      ec_ms_cm: valid ? b.readUInt16BE(5) / 1000 : null,\n'
          '      battery_voltage_v: 2 + b[7] / 100\n'
          '    };\n'
          '  }\n'
          '  s.decode_error = "";\n'
          '  s.sensor_valid = d.sensor_valid === true;\n'
          '  s.temperature_c = s.sensor_valid ? num(d.temperature_c) : null;\n'
          '  s.vwc_percent = s.sensor_valid ? num(d.vwc_percent) : null;\n'
          '  s.ec_ms_cm = s.sensor_valid ? num(d.ec_ms_cm) : null;\n'
          '  s.battery_voltage_v = num(d.battery_voltage_v);\n'
          '  s.last_seen = event.time || new Date().toISOString();\n'
          '  s.f_cnt = num(event.fCnt);\n'
          '  s.uplink_count = (s.uplink_count || 0) + 1;\n'
          '  const rx = Array.isArray(event.rxInfo) ? event.rxInfo : [];\n'
          '  if (rx.length) {\n'
          '    const best = rx.reduce((a,b) => Number(b.snr) > Number(a.snr) ? b : a);\n'
          '    s.rssi = num(best.rssi);\n'
          '    s.snr = num(best.snr);\n'
          '    s.gateway_id = best.gatewayId || "";\n'
          '  }\n'
          '  const lora = (((event.txInfo || {}).modulation || {}).lora || {});\n'
          '  s.sf = num(lora.spreadingFactor);\n'
          '  s.dr = num(event.dr);\n'
          '  s.frequency_hz = num((event.txInfo || {}).frequency);\n'
          '  if (s.command_queued_at && !s.first_uplink_after_queue &&\n'
          '      Date.parse(s.last_seen) > Date.parse(s.command_queued_at)) {\n'
          '    s.first_uplink_after_queue = s.last_seen;\n'
          '    s.command_state = "Later uplink received; interval application still unconfirmed.";\n'
          '  }\n'
          '  node.status({fill:s.sensor_valid ? "green" : "yellow",shape:"dot",\n'
          '    text:s.sensor_valid ? "soil reading valid" : "soil sensor invalid"});\n'
          '} else if (kind === "join") {\n'
          '  s.last_join = s.last_event_time;\n'
          '} else if (kind === "txack") {\n'
          '  s.last_txack = s.last_event_time;\n'
          '} else if (kind === "ack") {\n'
          '  s.last_network_ack = event.acknowledged === true ? "Network ACK received" : "Network ACK '
          'absent";\n'
          '} else if (kind === "log") {\n'
          '  s.last_log = event.description || event.code || "Device log";\n'
          '} else if (kind === "status") {\n'
          '  s.last_network_status = event.battery == null ? "Device status received" : "Network battery '
          'level " + event.battery;\n'
          '} else return null;\n'
          'flow.set("soil_state", s);\n'
          'const point = (metric,topic,value) => value === null ? null : '
          '{soilMetric:metric,topic:topic,payload:value};\n'
          'return [\n'
          '  {payload:{kind:"state",state:s}},\n'
          '  kind === "up" && s.sensor_valid ? point("temp","Temperature",s.temperature_c) : null,\n'
          '  kind === "up" && s.sensor_valid ? point("vwc","Water content",s.vwc_percent) : null,\n'
          '  kind === "up" && s.sensor_valid ? point("ec","Conductivity",s.ec_ms_cm) : null,\n'
          '  kind === "up" ? point("battery","Battery",s.battery_voltage_v) : null\n'
          '];',
  'outputs': 5,
  'timeout': 0,
  'noerr': 0,
  'initialize': '',
  'finalize': '',
  'libs': [],
  'x': 840,
  'y': 110,
  'wires': [['9b09b458f6a92cb2'],
            ['d91b8a1b25c2c954'],
            ['d91b8a1b25c2c954'],
            ['d91b8a1b25c2c954'],
            ['d91b8a1b25c2c954']]},
 {'id': '6e088ee63985651d',
  'type': 'function',
  'z': 'soil_dashboard_tab',
  'g': '6e97c77aaa496d62',
  'name': 'Validate and queue interval',
  'func': 'const action = msg.payload || {};\n'
          'const s = Object.assign({}, flow.get("soil_state") || {});\n'
          'const snapshot = () => ({payload:{kind:"state",state:s}});\n'
          'if (action.kind === "refresh") return [null,snapshot()];\n'
          'if (action.kind !== "command") return null;\n'
          'const app = String(env.get("IRRIGATION_APP_ID") || "").toLowerCase();\n'
          'const eui = String(env.get("SOIL_DEV_EUI") || "").toLowerCase();\n'
          'if (!/^[a-f0-9-]{36}$/.test(app) || !/^[a-f0-9]{16}$/.test(eui)) {\n'
          '  s.ui_error = "Set IRRIGATION_APP_ID and SOIL_DEV_EUI in Node-RED environment.";\n'
          '  flow.set("soil_state",s);\n'
          '  return [null,snapshot()];\n'
          '}\n'
          'const seconds = action.sleep_seconds;\n'
          'if (typeof seconds !== "number" || !Number.isInteger(seconds) ||\n'
          '    seconds < 10 || seconds > 86400) {\n'
          '  s.ui_error = "Sleep interval must be a whole number from 10 to 86400 seconds.";\n'
          '  flow.set("soil_state",s);\n'
          '  return [null,snapshot()];\n'
          '}\n'
          'let id = flow.get("soil_next_command_id");\n'
          'if (!Number.isInteger(id) || id < 1 || id > 65534) id = 1;\n'
          'flow.set("soil_next_command_id",id === 65534 ? 1 : id + 1);\n'
          's.pending_command_id = id;\n'
          's.ui_error = "";\n'
          's.command_queued_at = new Date().toISOString();\n'
          's.command_requested_seconds = seconds;\n'
          's.first_uplink_after_queue = null;\n'
          's.command_state = "Downlink submitted to ChirpStack; waiting for next Class A receive window.";\n'
          'flow.set("soil_state",s);\n'
          'node.status({fill:"yellow",shape:"dot",text:"queued " + seconds + " s"});\n'
          'return [{\n'
          '  topic:"application/" + app + "/device/" + eui + "/command/down",\n'
          '  '
          'payload:JSON.stringify({devEui:eui,confirmed:false,fPort:10,object:{sleep_seconds:seconds,command_id:id}}),\n'
          '  qos:"0",retain:false\n'
          '},snapshot()];',
  'outputs': 2,
  'timeout': 0,
  'noerr': 0,
  'initialize': '',
  'finalize': '',
  'libs': [],
  'x': 1440,
  'y': 70,
  'wires': [['5fbd5cbbff7bdeb2'], ['9b09b458f6a92cb2']]},
 {'id': '5fbd5cbbff7bdeb2',
  'type': 'mqtt out',
  'z': 'soil_dashboard_tab',
  'g': '6e97c77aaa496d62',
  'name': 'ChirpStack downlink',
  'topic': '',
  'qos': '0',
  'retain': '',
  'respTopic': '',
  'contentType': '',
  'userProps': '',
  'correl': '',
  'expiry': '',
  'broker': 'ae0178f3742ff530',
  'x': 1700,
  'y': 70,
  'wires': []},
 {'id': 'fbff2aaecefad608',
  'type': 'ui-chart',
  'z': 'soil_dashboard_tab',
  'g': '6e97c77aaa496d62',
  'group': 'sc_soil_graph_group',
  'name': 'Temperature history',
  'label': 'Temperature (°C)',
  'order': 1,
  'chartType': 'line',
  'category': 'topic',
  'categoryType': 'msg',
  'xAxisLabel': '',
  'xAxisProperty': '',
  'xAxisPropertyType': 'timestamp',
  'xAxisType': 'time',
  'xAxisFormat': '',
  'xAxisFormatType': '{HH}:{mm}',
  'xmin': '',
  'xmax': '',
  'yAxisLabel': '',
  'yAxisProperty': 'payload',
  'yAxisPropertyType': 'msg',
  'ymin': '',
  'ymax': '',
  'bins': '',
  'action': 'append',
  'stackSeries': False,
  'pointShape': 'circle',
  'pointRadius': '',
  'showLegend': False,
  'removeOlder': '24',
  'removeOlderUnit': '60',
  'removeOlderPoints': '',
  'colors': ['#1f77b4'],
  'textColor': ['#666666'],
  'textColorDefault': True,
  'gridColor': ['#e5e5e5'],
  'gridColorDefault': True,
  'width': '3',
  'height': '4',
  'className': '',
  'interpolation': 'linear',
  'x': 1340,
  'y': 230,
  'wires': [[]]},
 {'id': '96da356ad3e51a8f',
  'type': 'ui-chart',
  'z': 'soil_dashboard_tab',
  'g': '6e97c77aaa496d62',
  'group': 'sc_soil_graph_group',
  'name': 'Water content history',
  'label': 'Water content (% VWC)',
  'order': 3,
  'chartType': 'line',
  'category': 'topic',
  'categoryType': 'msg',
  'xAxisLabel': '',
  'xAxisProperty': '',
  'xAxisPropertyType': 'timestamp',
  'xAxisType': 'time',
  'xAxisFormat': '',
  'xAxisFormatType': '{HH}:{mm}',
  'xmin': '',
  'xmax': '',
  'yAxisLabel': '',
  'yAxisProperty': 'payload',
  'yAxisPropertyType': 'msg',
  'ymin': '',
  'ymax': '',
  'bins': '',
  'action': 'append',
  'stackSeries': False,
  'pointShape': 'circle',
  'pointRadius': '',
  'showLegend': False,
  'removeOlder': '24',
  'removeOlderUnit': '60',
  'removeOlderPoints': '',
  'colors': ['#1f77b4'],
  'textColor': ['#666666'],
  'textColorDefault': True,
  'gridColor': ['#e5e5e5'],
  'gridColorDefault': True,
  'width': '3',
  'height': '4',
  'className': '',
  'interpolation': 'linear',
  'x': 1340,
  'y': 290,
  'wires': [[]]},
 {'id': '19db8cbead5deb76',
  'type': 'ui-chart',
  'z': 'soil_dashboard_tab',
  'g': '6e97c77aaa496d62',
  'group': 'sc_soil_graph_group',
  'name': 'Conductivity history',
  'label': 'Conductivity (mS/cm)',
  'order': 5,
  'chartType': 'line',
  'category': 'topic',
  'categoryType': 'msg',
  'xAxisLabel': '',
  'xAxisProperty': '',
  'xAxisPropertyType': 'timestamp',
  'xAxisType': 'time',
  'xAxisFormat': '',
  'xAxisFormatType': '{HH}:{mm}',
  'xmin': '',
  'xmax': '',
  'yAxisLabel': '',
  'yAxisProperty': 'payload',
  'yAxisPropertyType': 'msg',
  'ymin': '',
  'ymax': '',
  'bins': '',
  'action': 'append',
  'stackSeries': False,
  'pointShape': 'circle',
  'pointRadius': '',
  'showLegend': False,
  'removeOlder': '24',
  'removeOlderUnit': '60',
  'removeOlderPoints': '',
  'colors': ['#1f77b4'],
  'textColor': ['#666666'],
  'textColorDefault': True,
  'gridColor': ['#e5e5e5'],
  'gridColorDefault': True,
  'width': '3',
  'height': '4',
  'className': '',
  'interpolation': 'linear',
  'x': 1340,
  'y': 350,
  'wires': [[]]},
 {'id': '9dff79b98691a3cc',
  'type': 'ui-chart',
  'z': 'soil_dashboard_tab',
  'g': '6e97c77aaa496d62',
  'group': 'sc_soil_graph_group',
  'name': 'Battery history',
  'label': 'Battery (V)',
  'order': 7,
  'chartType': 'line',
  'category': 'topic',
  'categoryType': 'msg',
  'xAxisLabel': '',
  'xAxisProperty': '',
  'xAxisPropertyType': 'timestamp',
  'xAxisType': 'time',
  'xAxisFormat': '',
  'xAxisFormatType': '{HH}:{mm}',
  'xmin': '',
  'xmax': '',
  'yAxisLabel': '',
  'yAxisProperty': 'payload',
  'yAxisPropertyType': 'msg',
  'ymin': '',
  'ymax': '',
  'bins': '',
  'action': 'append',
  'stackSeries': False,
  'pointShape': 'circle',
  'pointRadius': '',
  'showLegend': False,
  'removeOlder': '24',
  'removeOlderUnit': '60',
  'removeOlderPoints': '',
  'colors': ['#1f77b4'],
  'textColor': ['#666666'],
  'textColorDefault': True,
  'gridColor': ['#e5e5e5'],
  'gridColorDefault': True,
  'width': '3',
  'height': '4',
  'className': '',
  'interpolation': 'linear',
  'x': 1340,
  'y': 410,
  'wires': [[]]},
 {'id': '9b09b458f6a92cb2',
  'type': 'ui-template',
  'z': 'soil_dashboard_tab',
  'g': '6e97c77aaa496d62',
  'group': 'sc_soil_ui_group',
  'page': '',
  'ui': '',
  'name': 'SoilNode compact HTML',
  'order': 1,
  'width': '8',
  'height': '11',
  'head': '',
  'format': '<template>\n'
            '<div class="sc-soil">\n'
            '  <div class="sc-head">\n'
            '    <div><strong>SoilNode</strong><small>Class A soil monitor</small></div>\n'
            '    <span class="sc-pill" :class="state.sensor_valid ? \'ok\' : \'bad\'">{{ state.last_seen ? '
            "(state.sensor_valid ? 'Sensor valid' : 'Sensor read failed') : 'Waiting for uplink' }}</span>\n"
            '  </div>\n'
            '  <div class="sc-grid">\n'
            "    <div><small>Temperature</small><b>{{ show(state.temperature_c, ' °C') }}</b></div>\n"
            "    <div><small>Water content</small><b>{{ show(state.vwc_percent, ' % VWC') }}</b></div>\n"
            "    <div><small>Conductivity</small><b>{{ show(state.ec_ms_cm, ' mS/cm') }}</b></div>\n"
            "    <div><small>Battery</small><b>{{ show(state.battery_voltage_v, ' V') }}</b></div>\n"
            '    <div><small>Last uplink</small><b>{{ local(state.last_seen) }}</b></div>\n'
            "    <div><small>Signal</small><b>{{ show(state.rssi, ' dBm') }} / {{ show(state.snr, ' dB SNR') "
            '}}</b></div>\n'
            '  </div>\n'
            '  <div class="sc-section">\n'
            '    <h3>Sleep interval</h3>\n'
            '    <div class="sc-controls">\n'
            '      <v-text-field v-model.number="sleepSeconds" type="number" min="10" max="86400" step="1" '
            'label="Seconds (10–86400)" density="compact" hide-details></v-text-field>\n'
            '      <v-btn color="primary" :disabled="!validInterval()" @click="sendSleep">Queue '
            'interval</v-btn>\n'
            '    </div>\n'
            '    <small>Queued for next Class A receive window. Queue one command at a time.</small>\n'
            '  </div>\n'
            '  <div class="sc-section sc-feedback">\n'
            '    <h3>Command feedback</h3>\n'
            "    <div><b>Requested:</b> {{ show(state.command_requested_seconds, ' s') }} · ID {{ "
            'show(state.pending_command_id) }} · {{ local(state.command_queued_at) }}</div>\n'
            "    <div><b>State:</b> {{ state.command_state || 'No command queued' }}</div>\n"
            "    <div><b>Application result:</b> {{ state.last_ack_status || '-' }} · ID {{ "
            "show(state.last_ack_id) }} · active {{ show(state.active_sleep_seconds, ' s') }} · {{ "
            'local(state.last_ack_at) }}</div>\n'
            '    <small>Network txack and network ACK do not confirm application.</small>\n'
            '  </div>\n'
            '  <div class="sc-footer">\n'
            '    <span>Uplinks {{ show(state.uplink_count) }} · FCnt {{ show(state.f_cnt) }}</span>\n'
            "    <span>Gateway {{ state.gateway_id || '-' }}</span>\n"
            "    <span>Last event {{ state.last_event || '-' }} · {{ local(state.last_event_time) }}</span>\n"
            '  </div>\n'
            '  <div class="sc-alert" v-if="state.ui_error">{{ state.ui_error }}</div>\n'
            '  <div class="sc-alert" v-if="state.decode_error">{{ state.decode_error }}</div>\n'
            '</div>\n'
            '</template>\n'
            '<script>\n'
            'export default {\n'
            '  data() { return {state:{},sleepSeconds:600}; },\n'
            "  watch: {msg: {handler(m) { if (m && m.payload && m.payload.kind === 'state') this.state = "
            'm.payload.state || {}; }, immediate:true}},\n'
            '  methods: {\n'
            "    show(v,suffix='') { return v === null || v === undefined || v === '' ? '-' : String(v) + "
            'suffix; },\n'
            "    local(iso) { if (!iso) return '-'; const d = new Date(iso); return isNaN(d.getTime()) ? '-' "
            ': d.toLocaleString(); },\n'
            '    validInterval() { const n=Number(this.sleepSeconds); return Number.isInteger(n) && n>=10 && '
            'n<=86400; },\n'
            '    sendSleep() {\n'
            '      if (!this.validInterval()) return;\n'
            "      this.send({payload:{kind:'command',sleep_seconds:Number(this.sleepSeconds)}});\n"
            '    }\n'
            '  }\n'
            '}\n'
            '</script>\n'
            '<style>\n'
            '.sc-soil{padding:8px}.sc-head{display:flex;justify-content:space-between;align-items:center;gap:12px;font-size:1.15rem}.sc-head '
            'small{display:block;color:#667;font-size:.8rem}.sc-pill{padding:6px '
            '12px;border-radius:20px;background:#e5e7eb;font-size:.85rem}.sc-pill.ok{background:#dcfce7;color:#166534}.sc-pill.bad{background:#fef3c7;color:#92400e}.sc-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(145px,1fr));gap:10px;margin:14px '
            '0}.sc-grid>div{padding:10px;border:1px solid #ccd7df;border-radius:8px}.sc-grid '
            'small{display:block;color:#566}.sc-grid '
            'b{font-size:1.15rem}.sc-section,.sc-footer{border-top:1px solid '
            '#ccd7df;padding-top:10px;margin-top:10px}.sc-section h3{margin:0 0 '
            '9px}.sc-controls{display:flex;align-items:center;gap:10px;max-width:460px}.sc-controls '
            '.v-input{flex:1}.sc-section small,.sc-footer{color:#566;font-size:.82rem}.sc-feedback '
            'div{margin:5px 0}.sc-footer{display:flex;flex-wrap:wrap;gap:8px '
            '18px}.sc-alert{background:#b3261e;color:#fff;border-radius:5px;padding:8px;margin-top:10px}\n'
            '</style>',
  'storeOutMessages': False,
  'passthru': False,
  'resendOnRefresh': True,
  'templateScope': 'local',
  'className': '',
  'x': 1190,
  'y': 70,
  'wires': [['6e088ee63985651d']]},
 {'id': 'd91b8a1b25c2c954',
  'type': 'function',
  'z': 'soil_dashboard_tab',
  'g': '6e97c77aaa496d62',
  'name': 'Soil chart time zoom',
  'func': '// Per-chart time window. + shows a shorter span; - shows a longer span.\n'
          'const windows = [\n'
          '  {label:"30 min",ms:1800000}, {label:"1 h",ms:3600000},\n'
          '  {label:"3 h",ms:10800000}, {label:"6 h",ms:21600000},\n'
          '  {label:"12 h",ms:43200000}, {label:"24 h",ms:86400000}\n'
          '];\n'
          'const metrics = {temp:0,vwc:1,ec:2,battery:3};\n'
          'const metric = msg.soilMetric || (msg.payload && msg.payload.metric);\n'
          'if (!Object.prototype.hasOwnProperty.call(metrics,metric)) return null;\n'
          'const slot = metrics[metric];\n'
          'const key = "soil_zoom_" + metric;\n'
          'let index = Number(flow.get(key));\n'
          'if (!Number.isInteger(index) || index < 0 || index >= windows.length) index = 3;\n'
          'const control = msg.payload && msg.payload.kind === "zoom";\n'
          'if (control) {\n'
          '  if (msg.payload.direction !== 1 && msg.payload.direction !== -1) return null;\n'
          '  index = Math.max(0,Math.min(windows.length-1,index - msg.payload.direction));\n'
          '  flow.set(key,index);\n'
          '} else if (typeof msg.payload !== "number" || !Number.isFinite(msg.payload)) return null;\n'
          'const now = Date.now();\n'
          'const axis = {ui_update:{chartOptions:{xAxis:{min:now-windows[index].ms,max:now}}}};\n'
          'const out = Array(8).fill(null);\n'
          'out[slot] = control ? axis : [axis,{topic:msg.topic,payload:msg.payload}];\n'
          'if (control) out[slot+4] = {payload:{kind:"zoom_state",label:windows[index].label}};\n'
          'return out;',
  'outputs': 8,
  'timeout': 0,
  'noerr': 0,
  'initialize': '',
  'finalize': '',
  'libs': [],
  'x': 1070,
  'y': 290,
  'wires': [['fbff2aaecefad608'],
            ['96da356ad3e51a8f'],
            ['19db8cbead5deb76'],
            ['9dff79b98691a3cc'],
            ['8956e58ce9c79806'],
            ['a15396ed4d992509'],
            ['dbb636acc2ac9826'],
            ['e64987630f834543']]},
 {'id': '8956e58ce9c79806',
  'type': 'ui-template',
  'z': 'soil_dashboard_tab',
  'g': '6e97c77aaa496d62',
  'group': 'sc_soil_graph_group',
  'page': '',
  'ui': '',
  'name': 'TEMP chart zoom',
  'order': 2,
  'width': '1',
  'height': '4',
  'head': '',
  'format': '<template>\n'
            '<div class="sc-zoom">\n'
            "  <span>View {{ msg && msg.payload && msg.payload.kind === 'zoom_state' ? msg.payload.label : "
            "'6 h' }}</span>\n"
            '  <button type="button" title="Zoom in: shorter time range" aria-label="Zoom in" '
            '@click="zoom(1)">+</button>\n'
            '  <button type="button" title="Zoom out: longer time range" aria-label="Zoom out" '
            '@click="zoom(-1)">&minus;</button>\n'
            '</div>\n'
            '</template>\n'
            '<script>\n'
            'export default {\n'
            '  methods: {\n'
            '    zoom(direction) {\n'
            '      this.send({payload:{kind:"zoom",metric:"temp",direction}});\n'
            '    }\n'
            '  }\n'
            '}\n'
            '</script>\n'
            '<style>\n'
            '.sc-zoom{height:100%;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:5px;color:#566;font-size:.72rem}\n'
            '.sc-zoom span{text-align:center;white-space:nowrap}\n'
            '.sc-zoom button{width:27px;height:25px;padding:0;border:1px solid '
            '#b7c4ce;border-radius:5px;background:#f5f8fa;color:#203447;font-size:17px;line-height:1;cursor:pointer}\n'
            '.sc-zoom button:hover{background:#e3edf4}\n'
            '</style>',
  'storeOutMessages': False,
  'passthru': False,
  'resendOnRefresh': True,
  'templateScope': 'local',
  'className': '',
  'x': 1550,
  'y': 230,
  'wires': [['d91b8a1b25c2c954']]},
 {'id': 'a15396ed4d992509',
  'type': 'ui-template',
  'z': 'soil_dashboard_tab',
  'g': '6e97c77aaa496d62',
  'group': 'sc_soil_graph_group',
  'page': '',
  'ui': '',
  'name': 'VWC chart zoom',
  'order': 4,
  'width': '1',
  'height': '4',
  'head': '',
  'format': '<template>\n'
            '<div class="sc-zoom">\n'
            "  <span>View {{ msg && msg.payload && msg.payload.kind === 'zoom_state' ? msg.payload.label : "
            "'6 h' }}</span>\n"
            '  <button type="button" title="Zoom in: shorter time range" aria-label="Zoom in" '
            '@click="zoom(1)">+</button>\n'
            '  <button type="button" title="Zoom out: longer time range" aria-label="Zoom out" '
            '@click="zoom(-1)">&minus;</button>\n'
            '</div>\n'
            '</template>\n'
            '<script>\n'
            'export default {\n'
            '  methods: {\n'
            '    zoom(direction) {\n'
            '      this.send({payload:{kind:"zoom",metric:"vwc",direction}});\n'
            '    }\n'
            '  }\n'
            '}\n'
            '</script>\n'
            '<style>\n'
            '.sc-zoom{height:100%;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:5px;color:#566;font-size:.72rem}\n'
            '.sc-zoom span{text-align:center;white-space:nowrap}\n'
            '.sc-zoom button{width:27px;height:25px;padding:0;border:1px solid '
            '#b7c4ce;border-radius:5px;background:#f5f8fa;color:#203447;font-size:17px;line-height:1;cursor:pointer}\n'
            '.sc-zoom button:hover{background:#e3edf4}\n'
            '</style>',
  'storeOutMessages': False,
  'passthru': False,
  'resendOnRefresh': True,
  'templateScope': 'local',
  'className': '',
  'x': 1550,
  'y': 290,
  'wires': [['d91b8a1b25c2c954']]},
 {'id': 'dbb636acc2ac9826',
  'type': 'ui-template',
  'z': 'soil_dashboard_tab',
  'g': '6e97c77aaa496d62',
  'group': 'sc_soil_graph_group',
  'page': '',
  'ui': '',
  'name': 'EC chart zoom',
  'order': 6,
  'width': '1',
  'height': '4',
  'head': '',
  'format': '<template>\n'
            '<div class="sc-zoom">\n'
            "  <span>View {{ msg && msg.payload && msg.payload.kind === 'zoom_state' ? msg.payload.label : "
            "'6 h' }}</span>\n"
            '  <button type="button" title="Zoom in: shorter time range" aria-label="Zoom in" '
            '@click="zoom(1)">+</button>\n'
            '  <button type="button" title="Zoom out: longer time range" aria-label="Zoom out" '
            '@click="zoom(-1)">&minus;</button>\n'
            '</div>\n'
            '</template>\n'
            '<script>\n'
            'export default {\n'
            '  methods: {\n'
            '    zoom(direction) {\n'
            '      this.send({payload:{kind:"zoom",metric:"ec",direction}});\n'
            '    }\n'
            '  }\n'
            '}\n'
            '</script>\n'
            '<style>\n'
            '.sc-zoom{height:100%;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:5px;color:#566;font-size:.72rem}\n'
            '.sc-zoom span{text-align:center;white-space:nowrap}\n'
            '.sc-zoom button{width:27px;height:25px;padding:0;border:1px solid '
            '#b7c4ce;border-radius:5px;background:#f5f8fa;color:#203447;font-size:17px;line-height:1;cursor:pointer}\n'
            '.sc-zoom button:hover{background:#e3edf4}\n'
            '</style>',
  'storeOutMessages': False,
  'passthru': False,
  'resendOnRefresh': True,
  'templateScope': 'local',
  'className': '',
  'x': 1540,
  'y': 350,
  'wires': [['d91b8a1b25c2c954']]},
 {'id': 'e64987630f834543',
  'type': 'ui-template',
  'z': 'soil_dashboard_tab',
  'g': '6e97c77aaa496d62',
  'group': 'sc_soil_graph_group',
  'page': '',
  'ui': '',
  'name': 'BATTERY chart zoom',
  'order': 8,
  'width': '1',
  'height': '4',
  'head': '',
  'format': '<template>\n'
            '<div class="sc-zoom">\n'
            "  <span>View {{ msg && msg.payload && msg.payload.kind === 'zoom_state' ? msg.payload.label : "
            "'6 h' }}</span>\n"
            '  <button type="button" title="Zoom in: shorter time range" aria-label="Zoom in" '
            '@click="zoom(1)">+</button>\n'
            '  <button type="button" title="Zoom out: longer time range" aria-label="Zoom out" '
            '@click="zoom(-1)">&minus;</button>\n'
            '</div>\n'
            '</template>\n'
            '<script>\n'
            'export default {\n'
            '  methods: {\n'
            '    zoom(direction) {\n'
            '      this.send({payload:{kind:"zoom",metric:"battery",direction}});\n'
            '    }\n'
            '  }\n'
            '}\n'
            '</script>\n'
            '<style>\n'
            '.sc-zoom{height:100%;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:5px;color:#566;font-size:.72rem}\n'
            '.sc-zoom span{text-align:center;white-space:nowrap}\n'
            '.sc-zoom button{width:27px;height:25px;padding:0;border:1px solid '
            '#b7c4ce;border-radius:5px;background:#f5f8fa;color:#203447;font-size:17px;line-height:1;cursor:pointer}\n'
            '.sc-zoom button:hover{background:#e3edf4}\n'
            '</style>',
  'storeOutMessages': False,
  'passthru': False,
  'resendOnRefresh': True,
  'templateScope': 'local',
  'className': '',
  'x': 1540,
  'y': 410,
  'wires': [['d91b8a1b25c2c954']]},
 {'id': 'ae0178f3742ff530',
  'type': 'mqtt-broker',
  'name': 'chirpstack_mosquito',
  'broker': 'localhost',
  'port': 1883,
  'clientid': '',
  'autoConnect': True,
  'usetls': False,
  'protocolVersion': 4,
  'keepalive': 60,
  'cleansession': True,
  'autoUnsubscribe': True,
  'birthTopic': '',
  'birthQos': '0',
  'birthRetain': 'false',
  'birthPayload': '',
  'birthMsg': {},
  'closeTopic': '',
  'closeQos': '0',
  'closeRetain': 'false',
  'closePayload': '',
  'closeMsg': {},
  'willTopic': '',
  'willQos': '0',
  'willRetain': 'false',
  'willPayload': '',
  'willMsg': {},
  'userProps': '',
  'sessionExpiry': ''},
 {'id': 'sc_soil_graph_group',
  'type': 'ui-group',
  'z': 'soil_dashboard_tab',
  'name': 'SoilNode history',
  'page': 'sc_soil_ui_page',
  'width': '4',
  'height': 1,
  'order': 2,
  'showTitle': True,
  'className': '',
  'visible': 'true',
  'disabled': 'false',
  'groupType': 'default'},
 {'id': 'sc_soil_ui_group',
  'type': 'ui-group',
  'z': 'soil_dashboard_tab',
  'name': 'SoilNode compact status and control',
  'page': 'sc_soil_ui_page',
  'width': '8',
  'height': 1,
  'order': 1,
  'showTitle': True,
  'className': '',
  'visible': 'true',
  'disabled': 'false',
  'groupType': 'default'},
 {'id': 'sc_soil_ui_page',
  'type': 'ui-page',
  'z': 'soil_dashboard_tab',
  'name': 'SoilNode Compact',
  'ui': 'f53e93e9ba219e63',
  'path': '/soil-node',
  'icon': 'mdi-waves-arrow-up',
  'layout': 'grid',
  'theme': 'e49416861823a329',
  'breakpoints': [{'name': 'Default', 'px': '0', 'cols': '3'},
                  {'name': 'Tablet', 'px': '576', 'cols': '6'},
                  {'name': 'Small Desktop', 'px': '768', 'cols': '9'},
                  {'name': 'Desktop', 'px': '1024', 'cols': '12'}],
  'order': 3,
  'className': '',
  'visible': True,
  'disabled': False},
 {'id': 'f53e93e9ba219e63',
  'type': 'ui-base',
  'name': 'My Dashboard',
  'path': '/dashboard',
  'appIcon': '',
  'includeClientData': True,
  'acceptsClientConfig': ['ui-notification', 'ui-control'],
  'showPathInSidebar': False,
  'headerContent': 'page',
  'navigationStyle': 'fixed',
  'titleBarStyle': 'default',
  'showReconnectNotification': True,
  'notificationDisplayTime': 1,
  'showDisconnectNotification': True,
  'allowInstall': True},
 {'id': 'e49416861823a329',
  'type': 'ui-theme',
  'name': 'MyTheme',
  'colors': {'surface': '#ffffff',
             'primary': '#0094ce',
             'bgPage': '#eeeeee',
             'groupBg': '#ffffff',
             'groupOutline': '#cccccc'},
  'sizes': {'density': 'default',
            'pagePadding': '12px',
            'groupGap': '12px',
            'groupBorderRadius': '4px',
            'widgetGap': '12px'}},
 {'id': '6f1540b3df81e984',
  'type': 'global-config',
  'env': [],
  'modules': {'@flowfuse/node-red-dashboard': '1.30.2'}}]


if __name__ == "__main__":
    write_flow(NODES, OUTPUT, indent=4)
