// Safety-sequence smoke test for the generated Node-RED function, plus the
// import invariants the shipped flow has to keep.
const fs = require('fs');
const assert = require('assert');
const nodes = JSON.parse(fs.readFileSync('examples/IntegratedDashboard/irrigation_dashboard_flow.json', 'utf8'));
const byId = Object.fromEntries(nodes.map(n => [n.id, n]));

// Ids the live canvas already owns. Node-RED's importer preserves incoming ids
// and reuses or replaces a node whose id it recognises, but adds a *second*
// node for an id it does not -- so a re-import only updates the running
// dashboard in place if these match. See tools/build_irrigation_dashboard.py.
const IDS = {
  canvas: 'dc658b3ab3f2917e', tick: 'fd327e43632c19d6',
  settings: '0da71259acd21508', subscribeTick: '9b727c5b0531676e',
  subscribe: '405c9e29995e8efe', mqttIn: 'ed30a564d5bb461a',
  status: '50c3e5351eac102c', control: '1406698c9cd792c5',
  mqttOut: '03424753ad790e6b', ui: 'd490a04f3084b9da',
  page: 'irrigation_page', group: 'irrigation_group',
  base: 'f53e93e9ba219e63', theme: 'e49416861823a329'
};
const SHARED_BROKER = 'ae0178f3742ff530';

assert(!nodes.some(n => n.type === 'tab'),
  'no tab node may ship: the import dialog decides which flow the nodes join, and a tab would add a second one');
assert(!nodes.some(n => n.type === 'mqtt-broker'),
  'the shared ChirpStack broker must not ship, or the import adds a duplicate server and can overwrite its settings');
for (const n of nodes.filter(n => n.broker)) {
  assert.equal(n.broker, SHARED_BROKER,
    `${n.id} must reference the canvas's shared chirpstack_mosquito broker`);
}
// Singletons belong to no flow; everything else must name the canvas's flow,
// so an import cannot drop a node into a second copy of it.
const GLOBAL = new Set(['ui-base', 'ui-theme', 'ui-breakpoint', 'global-config']);
for (const n of nodes.filter(n => !GLOBAL.has(n.type))) {
  assert.equal(n.z, 'irrigation_single_page', `${n.id} must stay in the flow the canvas uses`);
}

const wired = [IDS.tick, IDS.settings, IDS.subscribeTick, IDS.subscribe, IDS.mqttIn,
               IDS.status, IDS.control, IDS.mqttOut, IDS.ui];
assert(byId[IDS.canvas] && byId[IDS.canvas].type === 'group', 'the canvas group node is part of the export');
assert.deepEqual([...byId[IDS.canvas].nodes].sort(), [...wired].sort(),
  'the canvas group must list exactly the flow nodes, or the canvas loses its frame on import');
for (const id of wired) {
  assert.equal(byId[id].g, IDS.canvas, `${id} must sit inside the canvas group`);
}
for (const group of nodes.filter(n => n.type === 'ui-group')) {
  const page = byId[group.page];
  assert(page && page.type === 'ui-page', `ui-group ${group.id} has no valid page`);
  const base = byId[page.ui];
  assert(base && base.type === 'ui-base', `ui-page ${page.id} has no valid base`);
}
const page = byId[IDS.page];
assert(page && page.type === 'ui-page', 'the page id must match the canvas, or an import adds a second "Irrigation" page');
assert(byId[IDS.group] && byId[IDS.group].type === 'ui-group', 'the group id must match the canvas, or an import adds a second group');
assert.equal(page.ui, IDS.base, 'the page must use the workspace dashboard, not one of its own');
assert.equal(page.theme, IDS.theme, 'the page must use the workspace theme');
assert.equal(byId[IDS.group].page, IDS.page);

const settings = byId[IDS.settings].func;
const control = byId[IDS.control].func;
const template = byId[IDS.ui].format;
const subscribeNode = byId[IDS.subscribe];
assert.deepEqual(byId[IDS.tick].wires[0], [IDS.settings]);
assert.deepEqual(byId[IDS.subscribeTick].wires[0], [IDS.subscribe]);
assert.equal(byId[IDS.mqttIn].inputs, 1);
assert.equal(byId[IDS.mqttIn].topic, '');
assert.deepEqual(subscribeNode.wires[0], [IDS.mqttIn]);

