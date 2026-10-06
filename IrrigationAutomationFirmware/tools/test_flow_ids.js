// Guard against ChirpStack identifiers committed inside Node-RED flows.
//
//     node tools/test_flow_ids.js
//
// Every dashboard flow is imported into the user's live Node-RED workspace, so a
// hardcoded application ID or DevEUI in git leaks the deployment, breaks when the
// ChirpStack registration changes, and contradicts the environment-variable
// convention the integrated dashboard documents. This has been fixed by hand
// several times; the checks here are what should catch the next one.
//
// A flow is correct when:
//   * its mqtt in node subscribes dynamically (empty topic, one input), fed by a
//     function that returns an {action:'subscribe'} message;
//   * that function reads IRRIGATION_APP_ID and its own <DEVICE>_DEV_EUI with
//     env.get, and builds the subscription topic from them;
//   * no function, topic or payload carries a literal UUID, 16-hex DevEUI or
//     SET_*_ placeholder;
//   * with the variables set it subscribes to the expected topic, and with them
//     missing it subscribes to nothing and reports the missing configuration.
const fs = require('fs');
const path = require('path');
const vm = require('vm');
const c = require('./flow_test_common.js');

const APP_ID = '12345678-1234-1234-1234-123456789abc';

// euiEnv null means the flow reads all six DevEUIs itself and subscribes across
// the whole application, as the integrated dashboard does.
const FLOWS = [
  { file: 'WaterLevel/include/water_level_dashboard_flow.json', euiEnv: 'WATER_DEV_EUI' },
  { file: 'SoilNode/include/soil_node_dashboard_compact_flow.json', euiEnv: 'SOIL_DEV_EUI' },
  { file: 'MainValve/include/main_valve_dashboard_flow.json', euiEnv: 'MAIN_DEV_EUI' },
  { file: 'PressureControlNode/include/pressure_node_dashboard_flow.json', euiEnv: 'VALVE1_DEV_EUI' },
  { file: 'PressureControlNode2/include/pressure_node2_dashboard_flow.json', euiEnv: 'VALVE2_DEV_EUI' },
  { file: 'PumpControl/include/pump_control_dashboard_flow.json', euiEnv: 'PUMP_DEV_EUI' },
  { file: 'IntegratedDashboard/irrigation_dashboard_flow.json', euiEnv: null },
];

