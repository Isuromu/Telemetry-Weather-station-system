// Checks for soil_node_dashboard_compact_flow.json.
//
//     node tools/test_soil_node_dashboard.js
//
// Regenerate the flow with tools/build_soil_node_dashboard.py after changing
// the builder. The decode and interval functions are executed here with a fake
// Node-RED environment: FPort 10 status, the FPort 11 command result, and the
// FPort 10 downlink the interval control builds.
const c = require('./flow_test_common.js');

const flow = c.loadFlow('SoilNode/include/soil_node_dashboard_compact_flow.json');
const byId = new Map(flow.map(n => [n.id, n]));
const APP = c.APP_ID;
const EUI = '0011223344556677';
const ENV = { IRRIGATION_APP_ID: APP, SOIL_DEV_EUI: EUI };
const TOPIC = 'application/' + APP + '/device/' + EUI + '/event/up';

const DECODE = byId.get('f5cfea6505836f56');
const INTERVAL = byId.get('6e088ee63985651d');

c.checkImportInvariants(flow, { name: 'soil_node' });

function statusEvent(object, extra) {
  return Object.assign({
    fPort: 10, fCnt: 4, dr: 5, adr: true, devAddr: '00aabbcc',
    time: '2026-10-05T13:26:58.759892249+00:00',
    deviceInfo: { deviceClassEnabled: 'CLASS_A' },
    rxInfo: [{ rssi: -80, snr: 5.5, gatewayId: 'aabbccddeeff0011' }],
    txInfo: { frequency: 867700000, modulation: { lora: { spreadingFactor: 7, bandwidth: 125000 } } },
    object,
  }, extra);
}

const decode = msg => c.runNode(DECODE.func, { env: ENV, msg }).result;
const stateOf = result => {
  const first = c.messages(result)[0];
  return first && first.payload && first.payload.state;
};

// --- FPort 10 status -------------------------------------------------------
const healthy = stateOf(decode({ topic: TOPIC, payload: statusEvent({
  sensor_valid: true, temperature_c: 21.5, vwc_percent: 33.2, ec_ms_cm: 1.23,
  battery_voltage_v: 3.7,
}) }));
if (!healthy) c.fail('the FPort 10 status did not reach the card');
else {
  if (healthy.sensor_valid !== true) c.fail('sensor_valid not mapped');
  if (healthy.temperature_c !== 21.5) c.fail('temperature_c not mapped: ' + healthy.temperature_c);
  if (healthy.vwc_percent !== 33.2) c.fail('vwc_percent not mapped: ' + healthy.vwc_percent);
  if (healthy.ec_ms_cm !== 1.23) c.fail('ec_ms_cm not mapped: ' + healthy.ec_ms_cm);
  if (healthy.battery_voltage_v !== 3.7) c.fail('battery_voltage_v not mapped: ' + healthy.battery_voltage_v);
  if (healthy.uplink_count !== 1) c.fail('uplink_count did not count the uplink');
  if (healthy.decode_error !== '') c.fail('a valid status set decode_error: ' + healthy.decode_error);
  if (healthy.last_seen !== '2026-10-05T13:26:58.759892249+00:00') c.fail('last_seen not taken from event.time');
}
c.done('soil_node: FPort 10 status reaches the card');

// A sensor the probe rejects still reports its battery.
const invalid = stateOf(decode({ topic: TOPIC, payload: statusEvent({
  sensor_valid: false, temperature_c: 21.5, vwc_percent: 33.2, ec_ms_cm: 1.23,
  battery_voltage_v: 3.7,
}) }));
if (!invalid) c.fail('an invalid-sensor status was dropped completely');
else {
  if (invalid.sensor_valid !== false) c.fail('sensor_valid false not mapped');
  if (invalid.temperature_c !== null || invalid.vwc_percent !== null || invalid.ec_ms_cm !== null) {
    c.fail('an invalid probe still published soil readings');
  }
  if (invalid.battery_voltage_v !== 3.7) c.fail('battery lost when the probe is invalid');
}
c.done('soil_node: an invalid probe publishes no soil reading but keeps the battery');

