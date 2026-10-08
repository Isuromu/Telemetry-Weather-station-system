// Shared checks for the dashboard flow tests.
//
// The identifier rule (a flow reads IRRIGATION_APP_ID and <DEVICE>_DEV_EUI from
// the Node-RED environment instead of carrying them in git) is asserted across
// every flow by tools/test_flow_ids.js. What is repeated here is the rest of the
// import contract, plus the two helpers the per-flow tests execute functions
// with: a fake Node-RED environment and a coverage check that every field the
// ChirpStack codec emits is either mapped into the card state or explicitly
// ignored by the flow.
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const APP_ID = '12345678-1234-1234-1234-123456789abc';
const BROKER_ID = 'ae0178f3742ff530';
const UI_BASE = 'f53e93e9ba219e63';

let failures = 0;
let sectionStart = 0;
const fail = m => { failures++; console.log('FAIL ' + m); };
const ok = m => console.log('ok   ' + m);
const done = m => { if (failures === sectionStart) ok(m); sectionStart = failures; };
const finish = () => {
  console.log(failures ? '\n' + failures + ' failure(s)' : '\nall checks passed');
  process.exit(failures ? 1 : 0);
};

const loadFlow = relative => JSON.parse(
  fs.readFileSync(path.join(__dirname, '..', 'examples', relative), 'utf8'));

// Run a function node the way Node-RED does: env.get, flow get/set and
// context get/set backed by one store, and a node object that only records.
function runNode(func, options) {
  const store = {};
  const env = options.env || {};
  // Pre-seed Node-RED context so a node that acts on earlier state (a queued
  // command, a last command id) sees it, as it would on a deployed flow.
  for (const [key, value] of Object.entries(options.flow || {})) store['f_' + key] = value;
  for (const [key, value] of Object.entries(options.context || {})) store['c_' + key] = value;
  for (const [key, value] of Object.entries(options.global || {})) store['g_' + key] = value;
  const seen = { status: [], send: [] };
  const flow = {
    get: k => store['f_' + k],
    set: (k, v) => { store['f_' + k] = v; },
  };
  const context = {
    get: k => store['c_' + k],
    set: (k, v) => { store['c_' + k] = v; },
  };
  // The shared command-id counters live in global context, so several dashboards
  // commanding the same device allocate from one series. Keyed 'g_' in the store.
  const global = {
    get: k => store['g_' + k],
    set: (k, v) => { store['g_' + k] = v; },
  };
  const node = {
    status: s => seen.status.push(s),
    warn: m => seen.warn = m,
    send: m => seen.send.push(m),
  };
  const fn = vm.runInNewContext(
    '(function(env, flow, context, node, msg, global) {' + func + '})', { Buffer });
  const result = fn({ get: k => env[k] }, flow, context, node, options.msg || null,
                    global);
  return { result, store, seen };
}

// Flatten a node's output: Node-RED accepts a single message or an array.
const messages = result => (Array.isArray(result) ? result.flat(Infinity) : [result])
  .filter(x => x && typeof x === 'object');

// --- the runaway-loop guard -----------------------------------------------
// A cycle made only of function, mqtt and inject nodes re-fires itself forever:
// every hop re-emits synchronously, so the runtime spins through thousands of
// hops and then dies with RangeError: Maximum call stack size exceeded, having
// flooded every ui-template on the path with state messages. A cycle that passes
// through a ui-* node is not a loop at all -- a ui-template emits only when the
// browser calls this.send() -- which is why the other six dashboards are allowed
// to wire a template back to the logic that feeds it.
function findRunawayLoop(flow) {
  const byId = new Map(flow.map(n => [n.id, n]));
  const inert = n => /^ui-/.test(n.type);
  // A link out has no wires of its own; it resolves to the link in nodes that
  // name the same link, wherever they sit in the flow.
  const links = new Map();
  for (const n of flow) {
    if (n.type !== 'link in') continue;
    const key = (n.links || []).join(',');
    if (!links.has(key)) links.set(key, []);
    links.get(key).push(n.id);
  }
  const adj = new Map();
  for (const n of flow) {
    if (inert(n)) continue;
    const targets = n.type === 'link out'
      ? links.get((n.links || []).join(',')) || []
      : (n.wires || []).flat().filter(Boolean);
    for (const target of targets) {
      const to = byId.get(target);
      if (!to || inert(to)) continue;
      if (!adj.has(n.id)) adj.set(n.id, []);
      adj.get(n.id).push(target);
    }
  }
  // Iterative three-colour DFS: 1 means on the current path, 2 means finished.
  const state = new Map();
  for (const root of adj.keys()) {
    if (state.get(root)) continue;
    const path = [root];
    const stack = [[root, 0]];
    state.set(root, 1);
    while (stack.length) {
      const frame = stack[stack.length - 1];
      const edges = adj.get(frame[0]) || [];
      if (frame[1] >= edges.length) {
        state.set(frame[0], 2);
        stack.pop();
        path.pop();
        continue;
      }
      const next = edges[frame[1]++];
      if (state.get(next) === 1) return path.slice(path.indexOf(next));
      if (!state.get(next)) {
        state.set(next, 1);
        path.push(next);
        stack.push([next, 0]);
      }
    }
  }
  return null;
}