const UUID = /["']([0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12})["']/i;
const DEV_EUI = /["']([0-9a-f]{16})["']/i;
const PLACEHOLDER = /\bSET_[A-Z0-9_]{3,}\b/;
const SUBSCRIBE_ACTION = /action\s*:\s*['"]subscribe['"]/;
const BUILDS_TOPIC = /["'`]application\//;

let failures = 0;
let sectionStart = 0;
const fail = m => { failures++; console.log('FAIL ' + m); };
const ok = m => console.log('ok   ' + m);
const done = m => { if (failures === sectionStart) ok(m); sectionStart = failures; };

const reads = (func, name) =>
  func.includes("env.get('" + name + "')") || func.includes('env.get("' + name + '")');

// Run a function node the way Node-RED does, with the given environment.
function runSubscribe(func, env) {
  const store = {};
  const flow = { get: k => store['f_' + k], set: (k, v) => { store['f_' + k] = v; } };
  const context = { get: k => store['c_' + k], set: (k, v) => { store['c_' + k] = v; } };
  const node = { status: () => {}, warn: () => {} };
  const fn = vm.runInNewContext('(function(env, flow, context, node) {' + func + '})', { Buffer });
  return fn({ get: k => env[k] }, flow, context, node);
}

const actions = result => {
  const list = Array.isArray(result) ? result.flat(Infinity) : [result];
  return list.filter(x => x && typeof x === 'object' && x.action !== undefined);
};

for (const spec of FLOWS) {
  const file = path.join(__dirname, '..', 'examples', spec.file);
  const name = path.basename(file);
  const flow = JSON.parse(fs.readFileSync(file, 'utf8'));

  // --- no identifiers or placeholders anywhere in the flow ------------------
  for (const node of flow) {
    if (node.type === 'mqtt-broker') continue;   // local broker settings, no device ids
    for (const field of ['func', 'topic', 'payload']) {
      const text = String(node[field] === undefined ? '' : node[field]);
      const uuid = text.match(UUID);
      const eui = text.match(DEV_EUI);
      const placeholder = text.match(PLACEHOLDER);
      if (uuid) fail(name + ' ' + node.id + ' ' + field + ' hardcodes application ' + uuid[1]);
      if (eui) fail(name + ' ' + node.id + ' ' + field + ' hardcodes DevEUI ' + eui[1]);
      if (placeholder) fail(name + ' ' + node.id + ' ' + field + ' ships placeholder ' + placeholder[0]);
    }
    if (node.type === 'function' && BUILDS_TOPIC.test(node.func) &&
        !reads(node.func, 'IRRIGATION_APP_ID')) {
      fail(name + ' ' + node.id + ' builds an application topic without IRRIGATION_APP_ID');
    }
  }

  // --- the wiring carries no loop that re-fires itself ----------------------
  // Checked for every flow, including the two the per-flow tests do not cover,
  // and before the `continue`s below so a structural failure cannot hide it.
  const cycle = c.findRunawayLoop(flow);
  if (cycle) {
    fail(name + ': runaway loop of function nodes: ' + c.describeLoop(flow, cycle));
  }
  done(name + ': no runaway loop of function nodes');

  // --- the mqtt in node takes its topic from the subscribe function ---------
  const mqtt = flow.filter(n => n.type === 'mqtt in');
  if (mqtt.length !== 1) {
    fail(name + ' has ' + mqtt.length + ' mqtt in nodes, expected exactly one');
    continue;
  }
  const mqttNode = mqtt[0];
  if (mqttNode.topic !== '' || mqttNode.inputs !== 1) {
    fail(name + ' mqtt in is not in dynamic subscription mode (topic ' +
      JSON.stringify(mqttNode.topic) + ', inputs ' + mqttNode.inputs + ')');
  }

  const subscribers = flow.filter(n => n.type === 'function' && SUBSCRIBE_ACTION.test(n.func));
  if (subscribers.length !== 1) {
    fail(name + ' has ' + subscribers.length + ' subscribe functions, expected exactly one');
    continue;
  }
  const subscribe = subscribers[0];
  const wires = (subscribe.wires || []).flat();
  if (!wires.includes(mqttNode.id)) {
    fail(name + ' subscribe function ' + subscribe.id + ' does not feed the mqtt in node');
  }
  if (!reads(subscribe.func, 'IRRIGATION_APP_ID')) {
    fail(name + ' subscribe function does not read IRRIGATION_APP_ID');
  }
  if (spec.euiEnv) {
    if (!reads(subscribe.func, spec.euiEnv)) {
      fail(name + ' subscribe function does not read ' + spec.euiEnv);
    }
  } else if (!flow.some(n => n.type === 'function' && /_DEV_EUI['"]/.test(n.func))) {
    // A flow that subscribes across the application still has to read each
    // device's DevEUI somewhere to route uplinks and address downlinks.
    fail(name + ' no function reads any <DEVICE>_DEV_EUI');
  }

  // --- behaviour: subscribe with the variables, refuse without them ---------
  const eui = spec.euiEnv ? (FLOWS.indexOf(spec) + 1).toString(16).padStart(16, '0') : null;
  const env = { IRRIGATION_APP_ID: APP_ID };
  if (spec.euiEnv) env[spec.euiEnv] = eui;
  const expected = spec.euiEnv
    ? 'application/' + APP_ID + '/device/' + eui + '/event/+'
    : 'application/' + APP_ID + '/device/+/event/up';

  const subscribed = actions(runSubscribe(subscribe.func, env));
  if (subscribed.length !== 1 || subscribed[0].topic !== expected) {
    fail(name + ' subscribes to ' + JSON.stringify(subscribed.map(a => a.topic)) +
      ', expected ' + JSON.stringify(expected));
  }
  const unconfigured = actions(runSubscribe(subscribe.func, {}));
  if (unconfigured.length) {
    fail(name + ' subscribes even with the environment variables unset: ' +
      JSON.stringify(unconfigured.map(a => a.topic)));
  }
  done(name + ': env-var subscription, no hardcoded identifiers');
}

console.log(failures ? '\n' + failures + ' failure(s)' : '\nall checks passed');
process.exit(failures ? 1 : 0);