// The codec missing must be visible on the card, not silently blank.
const noObject = stateOf(decode({ topic: TOPIC, payload: statusEvent(undefined) }));
if (!noObject || !/codec/i.test(noObject.decode_error || '')) {
  c.fail('a status without a decoded object does not mention the codec');
}
c.done('soil_node: a missing codec object is reported');

// A foreign device must not reach the card through the dynamic subscription.
if (c.messages(decode({ topic: 'application/' + APP + '/device/0000000000000000/event/up',
  payload: statusEvent({ sensor_valid: true, battery_voltage_v: 3.7 }) })).length) {
  c.fail('an uplink from another DevEUI reached the card');
}
c.done('soil_node: another DevEUI is filtered out');

// --- FPort 11 command result ----------------------------------------------
// The codec's acknowledged-object form, then the eight raw bytes it falls back
// to when the codec is not installed on the device profile.
const ack = stateOf(decode({ topic: 'application/' + APP + '/device/' + EUI + '/event/up',
  payload: statusEvent(undefined, { fPort: 11, object: {
    message_type: 'command_ack', command_id: 7, command_status: 'applied',
    active_sleep_seconds: 900,
  } }) }));
if (!ack) c.fail('the FPort 11 command result did not reach the card');
else {
  if (ack.last_ack_id !== 7) c.fail('command result id not mapped: ' + ack.last_ack_id);
  if (ack.last_ack_status !== 'applied') c.fail('command status not mapped: ' + ack.last_ack_status);
  if (ack.active_sleep_seconds !== 900) c.fail('active sleep not mapped: ' + ack.active_sleep_seconds);
  if (ack.decode_error !== '') c.fail('a valid command result set decode_error: ' + ack.decode_error);
}
c.done('soil_node: the FPort 11 command result object is reported');

const rawAck = stateOf(decode({ topic: 'application/' + APP + '/device/' + EUI + '/event/up',
  payload: statusEvent(undefined, { fPort: 11,
    data: Buffer.from([1, 0x00, 0x09, 0x00, 0x00, 0x00, 0x03, 0x84]).toString('base64') }) }));
if (!rawAck) c.fail('the raw FPort 11 fallback did not reach the card');
else {
  if (rawAck.last_ack_id !== 9) c.fail('raw command id not decoded: ' + rawAck.last_ack_id);
  if (rawAck.last_ack_status !== 'applied') c.fail('raw command status not decoded: ' + rawAck.last_ack_status);
  if (rawAck.active_sleep_seconds !== 900) c.fail('raw active sleep not decoded: ' + rawAck.active_sleep_seconds);
  if (rawAck.decode_error !== '') c.fail('the raw fallback set decode_error: ' + rawAck.decode_error);
}
c.done('soil_node: the raw eight-byte FPort 11 fallback decodes');

// --- the interval control builds the FPort 10 downlink --------------------
function runInterval(action) {
  return c.runNode(INTERVAL.func, { env: ENV, msg: { payload: action } });
}
const sent = c.messages(runInterval({ kind: 'command', sleep_seconds: 900 }).result)
  .find(m => typeof m.topic === 'string' && m.topic.endsWith('/command/down'));
if (!sent) c.fail('the interval command built no downlink');
else {
  if (sent.topic !== 'application/' + APP + '/device/' + EUI + '/command/down') {
    c.fail('downlink topic built from the wrong identifiers: ' + sent.topic);
  }
  const body = JSON.parse(sent.payload);
  if (body.fPort !== 10) c.fail('interval downlink is not FPort 10: ' + body.fPort);
  if (!body.object || body.object.sleep_seconds !== 900) {
    c.fail('interval downlink carries the wrong object: ' + JSON.stringify(body.object));
  }
  if (!Number.isInteger(body.object.command_id)) c.fail('interval downlink has no command id');
}
c.done('soil_node: the interval control builds the FPort 10 downlink from the environment');

const refused = runInterval({ kind: 'command', sleep_seconds: 5 });
if (c.messages(refused.result).some(m => typeof m.topic === 'string')) {
  c.fail('an out-of-range interval was still sent');
}
if (!/10 to 86400|whole number/i.test((refused.store.f_soil_state || {}).ui_error || '')) {
  c.fail('an out-of-range interval did not explain itself on the card');
}
c.done('soil_node: an out-of-range interval is refused with a message');

c.finish();
