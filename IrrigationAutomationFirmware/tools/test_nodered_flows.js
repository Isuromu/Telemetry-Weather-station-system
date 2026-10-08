// Cross-cutting checks for every Node-RED flow export in examples/.
//
// 1. every file parses, wires resolve, group membership is bidirectional
// 2. no ChirpStack identity (application UUID, DevEUI) is stored in the export
// 3. each mqtt in node subscribes dynamically from a once-per-deploy inject
// 4. every downlink builder reads its identity from environment variables
// 5. every function body executes without throwing
//
// Run from anywhere: node tools/test_nodered_flows.js
const fs = require('fs');
const path = require('path');
const assert = require('assert');

const ROOT = path.resolve(__dirname, '..');
const EXAMPLES = path.join(ROOT, 'examples');

// Identity is detected by shape rather than by a stored denylist: a list of the
// real application ID and DevEUIs would re-commit exactly what this test exists
// to keep out of the repository. Every identifier this project uses is either a
// UUID or a 16-hex DevEUI, so shape matching catches both without storing them.
const UUID = /[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}/;
// Lower case only: DevEUIs and Node-RED ids are lower case, which keeps base64
// payload samples and mixed-case CSS colours from tripping the check.
const HEX16 = /(^|[^0-9a-z-])([0-9a-f]{16})([^0-9a-z-]|$)/g;
// Fields that legitimately hold node/group/config ids rather than device identity.
const ID_FIELDS = new Set(['id', 'z', 'g', 'wires', 'nodes', 'page', 'ui', 'group',
  'broker', 'influxdb', 'theme', 'site', 'base', 'server']);
const STUB_APP = '12345678-1234-1234-1234-123456789abc';
const STUB_EUI = '0000000000000001';

function flowFiles(dir) {
  const out = [];
  for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
    const full = path.join(dir, entry.name);
    if (entry.isDirectory()) out.push(...flowFiles(full));
    else if (entry.name.endsWith('.json')) out.push(full);
  }
  return out.sort();
}

function stubEnv(values) {
  return { get: name => values[name] };
}

const fullEnv = stubEnv(new Proxy({}, {
  get: (_, name) => (name === 'IRRIGATION_APP_ID' ? STUB_APP
    : String(name).endsWith('_DEV_EUI') ? STUB_EUI : undefined),
}));

const emptyEnv = stubEnv({});
const nodeStub = { status: () => {}, warn: () => {}, error: () => {}, log: () => {}, debug: () => {} };
const memory = () => {
  const store = new Map();
  return { get: k => store.get(k), set: (k, v) => store.set(k, v) };
};

function checkStructure(file, nodes) {
  const byId = new Map(nodes.map(n => [n.id, n]));
  for (const n of nodes) {
    for (const outputs of n.wires || []) {
      for (const target of outputs || []) {
        assert(byId.has(target), `${file}: node ${n.id} wires to missing ${target}`);
      }
    }
    if (n.type === 'ui-group') {
      const page = byId.get(n.page);
      assert(page && page.type === 'ui-page', `${file}: ui-group ${n.id} has no valid page`);
      const base = byId.get(page.ui);
      assert(base && base.type === 'ui-base', `${file}: ui-page ${page.id} has no valid base`);
    }
  }
  for (const group of nodes.filter(n => n.type === 'group')) {
    for (const member of group.nodes || []) {
      const n = byId.get(member);
      assert(n, `${file}: group ${group.id} lists missing node ${member}`);
      assert.equal(n.g, group.id, `${file}: ${member} is not tagged with group ${group.id}`);
    }
    for (const n of nodes.filter(n => n.g === group.id)) {
      assert((group.nodes || []).includes(n.id), `${file}: ${n.id} missing from group ${group.id}`);
    }
  }
}

function checkIdentity(file, nodes) {
  const seen = new Set();
  const idSet = new Set(nodes.map(n => n.id));
  const walk = (value, node, path) => {
    if (typeof value === 'string') {
      if (value === '' || idSet.has(value)) return;
      assert(!UUID.test(value), `${file}: ${node.id} ${path} contains a UUID`);
      assert(!/application\/[0-9a-f]{8}/.test(value),
        `${file}: ${node.id} ${path} hardcodes an application/<uuid> topic`);
      for (const m of value.matchAll(HEX16)) {
        assert(/^0+$/.test(m[2]),
          `${file}: ${node.id} ${path} contains non-placeholder EUI-shaped value ${m[2]}`);
      }
    } else if (Array.isArray(value)) {
      value.forEach((v, i) => walk(v, node, `${path}[${i}]`));
    } else if (value && typeof value === 'object') {
      for (const [k, v] of Object.entries(value)) {
        if (ID_FIELDS.has(k)) continue;
        walk(v, node, path ? `${path}.${k}` : k);
      }
    }
  };
  for (const n of nodes) {
    assert(!seen.has(n.id), `${file}: duplicate node id ${n.id}`);
    seen.add(n.id);
    for (const [k, v] of Object.entries(n)) {
      if (ID_FIELDS.has(k)) continue;
      walk(v, n, k);
    }
  }
}

