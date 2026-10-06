// Checks for pump_control_dashboard_flow.json.
//
//     node tools/test_pump_control_dashboard.js
//
// Regenerate the flow with tools/build_pump_control_dashboard.py after changing
// the builder. The decoder is run against both the ChirpStack object and the
// seventeen raw protocol-v1 bytes it falls back to, and the command builder
// against the FPort 50 downlink it publishes.
const c = require('./flow_test_common.js');

const flow = c.loadFlow('PumpControl/include/pump_control_dashboard_flow.json');
const byId = new Map(flow.map(n => [n.id, n]));
const APP = c.APP_ID;
const EUI = '0000000000000006';
const ENV = { IRRIGATION_APP_ID: APP, PUMP_DEV_EUI: EUI };
const UP = 'application/' + APP + '/device/' + EUI + '/event/up';

const DECODE = byId.get('3feff66c35d116bc');
const SEND = byId.get('379f6c997142081b');

c.checkImportInvariants(flow, { name: 'pump_control' });

const decode = (payload, topic) => {
  const first = c.messages(c.runNode(DECODE.func,
    { env: ENV, msg: { topic: topic || UP, payload } }).result)[0];
  return first && first.payload ? first.payload.state : undefined;
};
const statusEvent = (extra) => Object.assign({
  fPort: 51, fCnt: 30, time: '2026-10-05T13:26:58.759892249+00:00',
  rxInfo: [{ rssi: -68, snr: 8, gatewayId: 'aabbccddeeff0011' }],
}, extra);

// --- the decoded object ----------------------------------------------------
const state = decode(statusEvent({ object: {
  command_result: 'accepted', last_command_id: 5, running: true, configuration_valid: true,
  actual_frequency_hz: 35.5, motor_current_a: 2.4, run_state: 'forward',
} }));
if (!state) c.fail('the FPort 51 status did not reach the card');
else {
  for (const [key, value] of Object.entries({
    command_result: 'accepted', last_command_id: 5, running: true,
    actual_frequency_hz: 35.5, motor_current_a: 2.4, run_state: 'forward',
  })) {
    if (state[key] !== value) {
      c.fail('state.' + key + ' is ' + JSON.stringify(state[key]) + ', expected ' + JSON.stringify(value));
    }
  }
  if (state.stale !== false) c.fail('a fresh status is still marked stale');
  if (state.f_port !== 51) c.fail('f_port is ' + state.f_port);
  if (state.rssi !== -68 || state.snr !== 8) c.fail('signal strength not taken from rxInfo');
}
c.done('pump_control: the FPort 51 object reaches the card');

// --- the raw protocol-v1 fallback -----------------------------------------
const v1 = Buffer.from([1, 0x03, 1, 0, 5, 0x0D, 0xDE, 0x0D, 0xAC, 0x00, 0xF0,
  0, 0, 0x00, 0xE6, 1, 0]);
const raw = decode(statusEvent({ data: v1.toString('base64') }));
if (!raw) c.fail('the seventeen-byte protocol-v1 status was dropped');
else {
  for (const [key, value] of Object.entries({
    communication_ok: true, configuration_valid: true, running: false,
    command_result: 'accepted', last_command_id: 5, commanded_frequency_hz: 35.5,
    actual_frequency_hz: 35, motor_current_a: 2.4, vfd_fault_code: 0,
    output_voltage_v: 23, run_state: 'forward', communication_error_code: 0,
  })) {
    if (raw[key] !== value) {
      c.fail('raw v1 state.' + key + ' is ' + JSON.stringify(raw[key]) + ', expected ' + JSON.stringify(value));
    }
  }
}
const short = decode(statusEvent({ data: v1.slice(0, 16).toString('base64') }));
if (short) c.fail('a sixteen-byte status was accepted as protocol v1');
c.done('pump_control: the raw protocol-v1 fallback decodes and a short frame is refused');

