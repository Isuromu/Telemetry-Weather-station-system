"""The valve_2 dashboard: the same status card as valve_1 without the flow meter, so pressure and battery history only.

The imported JSON, examples/PressureControlNode2/include/pressure_node2_dashboard_flow.json, is a build artifact -- change this
builder and re-run it rather than editing that file:

    python tools/build_pressure_node2_dashboard.py

The flow carries no ChirpStack identifier: it subscribes dynamically and reads
IRRIGATION_APP_ID and VALVE2_DEV_EUI from the Node-RED environment. write_flow refuses a flow that
breaks that rule, tools/test_flow_ids.js checks it across every flow, and
tools/test_pressure_node2_dashboard.js covers this flow's import
invariants and decoded output.
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from flow_common import write_flow  # noqa: E402

OUTPUT = "examples/PressureControlNode2/include/pressure_node2_dashboard_flow.json"
DEVICE_ENV = "VALVE2_DEV_EUI"

NODES = [{'id': '1b8d0edf8c6e22f1',
  'type': 'group',
  'z': 'a833c4a28d65a483',
  'style': {'stroke': '#999999',
            'stroke-opacity': '1',
            'fill': 'none',
            'fill-opacity': '1',
            'label': True,
            'label-position': 'nw',
            'color': '#767676'},
  'nodes': ['fc0338054ac37d9f',
            'feebcae906e80b11',
            '61eb4fb9c93db6f0',
            '37280428c21d2d61',
            'bf4283af1364aaa6',
            '9032b1a09fa9326e'],
  'x': 914,
  'y': 279,
  'w': 1142,
  'h': 202},
 {'id': 'fc0338054ac37d9f',
  'type': 'function',
  'z': 'a833c4a28d65a483',
  'g': '1b8d0edf8c6e22f1',
  'name': 'Build pressure and battery chart inputs',
  'func': '// Pressure and battery charts for valve_2 (no flow meter).\n'
          'const topic = String(msg.topic || "");\n'
          'if (topic && !topic.endsWith("/up")) return null;\n'
          'const event = msg.payload || {};\n'
          'if (event.fPort !== 31) return null;\n'
          'const raw = event.object || {};\n'
          'function num(v) { return typeof v === "number" && isFinite(v) ? v : null; }\n'
          'const upstream = num(raw.upstream_pressure_bar);\n'
          'const downstream = num(raw.downstream_pressure_bar);\n'
          'const battery = num(raw.battery_voltage_v);\n'
          'const chartMsgs = [];\n'
          'if (upstream !== null) chartMsgs.push({ pn2Metric: "pressure", topic: "Upstream", payload: '
          'upstream });\n'
          'if (downstream !== null) chartMsgs.push({ pn2Metric: "pressure", topic: "Downstream", payload: '
          'downstream });\n'
          'return [chartMsgs.length ? chartMsgs : null,\n'
          '        battery !== null ? { pn2Metric: "battery", topic: "Battery", payload: battery } : null];',
  'outputs': 2,
  'timeout': 0,
  'noerr': 0,
  'initialize': '',
  'finalize': '',
  'libs': [],
  'x': 1090,
  'y': 340,
  'wires': [['feebcae906e80b11'], ['feebcae906e80b11']]},
 {'id': 'feebcae906e80b11',
  'type': 'function',
  'z': 'a833c4a28d65a483',
  'g': '1b8d0edf8c6e22f1',
  'name': 'Valve 2 chart time zoom',
  'func': '// Per-chart time window. + shows a shorter span; - shows a longer span.\n'
          'const windows = [\n'
          '  {label:"30 min",ms:1800000}, {label:"1 h",ms:3600000},\n'
          '  {label:"3 h",ms:10800000}, {label:"6 h",ms:21600000},\n'
          '  {label:"12 h",ms:43200000}, {label:"24 h",ms:86400000}\n'
          '];\n'
          'const metrics = {pressure:0,battery:1};\n'
          'const metric = msg.pn2Metric || (msg.payload && msg.payload.metric);\n'
          'if (!Object.prototype.hasOwnProperty.call(metrics,metric)) return null;\n'
          'const slot = metrics[metric];\n'
          'const key = "pn2_zoom_" + metric;\n'
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
          'const out = Array(4).fill(null);\n'
          'out[slot] = control ? axis : [axis,{topic:msg.topic,payload:msg.payload}];\n'
          'if (control) out[slot+2] = {payload:{kind:"zoom_state",label:windows[index].label}};\n'
          'return out;',
  'outputs': 4,
  'timeout': 0,
  'noerr': 0,
  'initialize': '',
  'finalize': '',
  'libs': [],
  'x': 1480,
  'y': 380,
  'wires': [['61eb4fb9c93db6f0'], ['bf4283af1364aaa6'], ['37280428c21d2d61'], ['9032b1a09fa9326e']]},
 {'id': '61eb4fb9c93db6f0',
  'type': 'ui-chart',
  'z': 'a833c4a28d65a483',
  'g': '1b8d0edf8c6e22f1',
  'group': 'a53345cd688231d0',
  'name': 'Pressure history',
  'label': 'Pressure (bar)',
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
  'showLegend': True,
  'removeOlder': '24',
  'removeOlderUnit': '60',
  'removeOlderPoints': '',
  'colors': ['#1f77b4',
             '#ff7f0e',
             '#2ca02c',
             '#000000',
             '#000000',
             '#000000',
             '#000000',
             '#000000',
             '#000000'],
  'textColor': ['#666666'],
  'textColorDefault': True,
  'gridColor': ['#e5e5e5'],
  'gridColorDefault': True,
  'width': '3',
  'height': '4',
  'className': '',
  'interpolation': 'linear',
  'x': 1690,
  'y': 320,
  'wires': [[]]},
 {'id': '37280428c21d2d61',
  'type': 'ui-template',
  'z': 'a833c4a28d65a483',
  'g': '1b8d0edf8c6e22f1',
  'group': 'a53345cd688231d0',
  'page': '',
  'ui': '',
  'name': 'Pressure chart zoom',
  'order': 2,
  'width': '1',
  'height': '4',
  'head': '',
  'format': '<template>\n'
            '<div class="pn2-zoom">\n'
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
            '      this.send({payload:{kind:"zoom",metric:"pressure",direction}});\n'
            '    }\n'
            '  }\n'
            '}\n'
            '</script>\n'
            '<style>\n'
            '.pn2-zoom{height:100%;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:5px;color:#566;font-size:.72rem}\n'
            '.pn2-zoom span{text-align:center;white-space:nowrap}\n'
            '.pn2-zoom button{width:27px;height:25px;padding:0;border:1px solid '
            '#b7c4ce;border-radius:5px;background:#f5f8fa;color:#203447;font-size:17px;line-height:1;cursor:pointer}\n'
            '.pn2-zoom button:hover{background:#e3edf4}\n'
            '</style>',
  'storeOutMessages': False,
  'passthru': False,
  'resendOnRefresh': True,
  'templateScope': 'local',
  'className': '',
  'x': 1930,
  'y': 320,
  'wires': [['feebcae906e80b11']]},
 {'id': 'bf4283af1364aaa6',
  'type': 'ui-chart',
  'z': 'a833c4a28d65a483',
  'g': '1b8d0edf8c6e22f1',
  'group': 'a53345cd688231d0',
  'name': 'Battery voltage history',
  'label': 'Battery voltage (V)',
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
  'colors': ['#1f77b4',
             '#000000',
             '#000000',
             '#000000',
             '#000000',
             '#000000',
             '#000000',
             '#000000',
             '#000000'],
  'textColor': ['#666666'],
  'textColorDefault': True,
  'gridColor': ['#e5e5e5'],
  'gridColorDefault': True,
  'width': '3',
  'height': '4',
  'className': '',
  'interpolation': 'linear',
  'x': 1690,
  'y': 440,
  'wires': [[]]},
 {'id': '9032b1a09fa9326e',
  'type': 'ui-template',
  'z': 'a833c4a28d65a483',
  'g': '1b8d0edf8c6e22f1',
  'group': 'a53345cd688231d0',
  'page': '',
  'ui': '',
  'name': 'Battery chart zoom',
  'order': 4,
  'width': '1',
  'height': '4',
  'head': '',
  'format': '<template>\n'
            '<div class="pn2-zoom">\n'
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
            '.pn2-zoom{height:100%;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:5px;color:#566;font-size:.72rem}\n'
            '.pn2-zoom span{text-align:center;white-space:nowrap}\n'
            '.pn2-zoom button{width:27px;height:25px;padding:0;border:1px solid '
            '#b7c4ce;border-radius:5px;background:#f5f8fa;color:#203447;font-size:17px;line-height:1;cursor:pointer}\n'
            '.pn2-zoom button:hover{background:#e3edf4}\n'
            '</style>',
  'storeOutMessages': False,
  'passthru': False,
  'resendOnRefresh': True,
  'templateScope': 'local',
  'className': '',
  'x': 1930,
  'y': 440,
  'wires': [['feebcae906e80b11']]},
 {'id': 'a53345cd688231d0',
  'type': 'ui-group',
  'z': 'a833c4a28d65a483',
  'name': 'Charts',
  'page': '18e543d1f5a060fa',
  'width': '4',
  'height': 1,
  'order': 1,
  'showTitle': True,
  'className': '',
  'visible': 'true',
  'disabled': 'false',
  'groupType': 'default'},
 {'id': '18e543d1f5a060fa',
  'type': 'ui-page',
  'z': 'a833c4a28d65a483',
  'name': 'Valve 2',
  'ui': 'f53e93e9ba219e63',
  'path': '/page2',
  'icon': 'mdi-pipe-valve',
  'layout': 'grid',
  'theme': 'e49416861823a329',
  'breakpoints': [{'name': 'Default', 'px': '0', 'cols': '3'},
                  {'name': 'Tablet', 'px': '576', 'cols': '6'},
                  {'name': 'Small Desktop', 'px': '768', 'cols': '9'},
                  {'name': 'Desktop', 'px': '1024', 'cols': '12'}],
  'order': 4,
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
 {'id': '5c3e62f681d5ca29',
  'type': 'group',
  'z': 'a833c4a28d65a483',
  'style': {'stroke': '#999999',
            'stroke-opacity': '1',
            'fill': 'none',
            'fill-opacity': '1',
            'label': True,
            'label-position': 'nw',
            'color': '#767676'},
  'nodes': ['f6adecbd497e2de9',
            'f1428117c7276bb0',
            'c7fc1d6f240aabb7',
            'd7155969941e41f6',
            '4d60505cbce5fd1b',
            'aa6da601debd0c9b',
            'e0d2ef2e5fa9eeea'],
  'x': 54,
  'y': 89,
  'w': 1792,
  'h': 132},
 {'id': 'f6adecbd497e2de9',
  'type': 'inject',
  'z': 'a833c4a28d65a483',
  'g': '5c3e62f681d5ca29',
  'name': 'Refresh valve_2 subscription',
  'props': [{'p': 'payload'}],
  'repeat': '',
  'crontab': '',
  'once': True,
  'onceDelay': '0.5',
  'topic': '',
  'payload': '',
  'payloadType': 'date',
  'x': 220,
  'y': 130,
  'wires': [['f1428117c7276bb0']]},
 {'id': 'f1428117c7276bb0',
  'type': 'function',
  'z': 'a833c4a28d65a483',
  'g': '5c3e62f681d5ca29',
  'name': 'Subscribe valve_2 events',
  'func': '// Subscribe dynamically so the application ID and DevEUI stay out of the flow export.\n'
          "// Set IRRIGATION_APP_ID and VALVE2_DEV_EUI in this node's Environment tab,\n"
          '// or once in process env for the Node-RED service.\n'
          'const app = String(env.get("IRRIGATION_APP_ID") || "").toLowerCase();\n'
          'const eui = String(env.get("VALVE2_DEV_EUI") || "").toLowerCase();\n'
          'if (!/^[a-f0-9-]{36}$/.test(app) || !/^[a-f0-9]{16}$/.test(eui)) {\n'
          '    node.status({ fill: "red", shape: "ring", text: "IRRIGATION_APP_ID / VALVE2_DEV_EUI missing" '
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
  'x': 220,
  'y': 180,
  'wires': [['c7fc1d6f240aabb7']]},
 {'id': 'c7fc1d6f240aabb7',
  'type': 'mqtt in',
  'z': 'a833c4a28d65a483',
  'g': '5c3e62f681d5ca29',
  'name': 'ChirpStack events',
  'topic': '',
  'qos': '0',
  'datatype': 'json',
  'broker': 'ae0178f3742ff530',
  'nl': False,
  'rap': True,
  'rh': 0,
  'inputs': 1,
  'x': 620,
  'y': 180,
  'wires': [['fc0338054ac37d9f', 'd7155969941e41f6']]},
 {'id': 'd7155969941e41f6',
  'type': 'function',
  'z': 'a833c4a28d65a483',
  'g': '5c3e62f681d5ca29',
  'name': 'Track valve_2 state',
  'func': '// Keep a dashboard snapshot of valve_2 in the flow context.\n'
          '// ChirpStack publishes every event kind on event/+, so ack, txack, join, log\n'
          '// and status are recorded too. Only FPort 31 "up" events carry the protocol v2\n'
          '// status object produced by the FPort 30/31 codec.\n'
          'const APP_ID = String(env.get("IRRIGATION_APP_ID") || "").toLowerCase();\n'
          'const DEV_EUI = String(env.get("VALVE2_DEV_EUI") || "").toLowerCase();\n'
          'const PREFIX = "application/" + APP_ID + "/device/" + DEV_EUI + "/event/";\n'
          '\n'
          'let event = msg.payload;\n'
          'try {\n'
          '    if (Buffer.isBuffer(event)) event = JSON.parse(event.toString("utf8"));\n'
          '    else if (typeof event === "string") event = JSON.parse(event);\n'
          '} catch (err) {\n'
          '    node.warn("Invalid ChirpStack JSON: " + err.message);\n'
          '    return null;\n'
          '}\n'
          'if (!event || typeof event !== "object") return null;\n'
          '\n'
          'function num(v) { return typeof v === "number" && isFinite(v) ? v : null; }\n'
          '\n'
          'const topic = String(msg.topic || "");\n'
          'const kind = topic.indexOf(PREFIX) === 0 ? topic.slice(PREFIX.length).split("/")[0] : "up";\n'
          '\n'
          'const s = Object.assign({}, flow.get("pn2_state"));\n'
          's.last_event = kind;\n'
          's.last_event_time = new Date().toISOString();\n'
          '\n'
          'if (kind === "up") {\n'
          '    if (event.fPort !== 31) {\n'
          '        s.decode_error = "Ignored an FPort " + event.fPort + " uplink; valve_2 status is FPort '
          '31.";\n'
          '    } else if (!event.object || typeof event.object !== "object") {\n'
          '        s.decode_error = "The FPort 31 uplink has no decoded object. Check that the protocol v2 '
          'codec (FPort 30 down, FPort 31 up) is set on the valve_2 device profile.";\n'
          '    } else {\n'
          '        const d = event.object;\n'
          '        s.decode_error = "";\n'
          '        s.raw = d;\n'
          '        s.protocol_version = num(d.protocol_version);\n'
          '        s.runtime_mode = d.runtime_mode;\n'
          '        s.status_reason = d.status_reason;\n'
          '        s.pcv_last_commanded = d.pcv_last_commanded;\n'
          '        s.pcv_position_verified = d.pcv_position_verified === true;\n'
          '        s.sleep_seconds = num(d.sleep_seconds);\n'
          '        s.sleep_minutes = num(d.sleep_minutes);\n'
          '        s.last_command_id = num(d.last_command_id);\n'
          '        s.battery_voltage_v = num(d.battery_voltage_v);\n'
          '        s.battery_soc_percent = num(d.battery_soc_percent);\n'
          '        s.upstream_pressure_bar = num(d.upstream_pressure_bar);\n'
          '        s.upstream_temperature_c = num(d.upstream_temperature_c);\n'
          '        s.upstream_scale_validated = d.upstream_scale_validated === true;\n'
          '        s.downstream_pressure_bar = num(d.downstream_pressure_bar);\n'
          '        s.downstream_temperature_c = num(d.downstream_temperature_c);\n'
          '        s.downstream_scale_validated = d.downstream_scale_validated === true;\n'
          '        s.last_seen = event.time || new Date().toISOString();\n'
          '        s.f_cnt = num(event.fCnt);\n'
          '        s.f_port = num(event.fPort);\n'
          '        s.dr = num(event.dr);\n'
          '        s.adr = event.adr === true;\n'
          '        s.dev_addr = event.devAddr;\n'
          '        s.device_class = (event.deviceInfo || {}).deviceClassEnabled;\n'
          '        s.uplink_count = (num(s.uplink_count) || 0) + 1;\n'
          '        const rx = Array.isArray(event.rxInfo) ? event.rxInfo : [];\n'
          '        if (rx.length > 0) {\n'
          '            let best = rx[0];\n'
          '            for (let i = 1; i < rx.length; i++) {\n'
          '                if (Number(rx[i].snr) > Number(best.snr)) best = rx[i];\n'
          '            }\n'
          '            s.rssi = num(best.rssi);\n'
          '            s.snr = num(best.snr);\n'
          '            s.gateway_id = best.gatewayId;\n'
          '        }\n'
          '        const lora = ((event.txInfo || {}).modulation || {}).lora || {};\n'
          '        s.frequency = num((event.txInfo || {}).frequency);\n'
          '        s.sf = num(lora.spreadingFactor);\n'
          '        s.bandwidth = num(lora.bandwidth);\n'
          '        if (s.sent_command_id !== null && s.sent_command_id !== undefined &&\n'
          '            s.last_command_id === s.sent_command_id) {\n'
          '            s.sent_state = "Reported back by the device: " + (d.status_reason || "accepted");\n'
          '        }\n'
          '        node.status({ fill: "green", shape: "dot", text: "FPort 31 " + (s.status_reason || '
          '"uplink") });\n'
          '    }\n'
          '} else if (kind === "ack") {\n'
          '    s.network_ack = event.acknowledged === true ? "Downlink acknowledged" : "Downlink not '
          'acknowledged";\n'
          '} else if (kind === "txack") {\n'
          '    s.network_txack = "Gateway transmitted the downlink";\n'
          '} else if (kind === "join") {\n'
          '    s.network_join = s.last_event_time;\n'
          '} else if (kind === "log") {\n'
          '    s.network_log = event.description || event.code || "Device log";\n'
          '} else if (kind === "status") {\n'
          '    s.network_status = "Network server battery level " + event.battery;\n'
          '} else {\n'
          '    return null;\n'
          '}\n'
          '\n'
          'flow.set("pn2_state", s);\n'
          'msg.payload = { kind: "state", state: s };\n'
          'return msg;',
  'outputs': 1,
  'timeout': 0,
  'noerr': 0,
  'initialize': '',
  'finalize': '',
  'libs': [],
  'x': 890,
  'y': 130,
  'wires': [['4d60505cbce5fd1b']]},
 {'id': '4d60505cbce5fd1b',
  'type': 'ui-template',
  'z': 'a833c4a28d65a483',
  'g': '5c3e62f681d5ca29',
  'group': '2b20e091099f06cd',
  'page': '',
  'ui': '',
  'name': 'valve_2 state and commands',
  'order': 1,
  'width': '8',
  'height': '13',
  'head': '',
  'format': '<template>\n'
            '<div class="pn2">\n'
            '  <div class="pn2-head">\n'
            '    <strong>valve_2</strong>\n'
            '    <span v-if="state.last_seen">Last uplink {{ local(state.last_seen) }} &middot; FCnt {{ '
            'show(state.f_cnt) }} &middot; {{ show(state.uplink_count) }} received</span>\n'
            '    <span v-else>Waiting for an FPort 31 status uplink</span>\n'
            '  </div>\n'
            '\n'
            '  <div class="pn2-grid">\n'
            '    <div><small>Status reason</small><b>{{ show(state.status_reason) }}</b></div>\n'
            '    <div><small>Runtime mode</small><b>{{ show(state.runtime_mode) }}</b></div>\n'
            '    <div><small>PCV commanded</small><b>{{ show(state.pcv_last_commanded) }}</b></div>\n'
            '    <div><small>Position verified</small><b>{{ state.pcv_position_verified ? "yes" : "no" '
            '}}</b></div>\n'
            '    <div><small>Sleep interval</small><b>{{ sleepText(state.sleep_seconds) }}</b></div>\n'
            '    <div><small>Last command ID</small><b>{{ show(state.last_command_id) }}</b></div>\n'
            '    <div><small>Battery</small><b>{{ show(state.battery_voltage_v, " V") }}</b></div>\n'
            '    <div><small>Pressure upstream / downstream</small><b>{{ show(state.upstream_pressure_bar, " '
            'bar") }} / {{ show(state.downstream_pressure_bar, " bar") }}</b></div>\n'
            '    <div><small>Temperature upstream / downstream</small><b>{{ '
            'show(state.upstream_temperature_c, " C") }} / {{ show(state.downstream_temperature_c, " C") '
            '}}</b></div>\n'
            '    <div><small>Radio</small><b>{{ radioText }}</b></div>\n'
            '    <div><small>Gateway</small><b>{{ state.gateway_id || "-" }}</b></div>\n'
            '  </div>\n'
            '\n'
            '  <div class="pn2-controls">\n'
            '    <h3>Command valve_2</h3>\n'
            '    <p>valve_2 is Class A: it only hears a downlink in the receive window after one of its own '
            'uplinks, so every command below waits in ChirpStack until the node next wakes, and the node '
            'then sleeps for its configured interval. Each command is sent with a fresh command_id, because '
            'the node ignores an id it has already accepted.</p>\n'
            '    <div class="pn2-row">\n'
            '      <v-text-field :model-value="sleep_seconds" @update:model-value="onSleepInput" '
            'type="number" min="10" max="86400" step="1" label="Sleep interval in seconds (10 to 86400)" '
            'density="compact"></v-text-field>\n'
            '      <span class="pn2-note">{{ sleepNote }}</span>\n'
            '      <v-btn @click="sendSleep" :disabled="!sleepValid">Set sleep interval</v-btn>\n'
            '    </div>\n'
            '    <div class="pn2-row">\n'
            '      <v-btn color="success" @click="issue(\'open\', true)">Open + sleep</v-btn>\n'
            '      <v-btn color="error" @click="issue(\'close\', true)">Close + sleep</v-btn>\n'
            '      <v-btn @click="issue(\'open\', false)">Open only</v-btn>\n'
            '      <v-btn @click="issue(\'close\', false)">Close only</v-btn>\n'
            '      <v-btn variant="text" @click="issue(\'none\', false)">No-op test</v-btn>\n'
            '    </div>\n'
            '  </div>\n'
            '\n'
            '  <div class="pn2-footer">\n'
            '    <div><b>Last downlink queued:</b> {{ state.sent_at ? local(state.sent_at) : "-" }}<span '
            'v-if="state.sent_object"> &middot; ID {{ show(state.sent_command_id) }} &middot; {{ sentText '
            '}}</span></div>\n'
            '    <div><b>Delivery:</b> {{ state.sent_state || "-" }}</div>\n'
            '    <div><b>Last event:</b> {{ show(state.last_event) }} at {{ state.last_event_time ? '
            'local(state.last_event_time) : "-" }}<span v-if="state.network_ack"> &middot; {{ '
            'state.network_ack }}</span><span v-if="state.network_txack"> &middot; {{ state.network_txack '
            '}}</span><span v-if="state.network_join"> &middot; joined {{ local(state.network_join) '
            '}}</span><span v-if="state.network_status"> &middot; {{ state.network_status }}</span><span '
            'v-if="state.network_log"> &middot; {{ state.network_log }}</span></div>\n'
            '    <div v-if="state.decode_error" class="pn2-alert">{{ state.decode_error }}</div>\n'
            '    <div v-if="state.ui_error" class="pn2-alert">{{ state.ui_error }}</div>\n'
            '    <div><v-btn variant="text" size="small" @click="show_raw = !show_raw">{{ show_raw ? "Hide" '
            ': "Show" }} the raw status object</v-btn></div>\n'
            '    <pre v-if="show_raw" class="pn2-raw">{{ rawText }}</pre>\n'
            '  </div>\n'
            '</div>\n'
            '</template>\n'
            '<script>\n'
            'export default {\n'
            '  data() {\n'
            '    return { state: {}, sleep_seconds: 60, touched: false, show_raw: false };\n'
            '  },\n'
            '  computed: {\n'
            '    sleepValid() {\n'
            '      const n = Number(this.sleep_seconds);\n'
            '      return Number.isInteger(n) && n >= 10 && n <= 86400;\n'
            '    },\n'
            '    sleepNote() {\n'
            '      if (!this.sleepValid) return "whole number of seconds, 10 to 86400";\n'
            '      const n = Number(this.sleep_seconds);\n'
            '      if (n >= 3600) return "= " + n + " s (" + (n / 3600).toFixed(2) + " h)";\n'
            '      if (n >= 60) return "= " + n + " s (" + (n / 60).toFixed(1) + " min)";\n'
            '      return "= " + n + " s";\n'
            '    },\n'
            '    sentText() {\n'
            '      return this.state.sent_object ? JSON.stringify(this.state.sent_object) : "-";\n'
            '    },\n'
            '    rawText() {\n'
            '      return this.state.raw ? JSON.stringify(this.state.raw, null, 1) : "No FPort 31 status '
            'object received yet.";\n'
            '    },\n'
            '    radioText() {\n'
            '      const parts = [];\n'
            '      if (this.state.rssi !== null && this.state.rssi !== undefined) parts.push(this.state.rssi '
            '+ " dBm");\n'
            '      if (this.state.snr !== null && this.state.snr !== undefined) parts.push(this.state.snr + '
            '" dB SNR");\n'
            '      if (this.state.sf) parts.push("SF" + this.state.sf);\n'
            '      if (this.state.dr !== null && this.state.dr !== undefined) parts.push("DR" + '
            'this.state.dr);\n'
            '      if (this.state.frequency) parts.push((this.state.frequency / 1000000).toFixed(1) + " '
            'MHz");\n'
            '      return parts.length > 0 ? parts.join(" | ") : "-";\n'
            '    }\n'
            '  },\n'
            '  watch: {\n'
            '    msg: {\n'
            '      handler(m) {\n'
            '        if (!m || !m.payload || m.payload.kind !== "state") return;\n'
            '        this.state = m.payload.state || {};\n'
            '        // Follow the interval the node is really sleeping for until the\n'
            '        // operator types a new one.\n'
            '        if (!this.touched && typeof this.state.sleep_seconds === "number" && '
            'this.state.sleep_seconds > 0) {\n'
            '          this.sleep_seconds = this.state.sleep_seconds;\n'
            '        }\n'
            '      },\n'
            '      immediate: true\n'
            '    }\n'
            '  },\n'
            '  methods: {\n'
            '    show(v, suffix) {\n'
            '      if (v === null || v === undefined || v === "") return "-";\n'
            '      return String(v) + (suffix || "");\n'
            '    },\n'
            '    local(iso) {\n'
            '      if (!iso) return "-";\n'
            '      const d = new Date(iso);\n'
            '      return isNaN(d.getTime()) ? String(iso) : d.toLocaleString();\n'
            '    },\n'
            '    sleepText(seconds) {\n'
            '      const n = Number(seconds);\n'
            '      if (!isFinite(n) || n <= 0) return "-";\n'
            '      if (n >= 3600) return n + " s (" + (n / 3600).toFixed(2) + " h)";\n'
            '      if (n >= 60) return n + " s (" + (n / 60).toFixed(1) + " min)";\n'
            '      return n + " s";\n'
            '    },\n'
            '    onSleepInput(value) {\n'
            '      this.touched = true;\n'
            '      this.sleep_seconds = value === "" || value === null || value === undefined ? null : '
            'Number(value);\n'
            '    },\n'
            '    sendSleep() {\n'
            '      if (!this.sleepValid) return;\n'
            '      this.state.ui_error = "";\n'
            '      this.send({ payload: { kind: "command", sleep_seconds: Number(this.sleep_seconds) } });\n'
            '    },\n'
            '    issue(pcv, withSleep) {\n'
            '      if (withSleep && !this.sleepValid) return;\n'
            '      const command = { kind: "command", pcv: pcv };\n'
            '      let text = pcv === "none" ? "no valve movement" : pcv.toUpperCase();\n'
            '      if (withSleep) {\n'
            '        command.sleep_seconds = Number(this.sleep_seconds);\n'
            '        text += " plus a " + command.sleep_seconds + " second sleep interval";\n'
            '      }\n'
            '      if (pcv === "open" || pcv === "close") {\n'
            '        if (!window.confirm("Send " + text + " to valve_2? The command is delivered after its '
            'next uplink.")) return;\n'
            '      }\n'
            '      this.state.ui_error = "";\n'
            '      this.send({ payload: command });\n'
            '    }\n'
            '  }\n'
            '}\n'
            '</script>\n'
            '<style>\n'
            '.pn2{padding:8px}\n'
            '.pn2-head{display:flex;justify-content:space-between;gap:12px;align-items:center;font-size:1.15rem;flex-wrap:wrap}\n'
            '.pn2-head span{font-size:.8rem}\n'
            '.pn2-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(190px,1fr));gap:10px;margin:16px '
            '0}\n'
            '.pn2-grid>div{padding:12px;border:1px solid #ccd7df;border-radius:8px}\n'
            '.pn2-grid small{display:block;color:#566}\n'
            '.pn2-grid b{font-size:1.05rem}\n'
            '.pn2-controls,.pn2-footer{border-top:1px solid #ccd7df;padding-top:12px;margin-top:12px}\n'
            '.pn2-controls p,.pn2-footer p{font-size:.85rem}\n'
            '.pn2-row{display:flex;gap:10px;align-items:center;flex-wrap:wrap;margin-bottom:10px}\n'
            '.pn2-note{font-size:.85rem;color:#566;white-space:nowrap}\n'
            '.pn2-footer div{font-size:.85rem;margin-bottom:4px}\n'
            '.pn2-alert{background:#b3261e;color:white;border-radius:5px;padding:8px;margin:8px 0}\n'
            '.pn2-raw{background:#f4f7f9;border:1px solid '
            '#ccd7df;border-radius:5px;padding:8px;font-size:.75rem;overflow:auto;max-height:320px}\n'
            '</style>',
  'storeOutMessages': False,
  'passthru': False,
  'resendOnRefresh': True,
  'templateScope': 'local',
  'className': '',
  'x': 1180,
  'y': 130,
  'wires': [['aa6da601debd0c9b']]},
 {'id': 'aa6da601debd0c9b',
  'type': 'function',
  'z': 'a833c4a28d65a483',
  'g': '5c3e62f681d5ca29',
  'name': 'Send valve_2 command',
  'func': "// Build the FPort 30 downlink for valve_2. ChirpStack runs the codec's\n"
          '// encodeDownlink on "object", so the field names here are the codec\'s input\n'
          '// names: pcv, sleep_seconds, sleep_minutes, flow_total_reset and command_id.\n'
          'const APP_ID = String(env.get("IRRIGATION_APP_ID") || "").toLowerCase();\n'
          'const DEV_EUI = String(env.get("VALVE2_DEV_EUI") || "").toLowerCase();\n'
          'const CMD_FPORT = 30;\n'
          '// Keep in step with PressureNode2Config.h (MIN_REPORT_INTERVAL_SECONDS,\n'
          "// MAX_REPORT_INTERVAL_SECONDS) and with the codec's own 10..86400 s bound.\n"
          'const MIN_SLEEP_SECONDS = 10;\n'
          'const MAX_SLEEP_SECONDS = 86400;\n'
          '\n'
          'const s = Object.assign({}, flow.get("pn2_state"));\n'
          'const action = msg.payload || {};\n'
          '\n'
          'function snapshot() {\n'
          '    flow.set("pn2_state", s);\n'
          '    return { payload: { kind: "state", state: s } };\n'
          '}\n'
          'function feedback(text) {\n'
          '    s.ui_error = text;\n'
          '    return [null, snapshot()];\n'
          '}\n'
          '\n'
          'function dispatch(object, description) {\n'
          '    // valve_2 ignores a command_id it has already accepted, so every command\n'
          '    // needs a fresh one. Seed from the last id the device reported, then walk\n'
          '    // forward and remember the next candidate between messages.\n'
          '    let id = flow.get("pn2_next_cmd_id");\n'
          '    if (!Number.isInteger(id) || id < 0 || id > 65534) {\n'
          '        const last = Number(s.last_command_id);\n'
          '        id = Number.isInteger(last) && last >= 0 && last < 65535 ? (last + 1) % 65535 : 1;\n'
          '    }\n'
          '    if (id === Number(s.last_command_id)) id = (id + 1) % 65535;\n'
          '    flow.set("pn2_next_cmd_id", (id + 1) % 65535);\n'
          '\n'
          '    object.command_id = id;\n'
          '    s.sent_command_id = id;\n'
          '    s.sent_at = new Date().toISOString();\n'
          '    s.sent_description = description;\n'
          '    s.sent_object = object;\n'
          '    s.sent_state = "Queued in ChirpStack. valve_2 is Class A, so it collects the command in the '
          'receive window after its next uplink.";\n'
          '    s.ui_error = "";\n'
          '    node.status({ fill: "yellow", shape: "dot", text: "command " + id + ": " + description });\n'
          '\n'
          '    return [{\n'
          '        topic: "application/" + APP_ID + "/device/" + DEV_EUI + "/command/down",\n'
          '        payload: { devEui: DEV_EUI, confirmed: false, fPort: CMD_FPORT, object: object },\n'
          '        qos: "0",\n'
          '        retain: false\n'
          '    }, snapshot()];\n'
          '}\n'
          '\n'
          'if (action.kind !== "command") return null;\n'
          '\n'
          'const object = {};\n'
          'if (action.pcv === "open" || action.pcv === "close" || action.pcv === "none") {\n'
          '    object.pcv = action.pcv;\n'
          '}\n'
          'if (action.sleep_seconds !== undefined && action.sleep_seconds !== null && action.sleep_seconds '
          '!== "") {\n'
          '    const seconds = Number(action.sleep_seconds);\n'
          '    if (!Number.isInteger(seconds)) {\n'
          '        return feedback("Sleep interval must be a whole number of seconds.");\n'
          '    }\n'
          '    if (seconds < MIN_SLEEP_SECONDS || seconds > MAX_SLEEP_SECONDS) {\n'
          '        return feedback("Sleep interval must be " + MIN_SLEEP_SECONDS + " to " + '
          'MAX_SLEEP_SECONDS + " seconds.");\n'
          '    }\n'
          '    object.sleep_seconds = seconds;\n'
          '}\n'
          'if (object.pcv === undefined && object.sleep_seconds === undefined) {\n'
          '    return feedback("Choose a valve action, a sleep interval, or both.");\n'
          '}\n'
          '\n'
          'const description = object.pcv === undefined\n'
          '    ? "sleep " + object.sleep_seconds + " s"\n'
          '    : object.pcv + (object.sleep_seconds === undefined ? "" : " + sleep " + object.sleep_seconds '
          '+ " s");\n'
          'return dispatch(object, description);',
  'outputs': 2,
  'timeout': 0,
  'noerr': 0,
  'initialize': '',
  'finalize': '',
  'libs': [],
  'x': 1450,
  'y': 130,
  'wires': [['e0d2ef2e5fa9eeea'], ['4d60505cbce5fd1b']]},
 {'id': 'e0d2ef2e5fa9eeea',
  'type': 'mqtt out',
  'z': 'a833c4a28d65a483',
  'g': '5c3e62f681d5ca29',
  'name': 'Queue ChirpStack downlink',
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
  'y': 180,
  'wires': []},
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
 {'id': '2b20e091099f06cd',
  'type': 'ui-group',
  'z': 'a833c4a28d65a483',
  'name': 'Valve 2 state and commands',
  'page': '18e543d1f5a060fa',
  'width': '8',
  'height': 1,
  'order': 2,
  'showTitle': True,
  'className': '',
  'visible': 'true',
  'disabled': 'false',
  'groupType': 'default'},
 {'id': 'd02d6a3299cefe09',
  'type': 'global-config',
  'env': [],
  'modules': {'@flowfuse/node-red-dashboard': '1.30.2'}}]


if __name__ == "__main__":
    write_flow(NODES, OUTPUT, indent=4)