function checkSubscription(file, nodes, results) {
  const byId = new Map(nodes.map(n => [n.id, n]));
  const inputs = nodes.filter(n => n.type === 'mqtt in');
  if (inputs.length === 0) return;
  assert.equal(inputs.length, 1, `${file}: expected exactly one mqtt in node`);

  const mqtt = inputs[0];
  assert.equal(mqtt.topic, '', `${file}: ${mqtt.id} topic must be empty (dynamic subscription)`);
  // Node-RED only accepts control messages when the node has an input wired.
  assert.equal(mqtt.inputs, 1, `${file}: ${mqtt.id} must have "inputs": 1 to be dynamic`);

  const feeders = nodes.filter(n => (n.wires || []).some(w => (w || []).includes(mqtt.id)));
  assert.equal(feeders.length, 1, `${file}: ${mqtt.id} must be fed by one subscribe function`);
  const subscribe = feeders[0];
  assert.equal(subscribe.type, 'function', `${file}: ${subscribe.id} must be a function`);
  assert(/action:\s*['"]subscribe['"]/.test(subscribe.func),
    `${file}: ${subscribe.id} must return {action:'subscribe'}`);

  const triggers = nodes.filter(n => (n.wires || []).some(w => (w || []).includes(subscribe.id)));
  assert(triggers.length >= 1, `${file}: ${subscribe.id} needs an inject trigger`);
  const inject = triggers[0];
  assert.equal(inject.type, 'inject', `${file}: ${subscribe.id} must be triggered by an inject`);
  assert.equal(inject.once, true, `${file}: ${inject.id} must fire once per deploy`);
  assert.equal(inject.repeat, '', `${file}: ${inject.id} must not repeat`);
  assert(!/context\.get\(['"]topic['"]\)/.test(subscribe.func),
    `${file}: ${subscribe.id} must not guard on stored context (blocks redeploy resubscribe)`);

  const run = new Function('msg', 'node', 'env', 'context', 'flow', 'global', subscribe.func);
  // A subscribe function either returns the mqtt control message directly (one
  // output, as the shared dashboard does) or returns it as the first of several
  // outputs alongside a state snapshot (MainValve, WaterLevel). Normalise so the
  // assertions below apply to the control message either way; a run that refuses
  // to subscribe yields no such output and normalises to null.
  const controlOf = out => Array.isArray(out)
    ? (out.find(o => o && o.action === 'subscribe') || null)
    : out;
  const out = controlOf(run({}, nodeStub, fullEnv, memory(), memory(), {}));
  assert.equal(out && out.action, 'subscribe', `${file}: ${subscribe.id} did not subscribe`);
  assert.equal(out.qos, 0, `${file}: ${subscribe.id} must subscribe with qos 0`);
  // A single-device flow subscribes to its own DevEUI; the shared dashboard uses
  // the "+" wildcard and filters per device downstream.
  const device = out.topic.includes(STUB_EUI) ? STUB_EUI : '+';
  const event = out.topic.endsWith('/up') ? 'up' : '+';
  assert.equal(out.topic, `application/${STUB_APP}/device/${device}/event/${event}`,
    `${file}: ${subscribe.id} built an unexpected topic ${out.topic}`);
  assert.equal(controlOf(run({}, nodeStub, emptyEnv, memory(), memory(), {})), null,
    `${file}: ${subscribe.id} must refuse to subscribe without env vars`);
  results.subscriptions.push(`${path.relative(ROOT, file)} -> ${subscribe.name}`);
}

function checkDownlinkIdentity(file, nodes) {
  for (const n of nodes) {
    if (n.type !== 'function' || typeof n.func !== 'string') continue;
    if (!n.func.includes('/command/down')) continue;
    // Allow a literal name or a computed one, as in the shared dashboard's
    // env.get(k.toUpperCase() + '_DEV_EUI'); the character window crosses the
    // inner parentheses of the computed form.
    assert(/env\.get\([\s\S]{0,80}?IRRIGATION_APP_ID/.test(n.func),
      `${file}: ${n.id} builds a downlink topic without IRRIGATION_APP_ID`);
    assert(/env\.get\([\s\S]{0,80}?_DEV_EUI/.test(n.func),
      `${file}: ${n.id} builds a downlink topic without a *_DEV_EUI env var`);
  }
}

function smokeFunctions(file, nodes, results) {
  for (const n of nodes) {
    if (n.type !== 'function' || typeof n.func !== 'string') continue;
    const run = new Function('msg', 'flow', 'node', 'env', 'context', 'global', 'Buffer', n.func);
    try {
      run({ payload: {} }, memory(), nodeStub, fullEnv, memory(), {}, Buffer);
    } catch (err) {
      throw new Error(`${file}: ${n.id} (${n.name}) threw at runtime: ${err.message}`);
    }
    results.functions += 1;
  }
}

const results = { files: 0, functions: 0, subscriptions: [] };
for (const file of flowFiles(EXAMPLES)) {
  const rel = path.relative(ROOT, file);
  let nodes;
  try {
    nodes = JSON.parse(fs.readFileSync(file, 'utf8'));
  } catch (err) {
    throw new Error(`${rel}: invalid JSON: ${err.message}`);
  }
  assert(Array.isArray(nodes), `${rel}: flow export must be a JSON array`);
  assert(nodes.every(n => typeof n.id === 'string'), `${rel}: every node needs an id`);
  checkStructure(rel, nodes);
  checkIdentity(rel, nodes);
  checkSubscription(rel, nodes, results);
  checkDownlinkIdentity(rel, nodes);
  smokeFunctions(rel, nodes, results);
  results.files += 1;
}

console.log(`Node-RED flow checks passed: ${results.files} files, ${results.functions} functions, ` +
            `${results.subscriptions.length} dynamic subscriptions`);
for (const line of results.subscriptions) console.log(`  ${line}`);