if (decode(statusEvent({ object: { command_result: 'accepted' } }),
  'application/' + APP + '/device/0000000000000000/event/up')) {
  c.fail('an uplink from another DevEUI reached the card');
}
if (decode(statusEvent({ object: { command_result: 'accepted' } }), UP.replace('/up', '/join'))) {
  c.fail('a non-uplink event reached the card');
}
c.done('pump_control: another DevEUI and a non-uplink event are filtered out');

// --- commands --------------------------------------------------------------
function send(action, state, pending) {
  return c.runNode(SEND.func, {
    env: ENV,
    flow: { pump_state: state || { last_seen_ms: Date.now() }, pump_next_id: 5,
      pump_pending: pending || null },
    msg: { payload: action },
  });
}
const downlinkOf = run => c.messages(run.result)
  .find(m => typeof m.topic === 'string' && m.topic.endsWith('/command/down'));

const started = downlinkOf(send({ kind: 'command', command: 'start' }));
if (!started) c.fail('the start command built no downlink');
else {
  if (started.topic !== 'application/' + APP + '/device/' + EUI + '/command/down') {
    c.fail('downlink topic built from the wrong identifiers: ' + started.topic);
  }
  const body = JSON.parse(started.payload);
  if (body.fPort !== 50) c.fail('downlink is not FPort 50: ' + body.fPort);
  if (body.devEui !== EUI) c.fail('downlink devEui is ' + body.devEui);
  // [version, operation, id, argument], operation 4 is start.
  const bytes = Buffer.from(body.data, 'base64');
  if (bytes.toString('hex') !== '010400050000') {
    c.fail('start downlink bytes are ' + bytes.toString('hex'));
  }
}
const tuned = JSON.parse(downlinkOf(send({ kind: 'command', command: 'set_frequency', frequency_hz: 35.5 })).payload);
const tunedBytes = Buffer.from(tuned.data, 'base64');
if (tunedBytes[1] !== 3 || ((tunedBytes[4] << 8) | tunedBytes[5]) !== 3550) {
  c.fail('set_frequency downlink is ' + tunedBytes.toString('hex'));
}
c.done('pump_control: start and set_frequency build the FPort 50 downlink for protocol v1');

const badHz = send({ kind: 'command', command: 'set_frequency', frequency_hz: 5 });
if (downlinkOf(badHz)) c.fail('a 5 Hz set_frequency was sent');
if (!/10 to 50 Hz/.test((badHz.store.f_pump_state || {}).ui_error || '')) {
  c.fail('an out-of-range frequency did not explain itself');
}
const unknown = send({ kind: 'command', command: 'explode' });
if (downlinkOf(unknown)) c.fail('an unknown command was sent');
if (!/Unknown pump command/.test((unknown.store.f_pump_state || {}).ui_error || '')) {
  c.fail('an unknown command did not explain itself');
}
c.done('pump_control: an out-of-range frequency and an unknown command are refused');

const noStatus = send({ kind: 'command', command: 'start' }, {});
if (downlinkOf(noStatus)) c.fail('a command was sent before any status uplink');
const stale = send({ kind: 'command', command: 'start' }, { last_seen_ms: Date.now() - 200000 });
if (downlinkOf(stale)) c.fail('a start was sent while the status was stale');
if (!/stale/i.test((stale.store.f_pump_state || {}).ui_error || '')) {
  c.fail('a stale status did not explain the refusal');
}
const staleStop = send({ kind: 'command', command: 'stop' }, { last_seen_ms: Date.now() - 200000 });
if (!downlinkOf(staleStop)) c.fail('stop is no longer available while the status is stale');
const busy = send({ kind: 'command', command: 'start' }, { last_seen_ms: Date.now() },
  { id: 9, command: 'start' });
if (downlinkOf(busy)) c.fail('a second command was sent while one was pending');
if (!downlinkOf(send({ kind: 'command', command: 'stop' }, { last_seen_ms: Date.now() },
  { id: 9, command: 'start' }))) {
  c.fail('stop is no longer available while a command is pending');
}
c.done('pump_control: commands wait for a fresh status, and stop stays available');

c.finish();
