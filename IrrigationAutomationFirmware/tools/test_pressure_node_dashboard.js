// Checks for pressure_node_dashboard_flow.json (valve_1).
//
//     node tools/test_pressure_node_dashboard.js
//
// Regenerate the flow with tools/build_pressure_node_dashboard.py after
// changing the builder. valve_1 carries the TUF-2000M flow meter, so it adds
// flow rate, delivered volume and velocity to the pressure and battery of
// valve_2, plus the local-baseline reset command.
const c = require('./flow_test_common.js');

const flow = c.loadFlow('PressureControlNode/include/pressure_node_dashboard_flow.json');
const byId = new Map(flow.map(n => [n.id, n]));
const APP = c.APP_ID;
const EUI = '0000000000000004';
const ENV = { IRRIGATION_APP_ID: APP, VALVE1_DEV_EUI: EUI };
const TOPIC = 'application/' + APP + '/device/' + EUI + '/event/up';

const TRACK = byId.get('0a790e5e65191dec');
const SEND = byId.get('6ed1a0187f97215d');
const CHARTS = byId.get('1434a85b67183f37');
const ZOOM = byId.get('e5d54bec31ec3acd');

c.checkImportInvariants(flow, { name: 'valve_1' });

// The pressure-scale flag is a build-time constant in PressureNodeConfig.h, not
// a live reading, so the card must not present it as one.
for (const node of flow.filter(n => n.type === 'ui-template')) {
  if (/Sensor scale|not validated/.test(node.format || '')) {
    c.fail('the card still presents the build-time scale flag: ' + node.id);
  }
}
c.done('valve_1: the card does not present the build-time scale flag');

// The PCV has no position feedback: the codec reports pcv_position_verified as a
// constant false, so no card may present it as a reading.
for (const node of flow.filter(n => n.type === 'ui-template')) {
  if (/[Pp]osition verified/.test(node.format || '')) {
    c.fail('the card still presents the constant position flag: ' + node.id);
  }
}
c.done('valve_1: the card does not present the constant position flag');

function statusEvent(object, extra) {
  return Object.assign({
    fPort: 31, fCnt: 12, dr: 5, adr: true, devAddr: '00aabbcc',
    time: '2026-10-05T13:26:58.759892249+00:00',
    deviceInfo: { deviceClassEnabled: 'CLASS_A' },
    rxInfo: [{ rssi: -71, snr: 7, gatewayId: 'aabbccddeeff0011' }],
    txInfo: { frequency: 868100000, modulation: { lora: { spreadingFactor: 7, bandwidth: 125000 } } },
    object,
  }, extra);
}

// The tracker answers on msg.payload, so read the same message once.
const stateOf = msg => {
  const first = c.messages(c.runNode(TRACK.func, { env: ENV, msg }).result)[0];
  return first && first.payload ? first.payload.state : undefined;
};

const object = {
  protocol_version: 2, runtime_mode: 'class_a', status_reason: 'scheduled',
  pcv_last_commanded: 'open', sleep_seconds: 900,
  last_command_id: 21, battery_voltage_v: 3.58, battery_soc_percent: 64,
  upstream_pressure_bar: 1.91, upstream_temperature_c: 20.1, upstream_scale_validated: true,
  downstream_pressure_bar: 1.86, downstream_temperature_c: 20.4, downstream_scale_validated: true,
  water_velocity_m_s: 1.42, flow_rate_m3_h: 3.75, flow_total_since_reset_m3: 128.4,
  tuf_error_bits: 0,
};
const state = stateOf({ topic: TOPIC, payload: statusEvent(object) });
if (!state) c.fail('the FPort 31 status did not reach the card');
else {
  const expected = {
    upstream_pressure_bar: 1.91, downstream_pressure_bar: 1.86, battery_voltage_v: 3.58,
    water_velocity_m_s: 1.42, flow_rate_m3_h: 3.75, flow_total_since_reset_m3: 128.4,
    tuf_error_bits: 0, status_reason: 'scheduled', pcv_last_commanded: 'open', sleep_seconds: 900,
  };
  for (const [key, value] of Object.entries(expected)) {
    if (state[key] !== value) {
      c.fail('state.' + key + ' is ' + JSON.stringify(state[key]) + ', expected ' + JSON.stringify(value));
    }
  }
  if (state.decode_error !== '') c.fail('a valid status set decode_error: ' + state.decode_error);
}
c.done('valve_1: the FPort 31 status including the TUF-2000M readings reaches the card');

if (!/valve_1 status is FPort 31/.test((stateOf({ topic: TOPIC,
  payload: statusEvent(undefined, { fPort: 20 }) }) || {}).decode_error || '')) {
  c.fail('an uplink on the wrong FPort is not explained');
}
if (!/codec/i.test((stateOf({ topic: TOPIC,
  payload: statusEvent(undefined) }) || {}).decode_error || '')) {
  c.fail('a status without a decoded object does not mention the codec');
}
c.done('valve_1: wrong FPort and missing codec object are both explained');

function send(action, state, nextId) {
  return c.runNode(SEND.func, {
    env: ENV,
    flow: { pn1_state: state || { last_command_id: 21 } },
    // The id counter lives in global context, shared with the integrated dashboard.
    global: { cmd_next_id_valve1: nextId === undefined ? 22 : nextId },
    msg: { payload: action },
  });
}
const sent = c.messages(send({ kind: 'command', pcv: 'open', sleep_seconds: 600 }).result)
  .find(m => typeof m.topic === 'string' && m.topic.endsWith('/command/down'));