function describeLoop(flow, cycle) {
  const label = id => {
    const node = flow.find(n => n.id === id) || {};
    return (node.name || node.type || id) + ' [' + id + ']';
  };
  return cycle.map(label).join(' -> ') + ' -> back to ' + (label(cycle[0]).split(' [')[0]);
}

// --- the import contract every flow shares --------------------------------
function checkImportInvariants(flow, options) {
  const name = options.name;
  if (flow.some(n => n.type === 'tab')) {
    fail(name + ' ships a tab node, so importing creates a duplicate tab');
  }
  const referenced = [...new Set(flow.map(n => n.z).filter(Boolean))];
  if (referenced.length !== 1) {
    fail(name + ' nodes span ' + referenced.length + ' flow ids: ' + referenced.join(', '));
  } else if (flow.some(n => n.id === referenced[0])) {
    fail(name + ' points at a tab id that is present in the file');
  }
  const brokers = flow.filter(n => n.type === 'mqtt-broker');
  for (const broker of brokers) {
    if (broker.id !== BROKER_ID) {
      fail(name + ' ships broker ' + broker.id + ' instead of the shared ' + BROKER_ID);
    }
  }
  const mqtt = flow.filter(n => n.type === 'mqtt in');
  if (mqtt.length !== 1) {
    fail(name + ' has ' + mqtt.length + ' mqtt in nodes');
  } else if (mqtt[0].topic !== '' || mqtt[0].inputs !== 1) {
    fail(name + ' mqtt in is not dynamic (topic ' + JSON.stringify(mqtt[0].topic) +
      ', inputs ' + mqtt[0].inputs + ')');
  }
  if (brokers.length && mqtt.length && mqtt[0].broker !== BROKER_ID) {
    fail(name + ' mqtt in uses broker ' + mqtt[0].broker);
  }
  const template = flow.find(n => n.type === 'ui-template' && n.format &&
    /state and commands|compact|status|Pump controls/.test(n.name || ''));
  if (template && template.group) {
    // The card chain is widget -> ui-group -> ui-page -> ui-base, linked by
    // ui-group.page and ui-page.ui.
    const group = flow.find(n => n.id === template.group);
    const page = group && flow.find(n => n.id === group.page);
    if (!group || !page || page.ui !== UI_BASE) {
      fail(name + ' card does not chain to the shared dashboard base ' + UI_BASE);
    }
  }
  if (!flow.some(n => n.type === 'ui-base' && n.id === UI_BASE)) {
    fail(name + ' does not reference the shared dashboard base ' + UI_BASE);
  }
  const cycle = findRunawayLoop(flow);
  if (cycle) {
    fail(name + ': runaway loop of function nodes: ' + describeLoop(flow, cycle));
  }
  done(name + ': import invariants (no tab, one absent flow id, shared broker, ' +
    'dynamic subscription, no runaway loop)');
}

module.exports = {
  APP_ID, BROKER_ID, UI_BASE, fail, ok, done, finish,
  loadFlow, runNode, messages, checkImportInvariants,
  findRunawayLoop, describeLoop,
};
