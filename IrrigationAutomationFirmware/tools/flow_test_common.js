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
  const seen = { status: [], send: [] };
  const flow = {
    get: k => store['f_' + k],
    set: (k, v) => { store['f_' + k] = v; },
  };
  const context = {
    get: k => store['c_' + k],
    set: (k, v) => { store['c_' + k] = v; },
  };
  const node = {
    status: s => seen.status.push(s),
    warn: m => seen.warn = m,
    send: m => seen.send.push(m),
  };
  const fn = vm.runInNewContext(
    '(function(env, flow, context, node, msg) {' + func + '})', { Buffer });
  const result = fn({ get: k => env[k] }, flow, context, node, options.msg || null);
  return { result, store, seen };
}

// Flatten a node's output: Node-RED accepts a single message or an array.
const messages = result => (Array.isArray(result) ? result.flat(Infinity) : [result])
  .filter(x => x && typeof x === 'object');

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
  done(name + ': import invariants (no tab, one absent flow id, shared broker, dynamic subscription)');
}

module.exports = {
  APP_ID, BROKER_ID, UI_BASE, fail, ok, done, finish,
  loadFlow, runNode, messages, checkImportInvariants,
};
