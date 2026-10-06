// Checks for main_valve_dashboard_flow.json.
//
//     node tools/test_main_valve_dashboard.js
//
// Regenerate the flow with tools/build_main_valve_dashboard.py after changing
// the builder. The flow's decode function is executed against the installed
// ChirpStack codec on the same FPort 31 bytes, so protocol v1/v2/v3 must agree
// field for field, including the v3 signed pressure and its INT16_MIN sentinel.
const fs = require('fs');
const path = require('path');
const vm = require('vm');
const c = require('./flow_test_common.js');

const flow = c.loadFlow('MainValve/include/main_valve_dashboard_flow.json');
const byId = new Map(flow.map(n => [n.id, n]));
const APP = c.APP_ID;
const EUI = 'e3d1a4b700000002';
const ENV = { IRRIGATION_APP_ID: APP, MAIN_DEV_EUI: EUI };
const TOPIC = 'application/' + APP + '/device/' + EUI + '/event/up';

const DECODE = byId.get('592f34a506af7cbc');
const COMMAND = byId.get('ceb54dc9bb9f3f1b');
const SUBSCRIBE = byId.get('mv_subscribe');

c.checkImportInvariants(flow, { name: 'main_valve' });

const codec = vm.runInNewContext(fs.readFileSync(path.join(__dirname, '..', 'examples',
  'MainValve', 'include', 'main_valve_class_c_codec.js'), 'utf8') + '\n;decodeUplink', {});

function frame(v, flags, reason, actual10, target10, press, fault, cmd, mode, temp, reported, phase) {
  const b = [v, flags, reason, actual10 >> 8, actual10 & 255, target10 >> 8, target10 & 255,
    (press >> 8) & 255, press & 255, fault >> 8, fault & 255, cmd >> 8, cmd & 255, mode, temp];
  if (v >= 2) b.push(reported >> 8, reported & 255, phase);
  return b;
}

function decodeInFlow(bytes) {
  const event = { fPort: 31, data: Buffer.from(bytes).toString('base64'),
    time: '2026-10-05T13:26:58.759892249+00:00', fCnt: 4, rxInfo: [] };
  const out = c.runNode(DECODE.func, { env: ENV, msg: { topic: TOPIC, payload: event } });
  const first = c.messages(out.result)[0];
  if (!first) throw new Error('the flow dropped an FPort 31 frame the codec accepts');
  return first.payload.state;
}

// flags 0x03 = actuator online and pressure valid, reason 2 = remote_command.
const CASES = [
  ['v3 positive pressure 0.02 bar', frame(3, 3, 2, 250, 110, 2, 0, 7, 1, 75, 7, 3), 0.02, 3],
  ['v3 negative pressure -0.06 bar', frame(3, 3, 2, 250, 110, -6, 0, 7, 1, 75, 7, 3), -0.06, 3],
  ['v3 INT16_MIN marks pressure unavailable', frame(3, 3, 2, 250, 110, -32768, 0, 7, 1, 75, 7, 3), null, 3],
  ['v3 without the pressure-valid flag', frame(3, 1, 2, 250, 110, -32768, 0, 7, 1, 75, 7, 3), null, 3],
  ['v2 keeps the unsigned sentinel', frame(2, 3, 2, 250, 110, 0xFFFA, 0, 7, 1, 75, 7, 3), 655.3, 2],
  ['v1 fifteen-byte frame', frame(1, 3, 2, 250, 110, 0xFFFF, 0, 7, 1, 75), null, 1],
];
const round = v => (v === null ? null : Math.round(v * 1000) / 1000);
for (const [name, bytes, bar, version] of CASES) {
  const expected = codec({ fPort: 31, bytes }).data;
  const actual = decodeInFlow(bytes);
  if (round(actual.pressure_bar) !== bar || round(expected.pressure_bar) !== bar) {
    c.fail(name + ': pressure ' + round(actual.pressure_bar) + ' in the flow, ' +
      round(expected.pressure_bar) + ' in the codec, expected ' + bar);
  }
  if (actual.protocol_version !== version || expected.protocol_version !== version) {
    c.fail(name + ': protocol version ' + actual.protocol_version + '/' + expected.protocol_version);
  }
  if (round(actual.actual_angle_deg) !== round(expected.actual_angle_deg)) {
    c.fail(name + ': actual angle ' + actual.actual_angle_deg + ' vs codec ' + expected.actual_angle_deg);
  }
}
c.done('main_valve: the flow decodes v1/v2/v3 exactly as the codec does');

