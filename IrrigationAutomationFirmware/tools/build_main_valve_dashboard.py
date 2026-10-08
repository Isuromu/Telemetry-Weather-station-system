"""The MainValve dashboard: the FPort 31 status card and the ordered command queue with the five-minute timeout and unlock path.

The imported JSON, examples/MainValve/include/main_valve_dashboard_flow.json, is a build artifact -- change this
builder and re-run it rather than editing that file:

    python tools/build_main_valve_dashboard.py

The flow carries no ChirpStack identifier: it subscribes dynamically and reads
IRRIGATION_APP_ID and MAIN_DEV_EUI from the Node-RED environment. write_flow refuses a flow that
breaks that rule, tools/test_flow_ids.js checks it across every flow, and
tools/test_main_valve_dashboard.js covers this flow's import
invariants and decoded output.
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from flow_common import write_flow  # noqa: E402

OUTPUT = "examples/MainValve/include/main_valve_dashboard_flow.json"
DEVICE_ENV = "MAIN_DEV_EUI"

NODES = [{'id': '1a3af649fb7e41e9',
  'type': 'group',
  'z': '8df55dba201de6b7',
  'style': {'stroke': '#999999',
            'stroke-opacity': '1',
            'fill': 'none',
            'fill-opacity': '1',
            'label': True,
            'label-position': 'nw',
            'color': '#767676'},
  'nodes': ['39db54868b9406e1',
            '2cc81e7a4f64eb2a',
            '89a07c737bbe1a55',
            'a7a041eaf4f7e270',
            '98ab5f9d9887e2f9',
            'bc291b84a320963f'],
  'x': 114,
  'y': 399,
  'w': 1012,
  'h': 202},
 {'id': '39db54868b9406e1',
  'type': 'inject',
  'z': '8df55dba201de6b7',
  'g': '1a3af649fb7e41e9',
  'name': '',
  'props': [{'p': 'payload'}, {'p': 'topic', 'vt': 'str'}],
  'repeat': '',
  'crontab': '',
  'once': False,
  'onceDelay': 0.1,
  'topic': '',
  'payload': '',
  'payloadType': 'date',
  'x': 240,
  'y': 480,
  'wires': [['2cc81e7a4f64eb2a']]},
 {'id': '2cc81e7a4f64eb2a',
  'type': 'function',
  'z': '8df55dba201de6b7',
  'g': '1a3af649fb7e41e9',
  'name': 'function 1',
  'func': '// This formats data correctly for InfluxDB\nmsg.payload = {\n    freq: 9\n};\nreturn msg;',
  'outputs': 1,
  'timeout': 0,
  'noerr': 0,
  'initialize': '',
  'finalize': '',
  'libs': [],
  'x': 420,
  'y': 480,
  'wires': [['98ab5f9d9887e2f9']]},
 {'id': '89a07c737bbe1a55',
  'type': 'influxdb out',
  'z': '8df55dba201de6b7',
  'g': '1a3af649fb7e41e9',
  'influxdb': '6b15b438c5cf6d49',
  'name': '',
  'measurement': 'main_valve_data',
  'precision': '',
  'retentionPolicy': '',
  'database': 'database',
  'precisionV18FluxV20': 'ms',
  'retentionPolicyV18Flux': '',
  'org': 'amudario',
  'bucket': 'main_valve',
  'x': 940,
  'y': 440,
  'wires': []},
 {'id': 'a7a041eaf4f7e270',
  'type': 'debug',
  'z': '8df55dba201de6b7',
  'g': '1a3af649fb7e41e9',
  'name': 'debug 1',
  'active': True,
  'tosidebar': True,
  'console': False,
  'tostatus': False,
  'complete': 'false',
  'statusVal': '',
  'statusType': 'auto',
  'x': 840,
  'y': 500,
  'wires': []},
 {'id': '98ab5f9d9887e2f9',
  'type': 'switch',
  'z': '8df55dba201de6b7',
  'g': '1a3af649fb7e41e9',
  'name': '',
  'property': 'payload.freq',
  'propertyType': 'msg',
  'rules': [{'t': 'btwn', 'v': '10', 'vt': 'num', 'v2': '50', 'v2t': 'num'}, {'t': 'else'}],
  'checkall': 'true',
  'repair': False,
  'outputs': 2,
  'x': 610,
  'y': 480,
  'wires': [['89a07c737bbe1a55', 'a7a041eaf4f7e270'], ['bc291b84a320963f']]},
 {'id': 'bc291b84a320963f',
  'type': 'debug',
  'z': '8df55dba201de6b7',
  'g': '1a3af649fb7e41e9',
  'name': 'debug 2',
  'active': True,
  'tosidebar': True,
  'console': False,
  'tostatus': False,
  'complete': 'false',
  'statusVal': '',
  'statusType': 'auto',
  'x': 840,
  'y': 560,
  'wires': []},
 {'id': '6b15b438c5cf6d49',
  'type': 'influxdb',
  'hostname': '127.0.0.1',
  'port': 8086,
  'protocol': 'http',
  'database': 'database',
  'name': 'Bahmal_agrostar',
  'usetls': False,
  'tls': '',
  'influxdbVersion': '2.0',
  'url': 'http://localhost:8086',
  'timeout': 10,
  'rejectUnauthorized': True},
 {'id': 'ff6a3e894e6bb146',
  'type': 'group',
  'z': '8df55dba201de6b7',
  'style': {'stroke': '#999999',
            'stroke-opacity': '1',
            'fill': 'none',
            'fill-opacity': '1',
            'label': True,
            'label-position': 'nw',
            'color': '#767676'},
  'nodes': ['5cd77201c9cedda0',
            '592f34a506af7cbc',
            'ce93528d6f960ba4',
            'ceb54dc9bb9f3f1b',
            '84e13bd54cdbbc93',
            'b5dc490b4b627d4d',
            '3e2d92b9b9c64485',
            '1d3690f33fd9d797',
            # The subscription plumbing is tagged into this group, so it must be
            # listed here as well: group.nodes and a node's own g have to agree,
            # and tools/test_nodered_flows.js asserts exactly that.
            'mv_subscribe_tick',
            'mv_subscribe'],
  'x': 94,
  'y': 19,
  'w': 1412,
  'h': 262},
 {'id': 'mv_subscribe_tick',
  'type': 'inject',
  'z': '8df55dba201de6b7',
  'g': 'ff6a3e894e6bb146',
  'name': 'Refresh MainValve subscription',
  'props': [{'p': 'payload'}],
  'repeat': '',
  'crontab': '',
  'once': True,
  'onceDelay': 0.1,
  'topic': '',
  'payload': '',
  'payloadType': 'date',
  'x': 170,
  'y': 40,
  'wires': [['mv_subscribe']]},
 {'id': 'mv_subscribe',
  'type': 'function',
  'z': '8df55dba201de6b7',
  'g': 'ff6a3e894e6bb146',
  'name': 'Subscribe MainValve events',
  'func': '// Subscribe dynamically so the application ID and DevEUI stay out of the flow export.\n'
          "// Set IRRIGATION_APP_ID and MAIN_DEV_EUI in this node's Environment tab, or once in\n"
          '// process env for the Node-RED service.\n'
          "const app = String(env.get('IRRIGATION_APP_ID') || '').toLowerCase();\n"
          "const eui = String(env.get('MAIN_DEV_EUI') || '').toLowerCase();\n"
          "const state = Object.assign({}, flow.get('mv_state') || {});\n"
          "const snapshot = {payload:{kind:'state', state:state}};\n"
          'if (!/^[a-f0-9-]{36}$/.test(app) || !/^[a-f0-9]{16}$/.test(eui)) {\n'
          "  node.status({fill:'red',shape:'ring',text:'set IRRIGATION_APP_ID and MAIN_DEV_EUI'});\n"
          "  state.ui_error = 'Set IRRIGATION_APP_ID and MAIN_DEV_EUI in the Node-RED environment, then "
          "redeploy.';\n"
          "  flow.set('mv_state', state);\n"
          '  return [null, snapshot];\n'
          '}\n'
          "const topic = 'application/' + app + '/device/' + eui + '/event/+';\n"
          "state.ui_error = '';\n"
          "flow.set('mv_state', state);\n"
          '// No context guard: it would survive a redeploy, match, and leave the\n'
          '// mqtt in node unsubscribed. The once-per-deploy inject handles idempotence.\n'
          "node.status({fill:'green',shape:'dot',text:topic});\n"
          "return [{action:'subscribe', topic:topic, qos:0}, snapshot];",
  'outputs': 2,
  'timeout': '',
  'noerr': 0,
  'initialize': '',
  'finalize': '',
  'libs': [],
  'x': 400,
  'y': 40,
  'wires': [['5cd77201c9cedda0'], ['ce93528d6f960ba4']]},
 {'id': '5cd77201c9cedda0',
  'type': 'mqtt in',
  'z': '8df55dba201de6b7',
  'g': 'ff6a3e894e6bb146',
  'name': 'ChirpStack events',
  'topic': '',
  'qos': '0',
  'datatype': 'json',
  'broker': 'ae0178f3742ff530',
  'nl': False,
  'rap': True,
  'rh': 0,
  'inputs': 1,
  'x': 230,
  'y': 80,
  'wires': [['592f34a506af7cbc']]},
 {'id': '592f34a506af7cbc',
  'type': 'function',
  'z': '8df55dba201de6b7',
  'g': 'ff6a3e894e6bb146',
  'name': 'Filter and decode MainValve events',
  'func': "const app = String(env.get('IRRIGATION_APP_ID') || '').toLowerCase();\n"
          "const eui = String(env.get('MAIN_DEV_EUI') || '').toLowerCase();\n"
          'if (!/^[a-f0-9-]{36}$/.test(app) || !/^[a-f0-9]{16}$/.test(eui)) { '
          "node.status({fill:'red',shape:'ring',text:'set IRRIGATION_APP_ID and MAIN_DEV_EUI'}); return "
          'null; }\n'
          "const parts = String(msg.topic || '').split('/');\n"
          "if (parts.length !== 6 || parts[0] !== 'application' || parts[2] !== 'device' || parts[4] !== "
          "'event' || parts[1] !== app || parts[3].toLowerCase() !== eui) return null;\n"
          'let event = msg.payload;\n'
          "try { if (Buffer.isBuffer(event)) event = JSON.parse(event.toString('utf8')); else if (typeof "
          "event === 'string') event = JSON.parse(event); } catch (err) { node.warn('Invalid ChirpStack "
          "JSON: ' + err.message); return null; }\n"
          "if (!event || typeof event !== 'object') return null;\n"
          'const kind = parts[5];\n'
          "let s = flow.get('mv_state') || {};\n"
          'let advance = null;\n'
          's.last_event = kind;\n'
          's.last_event_time = new Date().toISOString();\n'
          "if (kind === 'up') {\n"
          '  if (event.fPort !== 31) return null;\n'
          '  let d = event.object;\n'
          '  let b = null;\n'
          "  if (!d || typeof d !== 'object' || !Object.prototype.hasOwnProperty.call(d, 'command_phase')) "
          '{\n'
          "    try { b = Buffer.from(event.data || '', 'base64'); } catch (_) { b = null; }\n"
          '    if (!b || !((b[0] === 1 && b.length === 15) || ((b[0] === 2 || b[0] === 3) && b.length === '
          "18))) { node.warn('MainValve FPort 31 payload is not v1/15 or v2/v3 18 bytes'); return null; }\n"
          '    const u16 = i => (b[i] << 8) | b[i + 1]; const i16 = i => { const v = u16(i); return v >= '
          '0x8000 ? v - 0x10000 : v; }; const flags = b[1];\n'
          '    const pressure100 = b[0] === 3 ? i16(7) : u16(7);\n'
          '    const pressureAvailable = (flags & 2) && (b[0] === 3 ? pressure100 !== -32768 : pressure100 '
          '!== 65535);\n'
          '    const reasons = '
          "['startup','local_command','remote_command','duplicate_command','invalid_command','modbus_error','pressure_interlock','pressure_sensor_error','actuator_busy','movement_timeout','actuator_fault','local_override'];\n"
          '    d = { actuator_online: !!(flags & 1), pressure_valid: !!(flags & 2), rs485_bus_mode: !!(flags '
          '& 4), valve_moving: !!(flags & 8), overpressure: !!(flags & 16), lorawan_active: !!(flags & 32), '
          "class_c_active: !!(flags & 64), reason_code: b[2], reason: reasons[b[2]] || 'unknown', "
          'actual_angle_deg: (flags & 1) && u16(3) !== 65535 ? u16(3) / 10 : null, target_angle_deg: (flags '
          '& 1) && u16(5) !== 65535 ? u16(5) / 10 : null, pressure_bar: pressureAvailable ? pressure100 / '
          '100 : null, actuator_fault_code: (flags & 1) ? u16(9) : null, last_command_id: u16(11) === 65535 '
          "? null : u16(11), actuator_mode: b[13] === 1 ? 'rs485_bus' : b[13] === 0 ? 'analog' : "
          "'unavailable', pressure_temperature_c: (flags & 2) ? b[14] - 40 : null };\n"
          '  }\n'
          '  if (b && b[0] >= 2) {\n'
          "    const phases = ['none','accepted','moving','finished','rejected','failed'];\n"
          '    d.protocol_version = b[0];\n'
          '    d.reported_command_id = b.readUInt16BE(15) === 65535 ? null : b.readUInt16BE(15);\n'
          "    d.command_phase = phases[b[17]] || 'unknown';\n"
          '  } else if (b) {\n'
          '    d.protocol_version = 1;\n'
          '    d.reported_command_id = null;\n'
          "    d.command_phase = 'unavailable';\n"
          '  }\n'
          '  s = Object.assign(s, d); s.last_seen = event.time || new Date().toISOString();\n'
          '  s.f_port = event.fPort; s.f_cnt = event.fCnt == null ? null : event.fCnt;\n'
          '  const rx = Array.isArray(event.rxInfo) ? event.rxInfo : [];\n'
          '  if (rx.length) { const best = rx.reduce((a,b) => Number(b.snr) > Number(a.snr) ? b : a); s.rssi '
          "= best.rssi; s.snr = best.snr; s.gateway_id = best.gatewayId || ''; }\n"
          "  const p = flow.get('mv_pending');\n"
          '  if (p && d.reported_command_id === p.id) {\n'
          "    if (d.command_phase === 'accepted') s.command_state = 'Accepted; waiting for movement';\n"
          "    else if (d.command_phase === 'moving') s.command_state = 'Moving';\n"
          "    else if (d.command_phase === 'finished') {\n"
          '      const wasExpired = s.pending_expired === true;\n'
          "      s.command_state = 'Finished; actual angle ' + d.actual_angle_deg;\n"
          '      s.pending_expired = false;\n'
          "      flow.set('mv_pending',null);\n"
          '      if (wasExpired) s.queue_paused = true;\n'
          "      else advance = {_mvInternal:true,payload:{kind:'advance'}};\n"
          "    } else if (d.command_phase === 'rejected') {\n"
          "      s.command_state = 'Rejected: ' + (d.reason || 'unknown') + '; queue paused';\n"
          '      s.queue_paused = true;\n'
          "      flow.set('mv_pending',null);\n"
          "    } else if (d.command_phase === 'failed') {\n"
          "      s.command_state = 'Failed: ' + (d.reason || 'unknown') + '; inspect before unlocking';\n"
          '      s.pending_expired = true;\n'
          '      s.queue_paused = true;\n'
          '    }\n'
          '  }\n'
          "  node.status({fill:'green',shape:'dot',text:'status ' + s.last_seen});\n"
          "} else if (kind === 'ack') { s.network_ack = event.acknowledged === true ? 'Confirmed downlink "
          "ACK' : 'Downlink not acknowledged'; }\n"
          "else if (kind === 'txack') { s.network_txack = 'Gateway transmission acknowledged'; }\n"
          "else if (kind === 'join') { s.network_join = s.last_event_time; }\n"
          "else if (kind === 'log') { s.network_log = event.description || event.code || 'Device log'; }\n"
          'else return null;\n'
          "s.queue = (flow.get('mv_queue') || []).slice();\n"
          "s.active_command = flow.get('mv_pending') || null;\n"
          "flow.set('mv_state', s);\n"
          "msg.payload = {kind:'state', state:s}; return [msg,advance];",
  'outputs': 2,
  'timeout': '',
  'noerr': 0,
  'initialize': '',
  'finalize': '',
  'libs': [],
  'x': 520,
  'y': 120,
  'wires': [['ce93528d6f960ba4'], ['ceb54dc9bb9f3f1b']]},
 {'id': 'ce93528d6f960ba4',
  'type': 'ui-template',
  'z': '8df55dba201de6b7',
  'g': 'ff6a3e894e6bb146',
  'group': 'ui-group-telemetry',
  'page': '',
  'ui': '',
  'name': 'MainValve controls and telemetry',
  'order': 1,
  'width': '12',
  'height': '13',
  'head': '',
  'format': '<template>\n'
            '<div class="mv">\n'
            '  <div class="mv-head"><strong>MainValve</strong><span>{{ state.last_seen ? \'Last uplink: \' + '
            "local(state.last_seen) : 'Waiting for status uplink' }}</span></div>\n"
            '  <div class="mv-grid">\n'
            "    <div><small>Actual angle</small><b>{{ show(state.actual_angle_deg, ' deg') }}</b></div>\n"
            "    <div><small>Target angle</small><b>{{ show(state.target_angle_deg, ' deg') }}</b></div>\n"
            "    <div><small>Upstream pressure</small><b>{{ show(state.pressure_bar, ' bar') }}</b></div>\n"
            "    <div><small>Temperature</small><b>{{ show(state.pressure_temperature_c, ' C') }}</b></div>\n"
            "    <div><small>Actuator</small><b>{{ state.actuator_online === undefined ? '-' : "
            "state.actuator_online ? 'Online' : 'Offline' }}</b></div>\n"
            "    <div><small>Movement</small><b>{{ state.valve_moving ? 'Moving' : 'Stopped' }}</b></div>\n"
            "    <div><small>LoRaWAN</small><b>{{ state.class_c_active ? 'Class C' : state.lorawan_active ? "
            "'Class A fallback' : 'Unknown / offline' }}</b></div>\n"
            "    <div><small>Signal</small><b>{{ show(state.rssi, ' dBm') }} / {{ show(state.snr, ' dB') "
            '}}</b></div>\n'
            '  </div>\n'
            '  <div class="mv-alert" v-if="state.overpressure || state.actuator_fault_code">{{ '
            "state.overpressure ? 'OVERPRESSURE ' : '' }}{{ state.actuator_fault_code ? 'Actuator fault: ' + "
            "state.actuator_fault_code : '' }}</div>\n"
            '  <div class="mv-controls"><h3>Queue valve positions</h3><p>0 deg = closed; 90 deg = open. New '
            'commands wait in Node-RED until the active movement finishes.</p>\n'
            '    <div class="mv-buttons"><v-btn color="error" @click="issue(0)">Queue close</v-btn><v-btn '
            'color="primary" @click="issue(45)">Queue half</v-btn><v-btn color="success" '
            '@click="issue(90)">Queue open</v-btn></div>\n'
            '    <div class="mv-custom"><v-text-field v-model.number="angle" type="number" min="0" max="90" '
            'step="0.1" label="Target angle (degrees)" density="compact"></v-text-field><v-btn '
            '@click="issue(angle)">Queue angle</v-btn></div>\n'
            '    <div class="mv-custom"><v-text-field v-model.number="percent" type="number" min="0" '
            'max="100" step="0.1" label="Travel percent" density="compact"></v-text-field><v-btn '
            '@click="issuePercent">Queue percent</v-btn></div>\n'
            '  </div>\n'
            '  <div class="mv-queue">\n'
            '    <h3>Command queue <small>({{ queue.length }} waiting)</small></h3>\n'
            '    <div class="mv-active"><b>Active:</b> <span v-if="state.active_command">ID {{ '
            'state.active_command.id }} - {{ state.active_command.angle_deg }} deg ({{ state.command_state '
            '}})</span><span v-else>None</span></div>\n'
            '    <div v-if="state.queue_paused" class="mv-alert">Queue paused. Check the last result before '
            'resuming.<br><v-btn v-if="!state.active_command" @click="resume">Resume queue</v-btn></div>\n'
            '    <ol v-if="queue.length" class="mv-list">\n'
            '      <li v-for="(item,index) in queue" :key="item.queue_id"><span>{{ index + 1 }}. {{ '
            'item.angle_deg }} deg <small>queued {{ local(item.queued_at) }}</small></span><v-btn '
            'icon="mdi-close" size="x-small" variant="text" color="error" title="Remove waiting command" '
            '@click="remove(item.queue_id)"></v-btn></li>\n'
            '    </ol>\n'
            '    <p v-else>No waiting commands.</p>\n'
            '  </div>\n'
            '  <div class="mv-footer">\n'
            "    <div><b>Status reason:</b> {{state.reason || '-'}} | <b>Last accepted ID:</b> "
            '{{show(state.last_command_id)}} | <b>Reported ID:</b> {{show(state.reported_command_id)}} | '
            "<b>Phase:</b> {{state.command_phase || '-'}}</div>\n"
            "    <div><b>RS485:</b> {{state.actuator_mode || '-'}} | <b>Pressure valid:</b> "
            "{{state.pressure_valid ? 'yes' : 'no'}} | <b>FPort:</b> {{show(state.f_port)}} | <b>FCnt:</b> "
            '{{show(state.f_cnt)}}</div>\n'
            '    <div v-if="state.pending_expired" class="mv-alert">Movement failed or no final status '
            'within 5 minutes. Check valve position and ChirpStack events before unlocking.<br><v-btn '
            '@click="unlock">Unlock after inspection</v-btn></div>\n'
            '    <div v-if="state.ui_error" class="mv-alert">{{state.ui_error}}</div>\n'
            '  </div>\n'
            '</div>\n'
            '</template>\n'
            '<script>\n'
            'export default {\n'
            '  data() { return {state:{},angle:45,percent:50} },\n'
            '  computed: {queue() { return Array.isArray(this.state.queue) ? this.state.queue : [] }},\n'
            "  watch: {msg: {handler(m) { if (m && m.payload && m.payload.kind === 'state') this.state = "
            'm.payload.state || {}; }, immediate:true}},\n'
            '  methods: {\n'
            "    local(iso) { if (!iso) return '-'; let s = String(iso); if (s[19] === '.') { let j = 23; "
            "while (j < s.length && s[j] >= '0' && s[j] <= '9') j++; s = s.slice(0, 23) + s.slice(j); } "
            'const d = new Date(s); return isNaN(d.getTime()) ? String(iso) : d.toLocaleString(); },\n'
            "    show(v,suffix='') { return v === null || v === undefined || v === '' ? '-' : String(v) + "
            'suffix },\n'
            '    issue(a) { const n=Number(a); if (!Number.isFinite(n)||n<0||n>90) '
            "{this.state.ui_error='Angle must be 0 to 90 degrees';return;} if (window.confirm('Queue "
            "MainValve target '+n+' degrees?')) this.send({payload:{kind:'command',angle_deg:n}}); },\n"
            '    issuePercent() { const p=Number(this.percent); if (!Number.isFinite(p)||p<0||p>100) '
            "{this.state.ui_error='Percent must be 0 to 100';return;} this.issue(p*0.9); },\n"
            "    remove(id) { this.send({payload:{kind:'remove',queue_id:id}}); },\n"
            "    unlock() { if (window.confirm('I checked the physical valve and ChirpStack queue. Unlock "
            "the active command?')) this.send({payload:{kind:'unlock'}}); },\n"
            "    resume() { if (window.confirm('Resume dispatching waiting commands in order?')) "
            "this.send({payload:{kind:'resume'}}); }\n"
            '  }\n'
            '}\n'
            '</script>\n'
            '<style>\n'
            '.mv{padding:8px}.mv-head{display:flex;justify-content:space-between;gap:12px;align-items:center;font-size:1.15rem}.mv-head '
            'span{font-size:.8rem}.mv-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(145px,1fr));gap:10px;margin:16px '
            '0}.mv-grid>div{padding:12px;border:1px solid #ccd7df;border-radius:8px}.mv-grid '
            'small{display:block;color:#566}.mv-grid '
            'b{font-size:1.3rem}.mv-controls,.mv-queue{border-top:1px solid '
            '#ccd7df;padding-top:12px;margin-top:12px}.mv-controls p,.mv-queue '
            'p{font-size:.85rem}.mv-buttons{display:flex;gap:8px;flex-wrap:wrap}.mv-custom{display:flex;align-items:center;gap:12px;max-width:430px;margin-top:8px}.mv-custom '
            '.v-input{flex:1}.mv-active{padding:8px;background:#eef5f7;border-radius:6px}.mv-list{list-style:none;padding:0}.mv-list '
            'li{display:flex;justify-content:space-between;align-items:center;border-bottom:1px solid '
            '#dde5e8;padding:5px 0}.mv-list '
            'small{display:block;color:#667}.mv-footer{font-size:.84rem;line-height:1.7;border-top:1px solid '
            '#ccd7df;margin-top:12px;padding-top:10px}.mv-alert{background:#b3261e;color:white;border-radius:5px;padding:8px;margin:8px '
            '0}\n'
            '</style>',
  'storeOutMessages': False,
  'passthru': False,
  'resendOnRefresh': True,
  'templateScope': 'local',
  'className': '',
  'x': 860,
  'y': 180,
  'wires': [['ceb54dc9bb9f3f1b']]},
 {'id': 'ceb54dc9bb9f3f1b',
  'type': 'function',
  'z': '8df55dba201de6b7',
  'g': 'ff6a3e894e6bb146',
  'name': 'Queue, remove, and dispatch commands',
  'func': "const app = String(env.get('IRRIGATION_APP_ID') || '').toLowerCase();\n"
          "const eui = String(env.get('MAIN_DEV_EUI') || '').toLowerCase();\n"
          "let s = flow.get('mv_state') || {};\n"
          "let queue = flow.get('mv_queue') || [];\n"
          'const action = msg.payload || {};\n'
          'function snapshot() {\n'
          '  s.queue = queue.slice();\n'
          "  s.active_command = flow.get('mv_pending') || null;\n"
          "  flow.set('mv_state', s);\n"
          "  return {payload:{kind:'state',state:s}};\n"
          '}\n'
          'function feedback(text) { s.ui_error = text; return [null,snapshot()]; }\n'
          'function dispatchNext() {\n'
          "  if (flow.get('mv_pending') || s.queue_paused || queue.length === 0) return [null,snapshot()];\n"
          '  const next = queue.shift();\n'
          "  flow.set('mv_queue', queue);\n"
          "  // One counter per device, in global context, shared with the integrated\n"
          "  // dashboard: MainValve refuses an id it has already taken.\n"
          "  let id = global.get('cmd_next_id_main');\n"
          "  const last = s.last_command_id == null ? null : Number(s.last_command_id);\n"
          "  const usable = Number.isInteger(id) && id >= 0 && id <= 65534;\n"
          "  const ahead = last !== null && ((id - last + 65535) % 65535) !== 0 &&\n"
          "    ((id - last + 65535) % 65535) < 32768;\n"
          "  if (!usable || (last !== null && !ahead)) id = last === null ? 1 : (last + 1) % 65535;\n"
          "  global.set('cmd_next_id_main', (id + 1) % 65535);\n"
          '  const angle10 = Math.round(next.angle_deg * 10);\n'
          '  const bytes = Buffer.from([1,1,angle10 >> 8,angle10 & 255,id >> 8,id & 255]);\n'
          "  flow.set('mv_pending',{id,angle_deg:angle10 / 10,queue_id:next.queue_id,queued_at:new "
          'Date().toISOString()});\n'
          '  s.queued_id = id;\n'
          "  s.command_state = 'Sent; awaiting acceptance';\n"
          '  s.pending_expired = false;\n'
          "  s.ui_error = '';\n"
          "  node.status({fill:'yellow',shape:'dot',text:'active ID '+id+', waiting '+queue.length});\n"
          '  return [{\n'
          "    topic:'application/'+app+'/device/'+eui+'/command/down',\n"
          "    payload:JSON.stringify({devEui:eui,confirmed:false,fPort:30,data:bytes.toString('base64')}),\n"
          "    qos:'0',retain:false\n"
          '  },snapshot()];\n'
          '}\n'
          "if (action.kind === 'advance') {\n"
          '  if (msg._mvInternal !== true) return null;\n'
          '  return dispatchNext();\n'
          '}\n'
          "if (action.kind === 'remove') {\n"
          '  const index = queue.findIndex(x => x.queue_id === action.queue_id);\n'
          "  if (index < 0) return feedback('Waiting command not found; it may already be active.');\n"
          '  queue.splice(index,1);\n'
          "  flow.set('mv_queue',queue);\n"
          "  s.ui_error = '';\n"
          '  return [null,snapshot()];\n'
          '}\n'
          "if (action.kind === 'unlock') {\n"
          "  if (s.pending_expired !== true) return feedback('Unlock is available after failure or "
          "timeout.');\n"
          "  flow.set('mv_pending',null);\n"
          '  s.queue_paused = true;\n'
          '  s.pending_expired = false;\n'
          "  s.command_state = 'Operator unlocked; queue paused';\n"
          "  s.ui_error = '';\n"
          '  return [null,snapshot()];\n'
          '}\n'
          "if (action.kind === 'resume') {\n"
          "  if (flow.get('mv_pending')) return feedback('Wait for the active command or inspect and unlock "
          "it.');\n"
          "  if (!s.queue_paused) return feedback('Queue is already running.');\n"
          '  s.queue_paused = false;\n'
          "  s.ui_error = '';\n"
          '  return dispatchNext();\n'
          '}\n'
          "if (action.kind !== 'command') return null;\n"
          "if (!/^[a-f0-9-]{36}$/.test(app) || !/^[a-f0-9]{16}$/.test(eui)) return feedback('Configure the "
          "ChirpStack application and DevEUI.');\n"
          "if (!s.last_seen || !(s.protocol_version >= 2)) return feedback('Wait for a protocol-v2 or v3 "
          "FPort 31 status uplink.');\n"
          'const angle = Number(action.angle_deg);\n'
          "if (!Number.isFinite(angle) || angle < 0 || angle > 90) return feedback('Angle must be 0 to 90 "
          "degrees.');\n"
          "if (queue.length >= 20) return feedback('Waiting queue is full (20 commands).');\n"
          "const queueId = (flow.get('mv_queue_counter') || 0) + 1;\n"
          "flow.set('mv_queue_counter',queueId);\n"
          'queue.push({queue_id:queueId,angle_deg:Math.round(angle * 10) / 10,queued_at:new '
          'Date().toISOString()});\n'
          "flow.set('mv_queue',queue);\n"
          "s.ui_error = '';\n"
          'return dispatchNext();',
  'outputs': 2,
  'timeout': '',
  'noerr': 0,
  'initialize': '',
  'finalize': '',
  'libs': [],
  'x': 1090,
  'y': 100,
  'wires': [['84e13bd54cdbbc93'], ['ce93528d6f960ba4']]},
 {'id': '84e13bd54cdbbc93',
  'type': 'mqtt out',
  'z': '8df55dba201de6b7',
  'g': 'ff6a3e894e6bb146',
  'name': 'Queue ChirpStack downlink',
  'topic': '',
  'qos': '',
  'retain': '',
  'respTopic': '',
  'contentType': '',
  'userProps': '',
  'correl': '',
  'expiry': '',
  'broker': 'ae0178f3742ff530',
  'x': 1360,
  'y': 200,
  'wires': []},
 {'id': 'b5dc490b4b627d4d',
  'type': 'inject',
  'z': '8df55dba201de6b7',
  'g': 'ff6a3e894e6bb146',
  'name': 'Check command timeout',
  'props': [{'p': 'payload'}],
  'repeat': '10',
  'crontab': '',
  'once': True,
  'onceDelay': 1,
  'topic': '',
  'payload': '',
  'payloadType': 'date',
  'x': 250,
  'y': 240,
  'wires': [['3e2d92b9b9c64485']]},
 {'id': '3e2d92b9b9c64485',
  'type': 'function',
  'z': '8df55dba201de6b7',
  'g': 'ff6a3e894e6bb146',
  'name': 'Show missing final result',
  'func': "const p = flow.get('mv_pending');\n"
          "const s = flow.get('mv_state') || {};\n"
          'if (p && !s.pending_expired && Date.now() - Date.parse(p.queued_at) >= 300000) {\n'
          '  s.pending_expired = true;\n'
          '  s.queue_paused = true;\n'
          "  s.command_state = 'Outcome unknown; inspect valve before unlocking';\n"
          '}\n'
          "s.queue = (flow.get('mv_queue') || []).slice();\n"
          's.active_command = p || null;\n'
          "flow.set('mv_state',s);\n"
          "msg.payload = {kind:'state',state:s};\n"
          'return msg;',
  'outputs': 1,
  'noerr': 0,
  'initialize': '',
  'finalize': '',
  'libs': [],
  'x': 510,
  'y': 240,
  'wires': [['ce93528d6f960ba4', '1d3690f33fd9d797']]},
 {'id': '1d3690f33fd9d797',
  'type': 'debug',
  'z': '8df55dba201de6b7',
  'g': 'ff6a3e894e6bb146',
  'name': 'debug 6',
  'active': False,
  'tosidebar': True,
  'console': False,
  'tostatus': False,
  'complete': 'false',
  'statusVal': '',
  'statusType': 'auto',
  'x': 740,
  'y': 240,
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
 {'id': 'ui-group-telemetry',
  'type': 'ui-group',
  'z': '8df55dba201de6b7',
  'name': 'Main Valve info',
  'page': '46258ab1c2ef86c9',
  'width': '6',
  'height': '1',
  'order': 1,
  'showTitle': True,
  'className': '',
  'visible': 'true',
  'disabled': 'false',
  'groupType': 'default'},
 {'id': '46258ab1c2ef86c9',
  'type': 'ui-page',
  'name': 'Pump and Main Valve',
  'ui': 'f53e93e9ba219e63',
  'path': '/page1',
  'icon': 'mdi-water-pump',
  'layout': 'grid',
  'theme': 'e49416861823a329',
  'breakpoints': [{'name': 'Default', 'px': '0', 'cols': '3'},
                  {'name': 'Tablet', 'px': '576', 'cols': '6'},
                  {'name': 'Small Desktop', 'px': '768', 'cols': '9'},
                  {'name': 'Desktop', 'px': '1024', 'cols': '12'}],
  'order': 1,
  'className': '',
  'visible': 'true',
  'disabled': 'false'},
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
 {'id': 'd7dd9725998e43f7',
  'type': 'global-config',
  'env': [],
  'modules': {'node-red-contrib-influxdb': '0.7.0', '@flowfuse/node-red-dashboard': '1.30.2'}}]


if __name__ == "__main__":
    write_flow(NODES, OUTPUT, indent=2)