// The water tile goes stale on the WaterLevel node's own schedule. Re-resolved
// here rather than imported from tools/water_level_interval.py: a check that
// mirrors the implementation it checks proves nothing. Resolution follows the
// compiler, so a -D in platformio.ini wins over the header's #ifndef fallback.
const ini = fs.readFileSync('platformio.ini', 'utf8');
const header = fs.readFileSync('examples/WaterLevel/src/WaterLevelConfig.h', 'utf8');
const sleep = Number((ini.match(/^\s*-D\s*WATER_LEVEL_SLEEP_SECONDS=(\d+)/m)
  || header.match(/#define\s+WATER_LEVEL_SLEEP_SECONDS\s+(\d+)/) || [])[1]);
assert(sleep, 'cannot resolve WATER_LEVEL_SLEEP_SECONDS from platformio.ini or WaterLevelConfig.h');
const waterAge = Number(settings.match(/waterMaxAgeSec:\s*(\d+)/)[1]);
assert.equal(waterAge, Math.max(2 * sleep, 60),
  `waterMaxAgeSec ${waterAge} s does not track the node's ${sleep} s reporting interval`);

// The field valves report on a fixed short cadence while their valve is open, and
// each valve runs its own firmware, so one valveOpenMaxAgeSec can only fit both if
// both headers agree. Resolved from the two headers directly, like the water
// interval above. valveClosedMaxAgeSec is deliberately not resolved: the closed
// cadence is an operator setting with a downlink, so there is no compile-time
// value to check it against.
const pcvHeaders = [
  'examples/PressureControlNode/include/PressureNodeConfig.h',
  'examples/PressureControlNode2/include/PressureNode2Config.h',
].map(p => fs.readFileSync(p, 'utf8'));
const openCadences = pcvHeaders.map(h => Number(
  (h.match(/#define\s+PCV_LORAWAN_OPEN_INTERVAL_SECONDS\s+(\d+)/)
   || h.match(/VALVE_OPEN_REPORT_INTERVAL_SECONDS\s*=\s*(\d+)/) || [])[1]));
openCadences.forEach((v, i) => assert(v,
  `cannot resolve the open-valve reporting interval from ${['PressureControlNode', 'PressureControlNode2'][i]}`));
assert.equal(openCadences[0], openCadences[1],
  `valve_1 and valve_2 open on different reporting cadences (${openCadences.join(' s vs ')} s); one limit cannot fit both`);
const valveOpenAge = Number(settings.match(/valveOpenMaxAgeSec:\s*(\d+)/)[1]);
assert(valveOpenAge >= 2 * openCadences[0] && valveOpenAge <= 4 * openCadences[0],
  `valveOpenMaxAgeSec ${valveOpenAge} s does not clear two of the ${openCadences[0]} s open-valve cadence`);

// PumpControl reports on a fixed stopped cadence, resolved from its source like the
// water interval above. The pump carries two stopped-state limits and they have to
// bracket each other: pumpOnlineMaxAgeSec is the loose one every liveness question
// reads, pumpStopTimeoutSec is the deadline a stop confirmation is judged against,
// and pumpStoppedMaxAgeSec is the tight one in between that the stop sequence uses
// to decide it may close the valves.
const pumpSource = fs.readFileSync('examples/PumpControl/src/main.cpp', 'utf8');
const stoppedCadence = Number(
  (pumpSource.match(/STATUS_INTERVAL_STOPPED_MS\s*=\s*(\d+)\s*\*\s*1000/) || [])[1]);
assert(stoppedCadence, 'cannot resolve STATUS_INTERVAL_STOPPED_MS from examples/PumpControl/src/main.cpp');
const pumpOnlineAge = Number(settings.match(/pumpOnlineMaxAgeSec:\s*(\d+)/)[1]);
const pumpStoppedAge = Number(settings.match(/pumpStoppedMaxAgeSec:\s*(\d+)/)[1]);
const pumpStopTimeout = Number(settings.match(/pumpStopTimeoutSec:\s*(\d+)/)[1]);
assert(pumpOnlineAge >= 2 * stoppedCadence && pumpOnlineAge <= 4 * stoppedCadence,
  `pumpOnlineMaxAgeSec ${pumpOnlineAge} s does not clear two of the ${stoppedCadence} s stopped heartbeat`);
assert(pumpStoppedAge < pumpOnlineAge,
  `pumpStoppedMaxAgeSec ${pumpStoppedAge} s is not tighter than pumpOnlineMaxAgeSec ${pumpOnlineAge} s, so the split does nothing`);
assert(pumpOnlineAge > pumpStopTimeout,
  `pumpOnlineMaxAgeSec ${pumpOnlineAge} s sits below pumpStopTimeoutSec ${pumpStopTimeout} s, leaving a band where the sequencer can neither judge the pump fresh nor fault`);
const subscriptionContext = new Map();
const subscription = new Function('env','context','node', subscribeNode.func);
const subscriptionStatus = {status:()=>{}};
const subscriptionEnv = {get:()=> '12345678-1234-1234-1234-123456789abc'};
const context = {get:k=>subscriptionContext.get(k),set:(k,v)=>subscriptionContext.set(k,v)};
assert.deepEqual(subscription(subscriptionEnv,context,subscriptionStatus), {
  action:'subscribe',topic:'application/12345678-1234-1234-1234-123456789abc/device/+/event/up',qos:0
});
assert.equal(subscription(subscriptionEnv,context,subscriptionStatus), null);
const vueScript = template.match(/<script>([\s\S]*?)<\/script>/)[1];
new Function(vueScript.replace('export default', 'return'));
assert(template.includes('v-for="(d,k) in cards"'));
assert(template.includes('{{ cardValue(k,d) }}'));
assert(!/\bvalue\s*\(\s*k\s*,\s*d\s*\)/.test(template),
  'Dashboard 2.0 reserves value in the render context; use cardValue');
assert(template.includes("kind:'refresh_pump'"));
assert(template.includes('VFD and radio status unavailable'));
assert(template.includes("timeZone:'Asia/Tashkent'"));
const memory = new Map();
const flow = {get:k=>memory.get(k),set:(k,v)=>memory.set(k,v)};
const env = {get:k=>{
  const name=k.replace(/_DEV_EUI$/,'').toLowerCase();
  return k==='IRRIGATION_APP_ID' ? '12345678-1234-1234-1234-123456789abc' :
    ({water:'0000000000000001',soil:'0000000000000002',main:'0000000000000003',
      valve1:'0000000000000004',valve2:'0000000000000005',pump:'0000000000000006'})[name];
}};
const node = {status:()=>{}};
const configure = new Function('msg','flow','node','env','Buffer',settings);
const run = new Function('msg','flow','node','env','Buffer',control);
const take = msg => run(msg,flow,node,env,Buffer);
configure({},flow,node,env,Buffer);
const app = '12345678-1234-1234-1234-123456789abc';
function up(name,object,port) {
  const eui=env.get(name.toUpperCase()+'_DEV_EUI');
  return take({topic:`application/${app}/device/${eui}/event/up`,payload:{fPort:port,object}});
}
function upRaw(name,bytes,port) {
  const eui=env.get(name.toUpperCase()+'_DEV_EUI');
  return take({topic:`application/${app}/device/${eui}/event/up`,payload:{fPort:port,data:Buffer.from(bytes).toString('base64')}});
}
up('water',{pressure_valid:true,depth_m:0.25},40);
up('soil',{sensor_valid:true,vwc_percent:50},10);
up('main',{actuator_online:true,actuator_fault_code:0,overpressure:false,actual_angle_deg:0,valve_moving:false,command_phase:'none'},31);
up('valve1',{pcv_last_commanded:'close',last_command_id:0},31);
up('valve2',{pcv_last_commanded:'close',last_command_id:0},31);
upRaw('pump',[2,51,0,0,0,0,0,0,0,0,0,0,0,0,0,3,0,0xfb,0xa4,3,0,60],51);
assert.equal(memory.get('irrigation').devices.pump.previous_join_error_code,-1116);
assert.equal(memory.get('irrigation').devices.pump.join_attempt_count,3);
upRaw('pump',[1,51,0,0,0,0,0,0,0,0,0,0,0,0,0,3,0],51);
assert.equal(memory.get('irrigation').devices.pump.run_state,'stopped');
let out=take({payload:{kind:'start',valve1:true,valve2:true}});
assert.equal(memory.get('irrigation').phase,'wait_main_open');
assert(out[0][0].topic.includes('0000000000000003'));
let q=memory.get('irrigation').pending;
up('main',{actuator_online:true,actuator_fault_code:0,overpressure:false,actual_angle_deg:90,valve_moving:false,command_phase:'finished',last_command_id:q.id,reported_command_id:q.id},31);
assert.equal(memory.get('irrigation').phase,'wait_field_open');
q=memory.get('irrigation').pending;
assert.equal(q.device,'valve1');
up('valve1',{pcv_last_commanded:'open',last_command_id:q.id,status_reason:'remote_command'},31);
take({payload:{kind:'tick'}});
q=memory.get('irrigation').pending;
assert.equal(q.device,'valve2');
up('valve2',{pcv_last_commanded:'open',last_command_id:q.id,status_reason:'remote_command'},31);
take({payload:{kind:'tick'}});
assert.equal(memory.get('irrigation').phase,'wait_frequency');
q=memory.get('irrigation').pending;
up('pump',{communication_ok:true,configuration_valid:true,vfd_fault_code:0,running:false,frequency_armed:true,commanded_frequency_hz:25,last_command_id:q.id,command_result:'accepted'},51);
assert.equal(memory.get('irrigation').phase,'wait_pump_start');
q=memory.get('irrigation').pending;
upRaw('pump',[2,59,6,q.id>>8,q.id&255,0x09,0xc4,0,0,0,0,0,0,0,0,3,0,0,0,1,0,60],51);
assert.equal(memory.get('irrigation').phase,'wait_pump_start');
assert.equal(memory.get('irrigation').notice,'Pump start accepted; waiting for final VFD condition');
up('pump',{communication_ok:true,configuration_valid:true,vfd_fault_code:0,running:true,frequency_armed:true,last_command_id:q.id,command_result:'accepted'},51);
assert.equal(memory.get('irrigation').phase,'running');
out=up('water',{pressure_valid:true,depth_m:0.19},40);
assert.equal(memory.get('irrigation').phase,'wait_pump_stop');
assert(out[0][0].topic.includes('0000000000000006'));
q=memory.get('irrigation').pending;
up('pump',{communication_ok:true,configuration_valid:true,vfd_fault_code:0,running:false,last_command_id:q.id,command_result:'in_progress'},51);
assert.equal(memory.get('irrigation').phase,'wait_pump_stop');
up('pump',{communication_ok:true,configuration_valid:true,vfd_fault_code:0,running:false,last_command_id:q.id,command_result:'accepted'},51);
assert.equal(memory.get('irrigation').phase,'wait_field_close');
assert.equal(memory.get('irrigation').pending.device,'valve2');

out=take({payload:{kind:'refresh_pump'}});
assert.equal(JSON.parse(out[0][0].payload).fPort,50);
assert.equal(JSON.parse(out[0][0].payload).data,'AQU=');
assert.equal(memory.get('irrigation').pumpRefresh.pending,true);
const refreshRequestedAt=memory.get('irrigation').pumpRefresh.requestedAt;
up('pump',{communication_ok:true,configuration_valid:true,vfd_fault_code:0,running:false,last_command_id:q.id,command_result:'accepted'},51);
assert.equal(memory.get('irrigation').pumpRefresh.pending,false);
assert.equal(memory.get('irrigation').pumpRefresh.requestedAt,refreshRequestedAt);
out=take({payload:{kind:'refresh_pump'}});
assert.equal(out[0],null);

const adaptiveMemory = new Map();
const adaptiveFlow = {get:k=>adaptiveMemory.get(k),set:(k,v)=>adaptiveMemory.set(k,v)};
configure({},adaptiveFlow,node,env,Buffer);
const realNow = Date.now;
const base = realNow();
const commonDevices = {
  water:{at:base,pressure_valid:true,depth_m:0.25},
  soil:{at:base,sensor_valid:true,vwc_percent:50},
  main:{at:base,actuator_online:true,actuator_fault_code:0,overpressure:false,actual_angle_deg:90,valve_moving:false},
  valve1:{at:base,pcv_last_commanded:'open'}, valve2:{at:base,pcv_last_commanded:'open'}
};
adaptiveMemory.set('irrigation',{phase:'running',runAt:base,selected:['valve1'],step:0,notice:'Watering',pending:null,devices:{...commonDevices,pump:{at:base,communication_ok:true,configuration_valid:true,vfd_fault_code:0,running:true}}});
Date.now=()=>base+46001;
new Function('msg','flow','node','env','Buffer',control)({payload:{kind:'tick'}},adaptiveFlow,node,env,Buffer);
assert.equal(adaptiveMemory.get('irrigation').notice,'pump: stop sent; awaiting device result');
assert.equal(adaptiveMemory.get('irrigation').stopReason,'Pump status unsafe');
// A stopped PumpControl reports once a minute, so the two questions asked of its
// last report are answered off different limits. This is the split, stated as the
// one thing it exists to prevent: the same 100 s old "stopped" report must clear the
// Start gate and must still be refused by the stop sequence.
const pumpTick = devices => {
  adaptiveMemory.set('irrigation',{phase:'idle',selected:[],step:0,notice:'Ready',pending:null,devices});
  Date.now=()=>base;
  new Function('msg','flow','node','env','Buffer',control)({payload:{kind:'tick'}},adaptiveFlow,node,env,Buffer);
  return adaptiveMemory.get('irrigation');
};
const stoppedPump = age => ({...commonDevices,
  pump:{at:base-age,communication_ok:true,configuration_valid:true,vfd_fault_code:0,running:false}});
assert.equal(pumpTick(stoppedPump(100000)).check,'',
  'a stopped pump silent for 100 s is within pumpOnlineMaxAgeSec and must not refuse Start');
assert.equal(pumpTick(stoppedPump(190000)).check,'Pump offline, running or faulted',
  'past pumpOnlineMaxAgeSec the Start gate must still refuse');
adaptiveMemory.set('irrigation',{phase:'stop_pump',selected:['valve1'],step:0,notice:'',pending:null,devices:stoppedPump(100000)});
Date.now=()=>base;
new Function('msg','flow','node','env','Buffer',control)({payload:{kind:'tick'}},adaptiveFlow,node,env,Buffer);
assert.equal(adaptiveMemory.get('irrigation').phase,'wait_pump_stop',
  'the same 100 s old "stopped" is past pumpStoppedMaxAgeSec: the stop sequence must command a stop rather than close the valves');
assert.equal(adaptiveMemory.get('irrigation').notice,'pump: stop sent; awaiting device result');
assert.equal(pumpTick(stoppedPump(76000)).devices.pump.stale,false,
  'the card reads pumpOnlineMaxAgeSec: one lost 60 s heartbeat must not flicker it stale');
assert.equal(pumpTick(stoppedPump(190000)).devices.pump.stale,true,
  'past pumpOnlineMaxAgeSec the card must read stale');
// The valve limits split by state: the same minute of silence is stale while a
// valve is open and still fresh while it is closed. That is what lets the open
// half stay tight without the closed half flagging a healthy idle valve.
adaptiveMemory.set('irrigation',{phase:'idle',selected:[],step:0,notice:'Ready',pending:null,devices:{...commonDevices,
  valve1:{at:base,pcv_last_commanded:'open'}, valve2:{at:base,pcv_last_commanded:'close'},
  pump:{at:base,communication_ok:true,configuration_valid:true,vfd_fault_code:0,running:false}}});
Date.now=()=>base+60000;
new Function('msg','flow','node','env','Buffer',control)({payload:{kind:'tick'}},adaptiveFlow,node,env,Buffer);
const splitState=adaptiveMemory.get('irrigation').devices;
assert.equal(splitState.valve1.stale,true,'a valve open and silent past valveOpenMaxAgeSec must read stale');
assert.equal(splitState.valve2.stale,false,'a closed valve silent for the same time is within valveClosedMaxAgeSec');
Date.now=realNow;
console.log('Irrigation sequence smoke test passed');