// --- subscription ----------------------------------------------------------
const subscribed = c.messages(c.runNode(SUBSCRIBE.func, { env: ENV }).result)
  .filter(m => m.action === 'subscribe');
if (subscribed.length !== 1 || subscribed[0].topic !== TOPIC.replace('/up', '/+')) {
  c.fail('subscription topic is ' + JSON.stringify(subscribed.map(m => m.topic)));
}
const repeat = c.runNode(SUBSCRIBE.func, { env: ENV, context: { topic: TOPIC.replace('/up', '/+') } });
if (c.messages(repeat.result).some(m => m.action === 'subscribe')) {
  c.fail('an unchanged subscription is requested again');
}
const missing = c.runNode(SUBSCRIBE.func, { env: {} });
if (c.messages(missing.result).some(m => m.action === 'subscribe')) {
  c.fail('the flow subscribes without IRRIGATION_APP_ID and MAIN_DEV_EUI');
}
if (!/IRRIGATION_APP_ID/.test(JSON.stringify(missing.result))) {
  c.fail('a missing environment does not say so on the card');
}
c.done('main_valve: subscribes from the environment and refuses without it');

// --- command path ----------------------------------------------------------
function queueCommand(state, action) {
  return c.runNode(COMMAND.func, {
    env: ENV, flow: { mv_state: state, mv_queue: [], mv_pending: null },
    msg: { payload: action },
  });
}
const fresh = queueCommand({ last_seen: '2026-10-05T13:26:58.759Z', protocol_version: 3 }, null);
const blocked = c.messages(fresh.result).some(m => m.topic);
if (blocked) c.fail('a command with no action kind was dispatched');

const sent = queueCommand({ last_seen: '2026-10-05T13:26:58.759Z', protocol_version: 3 },
  { kind: 'command', angle_deg: 45 });
const downlink = c.messages(sent.result)
  .find(m => typeof m.topic === 'string' && m.topic.endsWith('/command/down'));
if (!downlink) c.fail('a v3 status did not allow a command to be queued');
else {
  if (downlink.topic !== 'application/' + APP + '/device/' + EUI + '/command/down') {
    c.fail('downlink topic built from the wrong identifiers: ' + downlink.topic);
  }
  const body = JSON.parse(downlink.payload);
  if (body.fPort !== 30) c.fail('downlink is not FPort 30: ' + body.fPort);
  if (body.devEui !== EUI) c.fail('downlink devEui is ' + body.devEui);
  const bytes = Buffer.from(body.data, 'base64');
  if (bytes.length !== 6 || bytes[0] !== 1 || bytes[1] !== 1) {
    c.fail('downlink payload is not the six-byte protocol-v1 command: ' + bytes.toString('hex'));
  }
  if (Math.round(((bytes[2] << 8) | bytes[3]) / 10) !== 45) {
    c.fail('downlink angle is not 45 degrees: ' + ((bytes[2] << 8) | bytes[3]));
  }
}
c.done('main_valve: a v3 status unlocks the queue and the downlink is built from the environment');

const legacy = queueCommand({ last_seen: '2026-10-05T13:26:58.759Z', protocol_version: 1 },
  { kind: 'command', angle_deg: 45 });
if (c.messages(legacy.result).some(m => m.topic)) {
  c.fail('a v1-only status still dispatched a command');
}
c.done('main_valve: a v1 status cannot unlock a command');

const outOfRange = c.messages(queueCommand(
  { last_seen: '2026-10-05T13:26:58.759Z', protocol_version: 3 },
  { kind: 'command', angle_deg: 120 }).result);
if (outOfRange.some(m => m.topic)) c.fail('a 120-degree command was dispatched');
c.done('main_valve: an out-of-range angle is refused');

// --- the card renders timestamps readably ---------------------------------
const template = flow.find(n => n.type === 'ui-template').format;
const script = template.match(/<script>([\s\S]*?)<\/script>/)[1];
const methods = vm.runInNewContext(
  '(function(){' + script.replace('export default', 'return') + '})()', {}).methods;
for (const iso of ['2026-10-05T13:26:58.759892249+00:00', '2026-10-05T13:26:58.759Z',
  '2026-10-05T13:26:58+00:00']) {
  const shown = methods.local(iso);
  if (shown === iso || shown === '-' || shown.includes('T13:')) {
    c.fail('the card shows ' + JSON.stringify(shown) + ' for ' + iso);
  }
}
if (methods.local('') !== '-' || methods.local(null) !== '-') {
  c.fail('an absent timestamp is not shown as "-"');
}
c.done('main_valve: ChirpStack nanosecond timestamps render readably');

c.finish();
