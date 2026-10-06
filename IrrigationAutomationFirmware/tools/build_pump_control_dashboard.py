"""The PumpControl dashboard: pump status, the AUTO/MANUAL selector, command progress and the offline handling.

The imported JSON, examples/PumpControl/include/pump_control_dashboard_flow.json, is a build artifact -- change this
builder and re-run it rather than editing that file:

    python tools/build_pump_control_dashboard.py

The flow carries no ChirpStack identifier: it subscribes dynamically and reads
IRRIGATION_APP_ID and PUMP_DEV_EUI from the Node-RED environment. write_flow refuses a flow that
breaks that rule, tools/test_flow_ids.js checks it across every flow, and
tools/test_pump_control_dashboard.js covers this flow's import
invariants and decoded output.

Every node id and wire below is the live Node-RED workspace's, copied from its export, so an import
updates those nodes in place; an id the workspace does not own is imported as a second copy of the
flow instead. Keep them in step by re-exporting. The shared mqtt-broker node is referenced by id
only, never shipped, and the two outputs of Require fresh Pump status that carry a command or a
refresh must stay clear of Present Pump status, which takes a state with no last_seen_ms as
"offline" and blanks the card.
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from flow_common import write_flow  # noqa: E402

OUTPUT = "examples/PumpControl/include/pump_control_dashboard_flow.json"
DEVICE_ENV = "PUMP_DEV_EUI"

NODES = [{'id': '0239d524654eaef3',
  'type': 'group',
  'z': 'e036313ddf417d8b',
  'style': {'stroke': '#999999',
            'stroke-opacity': '1',
            'fill': 'none',
            'fill-opacity': '1',
            'label': True,
            'label-position': 'nw',
            'color': '#767676'},
  'nodes': ['3c050fdd79c7af0a',
            '8a06a7ba9b6ba73a',
            'd1ccbe2802b37566',
            '5dda9abcb9fba621',
            'dddea5d402b20c3a',
            '901b9f75f8165690',
            '330c2a944fd51214',
            '0f9e522f0e8d897a',
            '2fac3e1847ab5104',
            '2854ec2c2b2e63a8',
            '0776bf5a0d917ed8',
            '828a34a9ad1a8fe9',
            'c46819060b51b164',
            '2ed2f8261690ba82',
            '702e036d0744b7e6',
            'c5e28d3f4a6071b2',
            'effb85639d935a3d'],
  'x': 64,
  'y': 179,
  'w': 1862,
  'h': 342},
 {'id': '3c050fdd79c7af0a',
  'type': 'inject',
  'z': 'e036313ddf417d8b',
  'g': '0239d524654eaef3',
  'name': 'Refresh Pump subscription',
  'props': [{'p': 'payload'}],
  'repeat': '',
  'crontab': '',
  'once': True,
  'onceDelay': '0.5',
  'topic': '',
  'payload': '',
  'payloadType': 'date',
  'x': 230,
  'y': 260,
  'wires': [['8a06a7ba9b6ba73a']]},
 {'id': '8a06a7ba9b6ba73a',
  'type': 'function',
  'z': 'e036313ddf417d8b',
  'g': '0239d524654eaef3',
  'name': 'Subscribe Pump events',
  'func': '// Subscribe dynamically so the application ID and DevEUI stay out of the flow export.\n'
          "// Set IRRIGATION_APP_ID and PUMP_DEV_EUI in this node's Environment tab,\n"
          '// or once in process env for the Node-RED service.\n'
          'const app = String(env.get("IRRIGATION_APP_ID") || "").toLowerCase();\n'
          'const eui = String(env.get("PUMP_DEV_EUI") || "").toLowerCase();\n'
          'if (!/^[a-f0-9-]{36}$/.test(app) || !/^[a-f0-9]{16}$/.test(eui)) {\n'
          '    node.status({ fill: "red", shape: "ring", text: "IRRIGATION_APP_ID / PUMP_DEV_EUI '
          'missing" });\n'
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
  'x': 230,
  'y': 310,
  'wires': [['d1ccbe2802b37566']]},
 {'id': 'd1ccbe2802b37566',
  'type': 'mqtt in',
  'z': 'e036313ddf417d8b',
  'g': '0239d524654eaef3',
  'name': 'Pump ChirpStack uplinks',
  'topic': '',
  'qos': '0',
  'datatype': 'json',
  'broker': 'ae0178f3742ff530',
  'nl': False,
  'rap': True,
  'rh': 0,
  'inputs': 1,
  'x': 630,
  'y': 260,
  'wires': [['2854ec2c2b2e63a8']]},
 {'id': '5dda9abcb9fba621',
  'type': 'function',
  'z': 'e036313ddf417d8b',
  'g': '0239d524654eaef3',
  'name': 'Decode Pump status',
  'func': 'const app = String(env.get("IRRIGATION_APP_ID") || "").toLowerCase();\n'
          'const eui = String(env.get("PUMP_DEV_EUI") || "").toLowerCase();\n'
          'const parts = String(msg.topic || "").split("/");\n'
          'if (parts.length !== 6 || parts[0] !== "application" || parts[1] !== app ||\n'
          '    parts[2] !== "device" || parts[3].toLowerCase() !== eui ||\n'
          '    parts[4] !== "event" || parts[5] !== "up") return null;\n'
          'let event = msg.payload;\n'
          'try {\n'
          '  if (Buffer.isBuffer(event)) event = JSON.parse(event.toString("utf8"));\n'
          '  else if (typeof event === "string") event = JSON.parse(event);\n'
          '} catch (error) {\n'
          '  node.warn("Invalid ChirpStack event JSON: " + error.message);\n'
          '  return null;\n'
          '}\n'
          'if (!event || event.fPort !== 51) return null;\n'
          'let d = event.object;\n'
          'if (!d || typeof d !== "object" || d.command_result === undefined) {\n'
          '  let b;\n'
          '  try { b = Buffer.from(event.data || "", "base64"); } catch (_) { return null; }\n'
          '  if (b.length !== 17 || b[0] !== 1) {\n'
          '    node.warn("Pump status requires protocol v1, FPort 51, 17 bytes");\n'
          '    return null;\n'
          '  }\n'
          '  const u16 = i => (b[i] << 8) | b[i + 1];\n'
          '  const results = ["none", "accepted", "failed", "duplicate", "invalid", '
          '"storage_error"];\n'
          '  const states = ["unknown", "forward", "reverse", "stopped"];\n'
          '  d = {\n'
          '    communication_ok: !!(b[1] & 1),\n'
          '    configuration_valid: !!(b[1] & 2),\n'
          '    running: !!(b[1] & 4),\n'
          '    frequency_armed: !!(b[1] & 8),\n'
          '    lorawan_active: !!(b[1] & 16),\n'
          '    class_c_active: !!(b[1] & 32),\n'
          '    command_result: results[b[2]] || "unknown",\n'
          '    last_command_id: u16(3) === 65535 ? null : u16(3),\n'
          '    commanded_frequency_hz: u16(5) / 100,\n'
          '    actual_frequency_hz: u16(7) / 100,\n'
          '    motor_current_a: u16(9) / 100,\n'
          '    vfd_fault_code: u16(11),\n'
          '    output_voltage_v: u16(13) / 10,\n'
          '    run_state: states[b[15]] || "unknown",\n'
          '    communication_error_code: b[16]\n'
          '  };\n'
          '}\n'
          'const now = Date.now();\n'
          'const s = flow.get("pump_state") || {};\n'
          'Object.assign(s, d);\n'
          's.last_seen = event.time || new Date(now).toISOString();\n'
          's.last_seen_ms = now;\n'
          's.stale = false;\n'
          's.f_port = event.fPort;\n'
          'const rx = Array.isArray(event.rxInfo) ? event.rxInfo : [];\n'
          'if (rx.length) {\n'
          '  const best = rx.reduce((a, b) => Number(b.snr) > Number(a.snr) ? b : a);\n'
          '  s.rssi = best.rssi;\n'
          '  s.snr = best.snr;\n'
          '}\n'
          'const reportedId = d.last_command_id == null ? NaN : Number(d.last_command_id);\n'
          'const next = flow.get("pump_next_id");\n'
          'if (Number.isInteger(reportedId) && reportedId >= 0 && reportedId <= 65534) {\n'
          '  const expected = (reportedId + 1) % 65535;\n'
          '  if (!Number.isInteger(next) || next < 0 || next > 65534 ||\n'
          '      (next !== expected && ((expected - next + 65535) % 65535) < 32767)) {\n'
          '    flow.set("pump_next_id", expected);\n'
          '  }\n'
          '} else if (!Number.isInteger(next)) {\n'
          '  flow.set("pump_next_id", 0);\n'
          '}\n'
          'const pending = flow.get("pump_pending");\n'
          'if (pending && Number.isInteger(reportedId) && reportedId === pending.id) {\n'
          '  s.command_state = d.command_result;\n'
          '  s.last_command = pending;\n'
          '  flow.set("pump_pending", null);\n'
          '}\n'
          's.pending = flow.get("pump_pending") || null;\n'
          'flow.set("pump_state", s);\n'
          'msg.payload = {kind: "state", state: s};\n'
          'return msg;',
  'outputs': 1,
  'timeout': 0,
  'noerr': 0,
  'initialize': '',
  'finalize': '',
  'libs': [],
  'x': 880,
  'y': 260,
  'wires': [['effb85639d935a3d']]},
 {'id': 'dddea5d402b20c3a',
  'type': 'inject',
  'z': 'e036313ddf417d8b',
  'g': '0239d524654eaef3',
  'name': 'Refresh status and timeout',
  'props': [{'p': 'payload'}],
  'repeat': '15',
  'crontab': '',
  'once': True,
  'onceDelay': 1,
  'topic': '',
  'payload': '',
  'payloadType': 'date',
  'x': 620,
  'y': 400,
  'wires': [['901b9f75f8165690']]},
 {'id': '901b9f75f8165690',
  'type': 'function',
  'z': 'e036313ddf417d8b',
  'g': '0239d524654eaef3',
  'name': 'Check stale status',
  'func': 'const s = flow.get("pump_state") || {};\n'
          'const now = Date.now();\n'
          's.stale = !s.last_seen_ms || now - s.last_seen_ms > 150000;\n'
          'const pending = flow.get("pump_pending");\n'
          'if (pending && now - pending.queued_at_ms > 150000) {\n'
          '  s.command_state = "no device confirmation";\n'
          '  s.last_command = pending;\n'
          '  s.pending = null;\n'
          '  flow.set("pump_pending", null);\n'
          '} else {\n'
          '  s.pending = pending || null;\n'
          '}\n'
          'flow.set("pump_state", s);\n'
          'msg.payload = {kind: "state", state: s};\n'
          'return msg;',
  'outputs': 1,
  'timeout': 0,
  'noerr': 0,
  'initialize': '',
  'finalize': '',
  'libs': [],
  'x': 880,
  'y': 400,
  'wires': [['828a34a9ad1a8fe9']]},
 {'id': '330c2a944fd51214',
  'type': 'ui-template',
  'z': 'e036313ddf417d8b',
  'g': '0239d524654eaef3',
  'group': 'c6dc5eedbdc49f18',
  'page': '',
  'ui': '',
  'name': 'Pump controls and status',
  'order': 1,
  'width': '12',
  'height': '11',
  'head': '',
  'format': '<template>\n'
            '<div class="pump">\n'
            '  <div class="pump-head">\n'
            '    <div class="pump-title"><strong>Pump Control</strong><small>Grandfar pump / '
            'DELIXI VFD</small><div class="pump-radio-inline"><span :class="radioClass">{{ '
            'radioText }}</span><v-btn icon="mdi-refresh" size="x-small" variant="text" '
            ':loading="refreshing" :disabled="refreshing || cooldown > 0" :title="refreshTitle" '
            '@click="requestStatus"></v-btn></div></div>\n'
            '    <div class="pump-status">\n'
            '      <span class="pump-pill" :class="state.stale ? \'stale\' : state.running ? '
            '\'running\' : \'stopped\'">{{ state.stale ? \'No recent status\' : state.run_state '
            "=== 'unknown' ? 'Unknown' : state.running ? 'Running' : 'Stopped' }}</span>\n"
            '      <span class="pump-mode-pill" :class="manualModeClass">{{ manualModeText '
            '}}</span>\n'
            '    </div>\n'
            '  </div>\n'
            '  <div class="pump-grid">\n'
            "    <div><small>Actual frequency</small><b>{{ show(state.actual_frequency_hz, ' Hz') "
            '}}</b></div>\n'
            "    <div><small>Commanded frequency</small><b>{{ show(state.commanded_frequency_hz, ' "
            "Hz') }}</b></div>\n"
            "    <div><small>Motor current</small><b>{{ show(state.motor_current_a, ' A') "
            '}}</b></div>\n'
            "    <div><small>VFD communication</small><b>{{ state.stale ? 'Unknown' : "
            "state.communication_ok ? 'OK' : 'Error' }}</b></div>\n"
            '    <div><small>VFD fault</small><b>{{ show(state.vfd_fault_code) }}</b></div>\n'
            '  </div>\n'
            '  <div class="pump-alert" v-if="!state.stale && (state.vfd_fault_code || '
            'state.communication_ok === false)">VFD fault or communication problem. Check the pump '
            'before starting.</div>\n'
            '  <div class="pump-controls">\n'
            '    <h3>Controls</h3>\n'
            '    <div class="pump-frequency"><v-text-field v-model.number="frequency" '
            'type="number" min="10" max="50" step="0.1" label="Frequency (Hz)" density="compact" '
            'hide-details></v-text-field><v-btn color="primary" '
            '@click="issue(\'set_frequency\')">Set frequency</v-btn></div>\n'
            '    <div class="pump-buttons"><v-btn color="success" '
            '@click="issue(\'start\')">Start</v-btn><v-btn color="error" '
            '@click="issue(\'stop\')">Stop</v-btn></div>\n'
            '    <small>Start requires a valid VFD configuration and an explicit frequency command '
            'after boot.</small>\n'
            '  </div>\n'
            '  <div class="pump-feedback">\n'
            '    <h3>Command feedback</h3>\n'
            '    <div v-if="state.pending">ID {{ state.pending.id }} · {{ '
            'label(state.pending.command) }} · Queued {{ state.pending.queued_at }}</div>\n'
            '    <div v-else-if="state.last_command">ID {{ state.last_command.id }} · {{ '
            "label(state.last_command.command) }} · {{ state.command_state || 'Unknown' }}</div>\n"
            '    <div v-else>No command sent from this dashboard.</div>\n'
            '    <small>Queued means sent to ChirpStack. Device status confirms the '
            'result.</small>\n'
            '  </div>\n'
            '  <div class="pump-footer"><span>Last uplink: {{ state.last_seen || \'waiting\' '
            "}}</span><span>Configuration: {{ state.stale ? 'unknown' : state.configuration_valid "
            "? 'valid' : 'blocked' }} · Frequency armed: {{ state.stale ? 'unknown' : "
            "state.frequency_armed ? 'yes' : 'no' }}</span><span>Signal: {{ show(state.rssi, ' "
            "dBm') }} / {{ show(state.snr, ' dB') }}</span></div>\n"
            '  <div class="pump-alert" v-if="state.ui_error">{{ state.ui_error }}</div>\n'
            '</div>\n'
            '</template>\n'
            '<script>\n'
            'export default {\n'
            '  data() { return {state:{stale:true}, frequency:25, now:Date.now(), '
            'refreshReadyAt:0, timer:null} },\n'
            '  computed: {\n'
            '    cooldown() { return Math.max(0,Math.ceil((this.refreshReadyAt-this.now)/1000)) '
            '},\n'
            '    refreshing() { return !!this.state.refresh_pending && '
            'this.now-Number(this.state.refresh_requested_at_ms||0)<30000 },\n'
            "    refreshTitle() { return this.refreshing ? 'Requesting status' : this.cooldown ? "
            "'Refresh available in '+this.cooldown+' s' : 'Refresh status' },\n"
            "    radioText() { return this.state.stale ? 'No recent status' : "
            "this.state.class_c_active ? 'Class C active' : this.state.lorawan_active ? 'LoRaWAN "
            "active · Class A fallback' : 'Join not confirmed' },\n"
            "    radioClass() { return this.state.stale ? 'pump-radio-state offline' : "
            "this.state.class_c_active ? 'pump-radio-state online' : 'pump-radio-state fallback' "
            '},\n'
            '    manualModeText() { return this.state.stale || typeof this.state.manual_mode !== '
            "'boolean' ? 'MODE UNKNOWN' : this.state.manual_mode ? 'MANUAL' : 'AUTO' },\n"
            '    manualModeClass() { return this.state.stale || typeof this.state.manual_mode !== '
            "'boolean' ? 'unknown' : this.state.manual_mode ? 'manual' : 'auto' }\n"
            '  },\n'
            "  watch: {msg: {handler(m) { if (m && m.payload && m.payload.kind === 'state') "
            'this.state = m.payload.state || {}; }, immediate:true}},\n'
            '  mounted() { this.timer=setInterval(()=>{this.now=Date.now();},500); },\n'
            '  beforeUnmount() { if(this.timer) clearInterval(this.timer); },\n'
            '  methods: {\n'
            "    show(v,suffix='') { return v === null || v === undefined || v === '' ? '-' : "
            'String(v) + suffix },\n'
            "    label(v) { return ({set_frequency:'Set frequency',start:'Start',stop:'Stop'})[v] "
            '|| v },\n'
            '    issue(command) {\n'
            "      if (command === 'start' && !window.confirm('Start the pump?')) return;\n"
            "      this.send({payload:{kind:'command',command:command,frequency_hz:command === "
            "'set_frequency' ? Number(this.frequency) : undefined}});\n"
            '    },\n'
            '    requestStatus() { '
            "if(this.refreshing||this.cooldown)return;this.refreshReadyAt=Date.now()+10000;this.send({payload:{kind:'refresh'}}); "
            '}\n'
            '  }\n'
            '}\n'
            '</script>\n'
            '<style>\n'
            '.pump{padding:8px}.pump-head{display:flex;justify-content:space-between;align-items:flex-start;gap:12px;font-size:1.15rem}.pump-title>small{display:block;color:#667;font-size:.8rem}.pump-radio-inline{display:flex;align-items:center;gap:2px;margin-top:2px;min-height:24px}.pump-radio-state{font-size:.75rem;font-weight:600}.pump-radio-state.online{color:#166534}.pump-radio-state.offline{color:#b3261e}.pump-radio-state.fallback{color:#92400e}.pump-status{display:flex;flex-direction:column;align-items:flex-end;gap:4px}.pump-pill{padding:6px '
            '12px;border-radius:20px;background:#e5e7eb;font-size:.85rem}.pump-pill.running{background:#dcfce7;color:#166534}.pump-pill.stopped{background:#e6f0f5;color:#244b61}.pump-pill.stale{background:#fef3c7;color:#92400e}.pump-mode-pill{padding:3px '
            '8px;border-radius:20px;font-size:.68rem;font-weight:700;letter-spacing:.05em}.pump-mode-pill.auto{background:#dcfce7;color:#166534}.pump-mode-pill.manual{background:#ffedd5;color:#9a3412}.pump-mode-pill.unknown{background:#e5e7eb;color:#64748b}.pump-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(145px,1fr));gap:10px;margin:16px '
            '0}.pump-grid>div{padding:12px;border:1px solid #ccd7df;border-radius:8px}.pump-grid '
            'small{display:block;color:#566}.pump-grid '
            'b{font-size:1.3rem}.pump-controls,.pump-feedback,.pump-footer{border-top:1px solid '
            '#ccd7df;padding-top:12px;margin-top:12px}.pump-controls h3,.pump-feedback h3{margin:0 '
            '0 '
            '10px}.pump-frequency{display:flex;align-items:center;gap:10px;max-width:440px}.pump-frequency '
            '.v-input{flex:1}.pump-buttons{display:flex;gap:8px;margin:12px 0}.pump-controls '
            'small,.pump-feedback '
            'small,.pump-footer{color:#566;font-size:.82rem}.pump-footer{display:flex;flex-wrap:wrap;gap:8px '
            '20px}.pump-alert{background:#b3261e;color:white;border-radius:5px;padding:8px;margin:8px '
            '0}\n'
            '</style>',
  'storeOutMessages': False,
  'passthru': False,
  'resendOnRefresh': True,
  'templateScope': 'local',
  'className': '',
  'x': 1150,
  'y': 320,
  'wires': [['c46819060b51b164']]},
 {'id': '0f9e522f0e8d897a',
  'type': 'function',
  'z': 'e036313ddf417d8b',
  'g': '0239d524654eaef3',
  'name': 'Send Pump command',
  'func': 'const app = String(env.get("IRRIGATION_APP_ID") || "").toLowerCase();\n'
          'const eui = String(env.get("PUMP_DEV_EUI") || "").toLowerCase();\n'
          'const action = msg.payload || {};\n'
          'if (action.kind !== "command") return null;\n'
          'const s = flow.get("pump_state") || {};\n'
          'function reply(error) {\n'
          '  s.ui_error = error;\n'
          '  flow.set("pump_state", s);\n'
          '  return [null, {payload: {kind: "state", state: s}}];\n'
          '}\n'
          'const operations = {stop: 1, estop: 2, set_frequency: 3, start: 4};\n'
          'const op = operations[action.command];\n'
          'if (!op) return reply("Unknown pump command.");\n'
          'let arg = 0;\n'
          'if (op === 3) {\n'
          '  const hz = Number(action.frequency_hz);\n'
          '  if (!Number.isFinite(hz) || hz < 10 || hz > 50)\n'
          '    return reply("Frequency must be 10 to 50 Hz.");\n'
          '  arg = Math.round(hz * 100);\n'
          '}\n'
          'const pending = flow.get("pump_pending");\n'
          'if (pending && op !== 1) return reply("Wait for the current command result. Stop '
          'remains available.");\n'
          'if (!s.last_seen_ms) return reply("Wait for a Pump status uplink before sending '
          'commands.");\n'
          'if (op !== 1 && Date.now() - s.last_seen_ms > 150000)\n'
          '  return reply("Pump status is stale. Wait for a new uplink.");\n'
          'const id = flow.get("pump_next_id");\n'
          'if (!Number.isInteger(id) || id < 0 || id > 65534)\n'
          '  return reply("Wait for a status uplink to synchronize command IDs.");\n'
          'flow.set("pump_next_id", (id + 1) % 65535);\n'
          'const bytes = Buffer.from([1, op, id >> 8, id & 255, arg >> 8, arg & 255]);\n'
          'const current = {id, command: action.command, frequency_hz: op === 3 ? arg / 100 : '
          'null,\n'
          '  queued_at: new Date().toISOString(), queued_at_ms: Date.now()};\n'
          'flow.set("pump_pending", current);\n'
          's.pending = current;\n'
          's.last_command = current;\n'
          's.command_state = "queued";\n'
          's.ui_error = pending && op === 1\n'
          '  ? "Stop queued. A previous downlink may still be waiting in ChirpStack."\n'
          '  : "";\n'
          'flow.set("pump_state", s);\n'
          'return [{\n'
          '  topic: "application/" + app + "/device/" + eui + "/command/down",\n'
          '  payload: JSON.stringify({devEui: eui, confirmed: false, fPort: 50,\n'
          '    data: bytes.toString("base64")}),\n'
          '  qos: "0",\n'
          '  retain: false\n'
          '}, {payload: {kind: "state", state: s}}];',
  'outputs': 2,
  'timeout': 0,
  'noerr': 0,
  'initialize': '',
  'finalize': '',
  'libs': [],
  'x': 1460,
  'y': 260,
  'wires': [['2fac3e1847ab5104'], ['828a34a9ad1a8fe9']]},
 {'id': '2fac3e1847ab5104',
  'type': 'mqtt out',
  'z': 'e036313ddf417d8b',
  'g': '0239d524654eaef3',
  'name': 'Queue Pump downlink',
  'topic': '',
  'qos': '',
  'retain': '',
  'respTopic': '',
  'contentType': '',
  'userProps': '',
  'correl': '',
  'expiry': '',
  'broker': 'ae0178f3742ff530',
  'x': 1800,
  'y': 300,
  'wires': []},
 {'id': '2854ec2c2b2e63a8',
  'type': 'function',
  'z': 'e036313ddf417d8b',
  'g': '0239d524654eaef3',
  'name': 'Pump protocol v2 compatibility',
  'func': 'let event = msg.payload;\n'
          'try {\n'
          '  if (Buffer.isBuffer(event)) event = JSON.parse(event.toString("utf8"));\n'
          '  else if (typeof event === "string") event = JSON.parse(event);\n'
          '} catch (_) { return msg; }\n'
          'if (!event || event.fPort !== 51) return msg;\n'
          'if (!event.object || typeof event.object !== "object" || event.object.command_result '
          '=== undefined) {\n'
          '  let b;\n'
          '  try { b = Buffer.from(event.data || "", "base64"); } catch (_) { return msg; }\n'
          '  if (b.length === 22 && b[0] === 2) {\n'
          '    const u16 = i => (b[i] << 8) | b[i + 1];\n'
          '    const s16 = i => { const v = u16(i); return v >= 32768 ? v - 65536 : v; };\n'
          '    const results = '
          '["none","accepted","failed","duplicate","invalid","storage_error"];\n'
          '    const states = ["unknown","forward","reverse","stopped"];\n'
          '    const joinError = s16(17);\n'
          '    event.object = '
          '{communication_ok:!!(b[1]&1),configuration_valid:!!(b[1]&2),running:!!(b[1]&4),frequency_armed:!!(b[1]&8),lorawan_active:!!(b[1]&16),class_c_active:!!(b[1]&32),command_result:results[b[2]]||"unknown",last_command_id:u16(3)===65535?null:u16(3),commanded_frequency_hz:u16(5)/100,actual_frequency_hz:u16(7)/100,motor_current_a:u16(9)/100,vfd_fault_code:u16(11),output_voltage_v:u16(13)/10,run_state:states[b[15]]||"unknown",communication_error_code:b[16],previous_join_error_code:joinError,previous_join_error:joinError===-1116?"No '
          'OTAA JoinAccept received in RX1/RX2":joinError===0?"None":"RadioLib error '
          '"+joinError,join_attempt_count:b[19],previous_join_retry_seconds:u16(20)};\n'
          '    msg.payload = event;\n'
          '  }\n'
          '}\n'
          'const s = flow.get("pump_state") || {};\n'
          'if (s.refresh_pending) {\n'
          '  s.refresh_pending = false;\n'
          '  s.refresh_result = "Status received";\n'
          '  s.refresh_completed_at = new Date().toISOString();\n'
          '  flow.set("pump_state", s);\n'
          '}\n'
          'return msg;',
  'outputs': 1,
  'timeout': 0,
  'noerr': 0,
  'initialize': '',
  'finalize': '',
  'libs': [],
  'x': 870,
  'y': 220,
  'wires': [['702e036d0744b7e6']]},
 {'id': '0776bf5a0d917ed8',
  'type': 'function',
  'z': 'e036313ddf417d8b',
  'g': '0239d524654eaef3',
  'name': 'Request Pump status',
  'func': 'const app=String(env.get("IRRIGATION_APP_ID") || "").toLowerCase();\n'
          'const eui=String(env.get("PUMP_DEV_EUI") || "").toLowerCase();\n'
          'const action=msg.payload||{};\n'
          'if(action.kind!=="refresh")return null;\n'
          'const now=Date.now();\n'
          'const last=Number(flow.get("pump_last_refresh_ms")||0);\n'
          'const s=flow.get("pump_state")||{};\n'
          'if(now-last<10000){s.refresh_result="Refresh rate limit: try again in '
          '"+Math.ceil((10000-(now-last))/1000)+" '
          's";flow.set("pump_state",s);return[null,{payload:{kind:"state",state:s}}];}\n'
          'if(s.refresh_pending&&now-Number(s.refresh_requested_at_ms||0)<30000){s.refresh_result="A '
          'status request is already '
          'pending.";flow.set("pump_state",s);return[null,{payload:{kind:"state",state:s}}];}\n'
          'flow.set("pump_last_refresh_ms",now);\n'
          's.refresh_pending=true;s.refresh_requested_at_ms=now;s.refresh_requested_at=new '
          'Date(now).toISOString();s.refresh_result="Requesting…";flow.set("pump_state",s);\n'
          'return[{topic:"application/"+app+"/device/"+eui+"/command/down",payload:JSON.stringify({devEui:eui,confirmed:false,fPort:50,data:Buffer.from([1,5]).toString("base64")}),qos:"0",retain:false},{payload:{kind:"state",state:s}}];',
  'outputs': 2,
  'timeout': 0,
  'noerr': 0,
  'initialize': '',
  'finalize': '',
  'libs': [],
  'x': 1480,
  'y': 480,
  'wires': [['2fac3e1847ab5104'], ['828a34a9ad1a8fe9']]},
 {'id': '828a34a9ad1a8fe9',
  'type': 'function',
  'z': 'e036313ddf417d8b',
  'g': '0239d524654eaef3',
  'name': 'Present Pump status',
  'func': '// The router passes a command or a refresh request through unchanged, so those\n'
          '// messages carry no state. Building one from nothing produced an empty state\n'
          '// that Apply Pump offline state read as "no last_seen_ms", reported the node\n'
          '// offline and blanked every tile until the next uplink.\n'
          'const source=msg.payload&&msg.payload.state;\n'
          'if(!source||typeof source!=="object"||!Object.keys(source).length)return null;\n'
          'const s=Object.assign({},source);\n'
          'function readableTime(value){\n'
          '  if(!value)return value;\n'
          '  const normalized=String(value).replace(/(\\.\\d{3})\\d+/,"$1");\n'
          '  const date=new Date(normalized);\n'
          '  if(Number.isNaN(date.getTime()))return value;\n'
          '  const parts=new '
          'Intl.DateTimeFormat("en-GB",{timeZone:"Asia/Tashkent",year:"numeric",month:"2-digit",day:"2-digit",hour:"2-digit",minute:"2-digit",second:"2-digit",hour12:false}).formatToParts(date);\n'
          '  const p=Object.fromEntries(parts.map(x=>[x.type,x.value]));\n'
          '  return p.day+"/"+p.month+"/"+p.year+" "+p.hour+":"+p.minute+":"+p.second;\n'
          '}\n'
          's.last_seen=readableTime(s.last_seen);\n'
          'if(s.pending)s.pending=Object.assign({},s.pending,{queued_at:readableTime(s.pending.queued_at)});\n'
          'if(s.last_command)s.last_command=Object.assign({},s.last_command,{queued_at:readableTime(s.last_command.queued_at)});\n'
          'msg.payload={kind:"state",state:s};\n'
          'return msg;',
  'outputs': 1,
  'timeout': 0,
  'noerr': 0,
  'initialize': '',
  'finalize': '',
  'libs': [],
  'x': 1090,
  'y': 380,
  'wires': [['2ed2f8261690ba82']]},
 {'id': 'c46819060b51b164',
  'type': 'function',
  'z': 'e036313ddf417d8b',
  'g': '0239d524654eaef3',
  'name': 'Require fresh Pump status',
  'func': 'const action=msg.payload||{};\n'
          'if(action.kind==="refresh")return[null,msg,null];\n'
          'const s=flow.get("pump_state")||{};\n'
          'const offlineAfterMs=s.running?45000:75000;\n'
          'if(action.kind==="command"&&action.command!=="stop"&&(!s.last_seen_ms||Date.now()-s.last_seen_ms>offlineAfterMs)){\n'
          '  s.ui_error="Pump status is offline. Wait for a fresh uplink. Stop remains '
          'available.";\n'
          '  flow.set("pump_state",s);\n'
          '  return[null,null,{payload:{kind:"state",state:s}}];\n'
          '}\n'
          'return[msg,null,null];',
  'outputs': 3,
  'timeout': 0,
  'noerr': 0,
  'initialize': '',
  'finalize': '',
  'libs': [],
  'x': 1400,
  'y': 340,
  'wires': [['0f9e522f0e8d897a'], ['0776bf5a0d917ed8'], ['828a34a9ad1a8fe9']]},
 {'id': '2ed2f8261690ba82',
  'type': 'function',
  'z': 'e036313ddf417d8b',
  'g': '0239d524654eaef3',
  'name': 'Apply Pump offline state',
  'func': 'const source=(msg.payload&&msg.payload.state)||{};\n'
          'const s=Object.assign({},source);\n'
          'const offlineAfterMs=source.running?45000:75000;\n'
          's.offline_after_seconds=offlineAfterMs/1000;\n'
          's.stale=!s.last_seen_ms||Date.now()-s.last_seen_ms>offlineAfterMs;\n'
          'if(s.stale){\n'
          '  s.class_c_active=false;\n'
          '  s.lorawan_active=false;\n'
          '  s.communication_ok=null;\n'
          '  s.vfd_fault_code=null;\n'
          '}\n'
          'msg.payload={kind:"state",state:s};\n'
          'return msg;',
  'outputs': 1,
  'timeout': 0,
  'noerr': 0,
  'initialize': '',
  'finalize': '',
  'libs': [],
  'x': 1450,
  'y': 420,
  'wires': [['330c2a944fd51214']]},
 {'id': '702e036d0744b7e6',
  'type': 'function',
  'z': 'e036313ddf417d8b',
  'g': '0239d524654eaef3',
  'name': 'Normalize Pump command progress',
  'func': 'const event=msg.payload;\n'
          'if(event&&event.fPort===51&&event.object&&event.object.command_result==="unknown"){\n'
          '  try{const '
          'b=Buffer.from(event.data||"","base64");if(b.length>=3&&b[2]===6)event.object.command_result="in_progress";}catch(_){}\n'
          '}\n'
          'return msg;',
  'outputs': 1,
  'timeout': 0,
  'noerr': 0,
  'initialize': '',
  'finalize': '',
  'libs': [],
  'x': 1050,
  'y': 220,
  'wires': [['c5e28d3f4a6071b2']]},
 {'id': 'c5e28d3f4a6071b2',
  'type': 'function',
  'z': 'e036313ddf417d8b',
  'g': '0239d524654eaef3',
  'name': 'Decode Pump AUTO/MANUAL state',
  'func': 'let event = msg.payload;\n'
          'try {\n'
          '  if (Buffer.isBuffer(event)) event = JSON.parse(event.toString("utf8"));\n'
          '  else if (typeof event === "string") event = JSON.parse(event);\n'
          '} catch (_) { return msg; }\n'
          'if (!event || event.fPort !== 51) return msg;\n'
          'let bytes;\n'
          'try { bytes = Buffer.from(event.data || "", "base64"); } catch (_) { return msg; }\n'
          'if (bytes.length === 22 && bytes[0] === 2) {\n'
          '  if (!event.object || typeof event.object !== "object") event.object = {};\n'
          '  event.object.manual_mode = !!(bytes[1] & 64);\n'
          '  msg.payload = event;\n'
          '}\n'
          'return msg;',
  'outputs': 1,
  'timeout': 0,
  'noerr': 0,
  'initialize': '',
  'finalize': '',
  'libs': [],
  'x': 1050,
  'y': 250,
  'wires': [['5dda9abcb9fba621']]},
 {'id': 'effb85639d935a3d',
  'type': 'function',
  'z': 'e036313ddf417d8b',
  'g': '0239d524654eaef3',
  'name': 'Track Pump command completion',
  'func': 'const s=(msg.payload&&msg.payload.state)||{};\n'
          'if(s.command_result==="in_progress"&&s.last_command){\n'
          '  s.command_state="in progress: waiting for final VFD condition";\n'
          '  s.pending=s.last_command;\n'
          '  flow.set("pump_pending",s.last_command);\n'
          '  flow.set("pump_state",s);\n'
          '}\n'
          'return msg;',
  'outputs': 1,
  'timeout': 0,
  'noerr': 0,
  'initialize': '',
  'finalize': '',
  'libs': [],
  'x': 1070,
  'y': 280,
  'wires': [['828a34a9ad1a8fe9']]},
 {'id': 'c6dc5eedbdc49f18',
  'type': 'ui-group',
  'z': 'e036313ddf417d8b',
  'name': 'Water Pump',
  'page': '46258ab1c2ef86c9',
  'width': '6',
  'height': '8',
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
  'order': 5,
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
            'widgetGap': '12px'}}]

if __name__ == "__main__":
    write_flow(NODES, OUTPUT, indent=2)