if (!sent) c.fail('the valve command built no downlink');
else {
  if (sent.topic !== 'application/' + APP + '/device/' + EUI + '/command/down') {
    c.fail('downlink topic built from the wrong identifiers: ' + sent.topic);
  }
  if (sent.payload.fPort !== 30) c.fail('downlink is not FPort 30: ' + sent.payload.fPort);
  if (sent.payload.devEui !== EUI) c.fail('downlink devEui is ' + sent.payload.devEui);
  if (sent.payload.object.pcv !== 'open') c.fail('downlink pcv is ' + sent.payload.object.pcv);
  if (sent.payload.object.sleep_seconds !== 600) c.fail('downlink sleep is ' + sent.payload.object.sleep_seconds);
  if (sent.payload.object.command_id !== 22) c.fail('downlink command id is ' + sent.payload.object.command_id);
}
c.done('valve_1: the command builds an FPort 30 downlink with the codec field names');

// valve_1 ignores a command_id it has already taken, and the integrated dashboard
// advances the shared counter from its own tab, so a counter behind the valve has to
// catch up rather than send an id the valve has seen.
const caughtUp = c.messages(send({ kind: 'command', pcv: 'open', sleep_seconds: 600 },
  { last_command_id: 305 }, 5).result)
  .find(m => typeof m.topic === 'string' && m.topic.endsWith('/command/down'));
if (!caughtUp || caughtUp.payload.object.command_id !== 306) {
  c.fail('a counter behind valve_1 did not catch up: ' +
    (caughtUp ? caughtUp.payload.object.command_id : 'no downlink'));
}
c.done('valve_1: a counter behind the valve catches up to the id it reported');

// The flow meter's local baseline reset travels as its own flag.
const reset = c.messages(send({ kind: 'command', flow_total_reset: true }).result)
  .find(m => typeof m.topic === 'string' && m.topic.endsWith('/command/down'));
if (!reset || reset.payload.object.flow_total_reset !== true) {
  c.fail('the local flow baseline reset did not reach the downlink: ' +
    JSON.stringify(reset && reset.payload.object));
}
c.done('valve_1: the flow-total reset is sent as a codec flag');

const badSleep = send({ kind: 'command', sleep_seconds: 5 });
if (c.messages(badSleep.result).some(m => m.topic)) c.fail('a five-second interval was sent');
if (!/Sleep interval must be/.test((badSleep.store.f_pn1_state || {}).ui_error || '')) {
  c.fail('an out-of-range interval did not explain itself');
}
const empty = send({ kind: 'command' });
if (c.messages(empty.result).some(m => m.topic)) c.fail('an empty command was sent');
if (!/Choose a valve action/.test((empty.store.f_pn1_state || {}).ui_error || '')) {
  c.fail('an empty command did not explain itself');
}
c.done('valve_1: an out-of-range or empty command is refused with a message');

// --- charts: pressure, battery and the TUF flow rate ----------------------
const charts = c.runNode(CHARTS.func, { env: ENV,
  msg: { topic: TOPIC, payload: statusEvent(object) } });
const outputs = Array.isArray(charts.result) ? charts.result : [charts.result];
if (!outputs[0] || outputs[0].map(m => m.topic).join(',') !== 'Upstream,Downstream') {
  c.fail('the pressure series is ' + JSON.stringify(outputs[0] && outputs[0].map(m => m.topic)));
}
if (!outputs[1] || outputs[1].pn1Metric !== 'battery' || outputs[1].payload !== 3.58) {
  c.fail('the battery series is not labelled for the chart');
}
if (!outputs[2] || outputs[2].pn1Metric !== 'flow' || outputs[2].payload !== 3.75) {
  c.fail('the flow-rate series is not labelled for the chart');
}
if (c.messages(c.runNode(CHARTS.func, { env: ENV,
  msg: { topic: TOPIC, payload: statusEvent(undefined, { fPort: 20 }) } }).result).length) {
  c.fail('a non-status uplink reached the charts');
}
c.done('valve_1: pressure, battery and flow-rate series reach their charts');

const zoom = action => {
  const out = c.runNode(ZOOM.func, { env: ENV, msg: action });
  return Array.isArray(out.result) ? out.result : [out.result];
};
const flowPoint = zoom({ pn1Metric: 'flow', topic: 'Flow Rate', payload: 3.75 });
if (!Array.isArray(flowPoint[2]) || flowPoint[2][0].ui_update === undefined ||
    flowPoint[2][1].payload !== 3.75) {
  c.fail('a flow-rate point did not produce the axis update and the point on the third chart');
}
if (flowPoint[0] !== null || flowPoint[1] !== null) {
  c.fail('a flow-rate point also wrote another chart');
}
const control = zoom({ payload: { kind: 'zoom', direction: 1, metric: 'flow' } });
if (!control[5] || control[5].payload.label !== '3 h') {
  c.fail('the flow-rate zoom label did not move to 3 h: ' +
    JSON.stringify(control[5] && control[5].payload));
}
if (!control[2] || control[2].ui_update === undefined) {
  c.fail('a zoom control did not re-scale the flow chart');
}
c.done('valve_1: each of the three charts keeps its own zoom window');

c.finish();
