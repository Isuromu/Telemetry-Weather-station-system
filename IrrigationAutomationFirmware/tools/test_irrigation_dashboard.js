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

// The field-valve waits are failure detectors, not retries, so the deadline also
// has to clear the interval the node sleeps. The closed cadence is an operator
// setting with a downlink and no compile-time value, so this is read for the
// timing test below rather than asserted against a cadence.
const fieldValveCommandTimeoutSec = Number(settings.match(/fieldValveCommandTimeoutSec:\s*(\d+)/)[1]);
assert(fieldValveCommandTimeoutSec, 'cannot resolve fieldValveCommandTimeoutSec from the settings node');

// PumpControl reports on a fixed stopped cadence, resolved from its source like the
// water interval above. The pump carries two stopped-state limits and they have to
// bracket each other: pumpOnlineMaxAgeSec is the liveness limit, while
// pumpStoppedMaxAgeSec is the tighter limit the stop sequence uses before it may
// close the valves.
const pumpSource = fs.readFileSync('examples/PumpControl/src/main.cpp', 'utf8');
const stoppedCadence = Number(
  (pumpSource.match(/STATUS_INTERVAL_STOPPED_MS\s*=\s*(\d+)\s*\*\s*1000/) || [])[1]);
assert(stoppedCadence, 'cannot resolve STATUS_INTERVAL_STOPPED_MS from examples/PumpControl/src/main.cpp');
const pumpOnlineAge = Number(settings.match(/pumpOnlineMaxAgeSec:\s*(\d+)/)[1]);
const pumpStoppedAge = Number(settings.match(/pumpStoppedMaxAgeSec:\s*(\d+)/)[1]);
assert(pumpOnlineAge >= 2 * stoppedCadence && pumpOnlineAge <= 4 * stoppedCadence,
  `pumpOnlineMaxAgeSec ${pumpOnlineAge} s does not clear two of the ${stoppedCadence} s stopped heartbeat`);
assert(pumpStoppedAge >= stoppedCadence && pumpStoppedAge < pumpOnlineAge,
  `pumpStoppedMaxAgeSec ${pumpStoppedAge} s must be at least one heartbeat and tighter than pumpOnlineMaxAgeSec ${pumpOnlineAge} s`);
const subscriptionContext = new Map();
const subscription = new Function('env','context','node', subscribeNode.func);
const subscriptionStatus = {status:()=>{}};
const subscriptionEnv = {get:()=> '12345678-1234-1234-1234-123456789abc'};
const context = {get:k=>subscriptionContext.get(k),set:(k,v)=>subscriptionContext.set(k,v)};
assert.deepEqual(subscription(subscriptionEnv,context,subscriptionStatus), {
  action:'subscribe',topic:'application/12345678-1234-1234-1234-123456789abc/device/+/event/up',qos:0
});
// Called a second time with the same context -- which is what a redeploy looks
// like, because context survives one -- it must subscribe again. Returning null
// here would leave the mqtt in node unsubscribed after every redeploy. The
// once-per-deploy inject is what stops this being a per-message hot path; see
// tools/test_nodered_flows.js, which forbids a stored-context guard.
assert.deepEqual(subscription(subscriptionEnv,context,subscriptionStatus), {
  action:'subscribe',topic:'application/12345678-1234-1234-1234-123456789abc/device/+/event/up',qos:0
});
const vueScript = template.match(/<script>([\s\S]*?)<\/script>/)[1];
new Function(vueScript.replace('export default', 'return'));
assert(template.includes('v-for="(d,k) in cards"'));
assert(template.includes('{{ cardValue(k,d) }}'));
assert(!/\bvalue\s*\(\s*k\s*,\s*d\s*\)/.test(template),
  'Dashboard 2.0 reserves value in the render context; use cardValue');
assert(template.includes("kind:'refresh_pump'"));
assert(template.includes('VFD and radio status unavailable'));
assert(template.includes('pumpModeLabel(d)'),
  'the Pump card must show its AUTO or MANUAL selector state under the top-right status');
assert(template.includes("timeZone:'Asia/Tashkent'"));
const memory = new Map();
const flow = {get:k=>memory.get(k),set:(k,v)=>memory.set(k,v)};
// The shared command-id counters live in global context, so this dashboard and the
// per-device cards allocate from one series per device.
const globalMemory = new Map();
const globalCtx = {get:k=>globalMemory.get(k),set:(k,v)=>globalMemory.set(k,v)};
const env = {get:k=>{
  const name=k.replace(/_DEV_EUI$/,'').toLowerCase();
  return k==='IRRIGATION_APP_ID' ? '12345678-1234-1234-1234-123456789abc' :
    ({water:'0000000000000001',soil:'0000000000000002',main:'0000000000000003',
      valve1:'0000000000000004',valve2:'0000000000000005',pump:'0000000000000006'})[name];
}};
const node = {status:()=>{}};
const configure = new Function('msg','flow','node','env','Buffer','global',settings);
const run = new Function('msg','flow','node','env','Buffer','global',control);
const take = msg => run(msg,flow,node,env,Buffer,globalCtx);
configure({},flow,node,env,Buffer,globalCtx);
const app = '12345678-1234-1234-1234-123456789abc';
function up(name,object,port) {
  const eui=env.get(name.toUpperCase()+'_DEV_EUI');
  return take({topic:`application/${app}/device/${eui}/event/up`,payload:{fPort:port,object}});
}
function upRaw(name,bytes,port) {
  const eui=env.get(name.toUpperCase()+'_DEV_EUI');
  return take({topic:`application/${app}/device/${eui}/event/up`,payload:{fPort:port,data:Buffer.from(bytes).toString('base64')}});
}
// A field valve in flight is s.selected[s.step]: a PCV carries no pending entry,
// because it is confirmed by the state it reports rather than by its command id.
function inFlight() { const s=memory.get('irrigation'); return s.selected[s.step]; }
// A valve echoes the id of the command it accepted, and the sequencer now requires that
// echo before it treats a report as proof -- in Class A the first report after a command
// still carries the previous state. Fixtures must carry the echo, as a device does.
const valveCmdId = k => ((memory.get('irrigation')||{}).valveCmd||{})[k];
up('water',{pressure_valid:true,depth_m:0.25},40);
up('soil',{sensor_valid:true,vwc_percent:50},10);
up('main',{actuator_online:true,actuator_fault_code:0,overpressure:false,actual_angle_deg:0,valve_moving:false,command_phase:'none'},31);
up('valve1',{pcv_last_commanded:'closed',last_command_id:0},31);
up('valve2',{pcv_last_commanded:'closed',last_command_id:0},31);
upRaw('pump',[2,115,0,0,0,0,0,0,0,0,0,0,0,0,0,3,0,0xfb,0xa4,3,0,60],51);
assert.equal(memory.get('irrigation').devices.pump.previous_join_error_code,-1116);
assert.equal(memory.get('irrigation').devices.pump.join_attempt_count,3);
assert.equal(memory.get('irrigation').devices.pump.manual_mode,true);
upRaw('pump',[1,51,0,0,0,0,0,0,0,0,0,0,0,0,0,3,0],51);
assert.equal(memory.get('irrigation').devices.pump.run_state,'stopped');
let out=take({payload:{kind:'start',valve1:true,valve2:true}});
assert.equal(memory.get('irrigation').phase,'wait_main_open');
assert(out[0][0].topic.includes('0000000000000003'));
let q=memory.get('irrigation').pending;
up('main',{actuator_online:true,actuator_fault_code:0,overpressure:false,actual_angle_deg:90,valve_moving:false,command_phase:'finished',last_command_id:q.id,reported_command_id:q.id},31);
assert.equal(memory.get('irrigation').phase,'wait_field_open');
assert.equal(inFlight(),'valve1');
up('valve1',{pcv_last_commanded:'open',last_command_id:valveCmdId('valve1'),status_reason:'remote_command'},31);
take({payload:{kind:'tick'}});
assert.equal(inFlight(),'valve2');
up('valve2',{pcv_last_commanded:'open',last_command_id:valveCmdId('valve2'),status_reason:'remote_command'},31);
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
assert.equal(inFlight(),'valve2');

// --- the close half, end to end -------------------------------------------
// The codec names the uplink state 'closed' while the downlink action is 'close'.
// Every stop run used to fault here, because the sequencer compared the report to
// 'close' and nothing ever matched it.
up('valve2',{pcv_last_commanded:'closed',last_command_id:valveCmdId('valve2')},31);
assert.equal(memory.get('irrigation').phase,'close_fields','a fresh closed report must confirm the close');
assert.equal(memory.get('irrigation').step,0);
take({payload:{kind:'tick'}});
assert.equal(inFlight(),'valve1');
up('valve1',{pcv_last_commanded:'closed',last_command_id:valveCmdId('valve1')},31);
take({payload:{kind:'tick'}});
assert.equal(memory.get('irrigation').phase,'wait_main_close',
  'both field valves confirming must let the sequence reach the main valve');
const mainClose=memory.get('irrigation').pending;
up('main',{actuator_online:true,actuator_fault_code:0,overpressure:false,actual_angle_deg:0,valve_moving:false,command_phase:'finished',last_command_id:mainClose.id,reported_command_id:mainClose.id},31);
assert.equal(memory.get('irrigation').phase,'idle');
assert.equal(memory.get('irrigation').notice,'Watering stopped; valves closed');

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
new Function('msg','flow','node','env','Buffer','global',control)({payload:{kind:'tick'}},adaptiveFlow,node,env,Buffer,globalCtx);
assert.equal(adaptiveMemory.get('irrigation').notice,'pump: stop sent; awaiting device result');
assert.equal(adaptiveMemory.get('irrigation').stopReason,'Pump status unsafe');
// A stopped PumpControl reports once a minute, so the two questions asked of its
// last report are answered off different limits. This is the split, stated as the
// one thing it exists to prevent: the same 100 s old "stopped" report must clear the
// Start gate and must still be refused by the stop sequence.
const pumpTick = devices => {
  adaptiveMemory.set('irrigation',{phase:'idle',selected:[],step:0,notice:'Ready',pending:null,devices});
  Date.now=()=>base;
  new Function('msg','flow','node','env','Buffer','global',control)({payload:{kind:'tick'}},adaptiveFlow,node,env,Buffer,globalCtx);
  return adaptiveMemory.get('irrigation');
};
const stoppedPump = age => ({...commonDevices,
  pump:{at:base-age,communication_ok:true,configuration_valid:true,vfd_fault_code:0,running:false}});
const withinPumpOnlineAgeMs = (pumpOnlineAge - 1) * 1000;
const beyondPumpOnlineAgeMs = (pumpOnlineAge + 1) * 1000;
const beyondPumpStoppedAgeMs = (pumpStoppedAge + 1) * 1000;
assert.equal(pumpTick(stoppedPump(withinPumpOnlineAgeMs)).check,'',
  'a stopped pump within pumpOnlineMaxAgeSec must not refuse Start');
assert.equal(pumpTick(stoppedPump(beyondPumpOnlineAgeMs)).check,'Pump offline, running or faulted',
  'past pumpOnlineMaxAgeSec the Start gate must still refuse');
adaptiveMemory.set('irrigation',{phase:'stop_pump',selected:['valve1'],step:0,notice:'',pending:null,devices:stoppedPump(beyondPumpStoppedAgeMs)});
Date.now=()=>base;
new Function('msg','flow','node','env','Buffer','global',control)({payload:{kind:'tick'}},adaptiveFlow,node,env,Buffer,globalCtx);
assert.equal(adaptiveMemory.get('irrigation').phase,'wait_pump_stop',
  'a stopped report past pumpStoppedMaxAgeSec must cause a stop command before closing valves');
assert.equal(adaptiveMemory.get('irrigation').notice,'pump: stop sent; awaiting device result');
assert.equal(pumpTick(stoppedPump(withinPumpOnlineAgeMs)).devices.pump.stale,false,
  'the card must remain live within pumpOnlineMaxAgeSec');
assert.equal(pumpTick(stoppedPump(beyondPumpOnlineAgeMs)).devices.pump.stale,true,
  'past pumpOnlineMaxAgeSec the card must read stale');
// The valve limits split by state: the same minute of silence is stale while a
// valve is open and still fresh while it is closed. That is what lets the open
// half stay tight without the closed half flagging a healthy idle valve.
adaptiveMemory.set('irrigation',{phase:'idle',selected:[],step:0,notice:'Ready',pending:null,devices:{...commonDevices,
  valve1:{at:base,pcv_last_commanded:'open'}, valve2:{at:base,pcv_last_commanded:'closed'},
  pump:{at:base,communication_ok:true,configuration_valid:true,vfd_fault_code:0,running:false}}});
Date.now=()=>base+60000;
new Function('msg','flow','node','env','Buffer','global',control)({payload:{kind:'tick'}},adaptiveFlow,node,env,Buffer,globalCtx);
const splitState=adaptiveMemory.get('irrigation').devices;
assert.equal(splitState.valve1.stale,true,'a valve open and silent past valveOpenMaxAgeSec must read stale');
assert.equal(splitState.valve2.stale,false,'a closed valve silent for the same time is within valveClosedMaxAgeSec');

// --- the close wait in isolation -------------------------------------------
// Confirmation is the valve's own fresh report, with no command id to match: a
// stale report must not confirm, a reported actuation failure must fault at once
// and name the valve, and silence must fault at fieldValveCommandTimeoutSec.
const closeTick = (valve,ageMs) => {
  adaptiveMemory.set('irrigation',{phase:'wait_field_close',phaseAt:base-ageMs,selected:['valve1'],step:0,notice:'',pending:null,
    devices:{...commonDevices,valve1:valve,
      pump:{at:base,communication_ok:true,configuration_valid:true,vfd_fault_code:0,running:false}}});
  Date.now=()=>base;
  new Function('msg','flow','node','env','Buffer','global',control)({payload:{kind:'tick'}},adaptiveFlow,node,env,Buffer,globalCtx);
  return adaptiveMemory.get('irrigation');
};
assert.equal(closeTick({at:base,pcv_last_commanded:'closed'},1000).phase,'close_fields',
  'a fresh closed report must confirm the close');
assert.equal(closeTick({at:base-181000,pcv_last_commanded:'closed'},1000).phase,'wait_field_close',
  'a closed report older than valveClosedMaxAgeSec must not confirm');
const failedClose=closeTick({at:base,pcv_last_commanded:'open',status_reason:'pcv_actuation_failed'},1000);
assert.equal(failedClose.phase,'fault','a reported actuation failure must fault without waiting out the deadline');
assert.equal(failedClose.notice,'Field valve close failed: valve1 reported pcv_actuation_failed');
const timedOutClose=closeTick({at:base,pcv_last_commanded:'open'},(fieldValveCommandTimeoutSec+1)*1000);
assert.equal(timedOutClose.phase,'fault','silence past fieldValveCommandTimeoutSec must fault');
assert.equal(timedOutClose.notice,'Field valve close unconfirmed: valve1');

// --- command ids ------------------------------------------------------------
// One counter per device, in global context, shared with the device's own card, so
// several dashboards commanding the same hardware allocate from one series. A
// counter that has fallen behind the device must catch up, and the pump refuses an
// id that is not newer than the last one it accepted.
const pumpCommandId = (sharedId,deviceLast) => {
  globalMemory.set('cmd_next_id_pump',sharedId);
  adaptiveMemory.set('irrigation',{phase:'set_frequency',phaseAt:base,selected:['valve1'],step:0,notice:'',pending:null,
    devices:{...commonDevices,pump:{at:base,communication_ok:true,configuration_valid:true,vfd_fault_code:0,
      running:false,frequency_armed:false,last_command_id:deviceLast}}});
  Date.now=()=>base;
  const out=new Function('msg','flow','node','env','Buffer','global',control)({payload:{kind:'tick'}},adaptiveFlow,node,env,Buffer,globalCtx);
  const bytes=Buffer.from(JSON.parse(out[0][0].payload).data,'base64');
  return (bytes[2]<<8)|bytes[3];
};
assert.equal(pumpCommandId(5,305),306,
  'a shared counter behind the pump\'s accepted id must catch up instead of reusing an id');
assert.equal(pumpCommandId(400,305),400,
  'a shared counter already ahead of the pump must be used as-is');
// The pump accepts only an id 1..32767 ahead -- its rule is a signed 16-bit
// difference -- and the counters wrap 65534 -> 0, so "ahead" is modular, not numeric.
assert.equal(pumpCommandId(1,65534),1,
  'an id that wrapped past the device must not be mistaken for one that fell behind');
assert.equal(pumpCommandId(0,65534),0,
  'the wrapped id 0 is one ahead of 65534 and must be kept');
assert.equal(pumpCommandId(32767,0),32767,
  'an id exactly 32767 ahead is the furthest the pump still accepts');
assert.equal(pumpCommandId(32768,0),1,
  'an id 32768 ahead reads as behind, so the counter must catch up');
assert.equal(globalMemory.get('cmd_next_id_pump'),2,
  'the shared counter must advance past the id it just issued');
globalMemory.set('cmd_next_id_pump',100);
const firstSharedId=pumpCommandId(100,99);
const secondSharedId=pumpCommandId(globalMemory.get('cmd_next_id_pump'),99);
assert.equal(firstSharedId,100);
assert.notEqual(secondSharedId,firstSharedId,
  'two consecutive commands must not allocate the same id');

// --- refusal recovery -------------------------------------------------------
// A refusal carries the id the device actually holds, so a command refused for its id
// is resent once with a corrected one, instead of waiting out the deadline.
const refusalTick = pendingRetried => {
  globalMemory.set('cmd_next_id_pump',5);
  adaptiveMemory.set('irrigation',{phase:'wait_pump_start',phaseAt:base,selected:['valve1'],step:0,notice:'',
    pending:{device:'pump',op:'start',id:500,at:base,retried:pendingRetried},
    devices:{...commonDevices,pump:{at:base,communication_ok:true,configuration_valid:true,vfd_fault_code:0,
      running:false,frequency_armed:true,last_command_id:305,command_result:'invalid'}}});
  Date.now=()=>base;
  const out=new Function('msg','flow','node','env','Buffer','global',control)({payload:{kind:'tick'}},adaptiveFlow,node,env,Buffer,globalCtx);
  const downlinks=(Array.isArray(out[0])?out[0]:[]).filter(m=>typeof m.topic==='string'&&m.topic.endsWith('/command/down'));
  return {ids:downlinks.map(m=>{const b=Buffer.from(JSON.parse(m.payload).data,'base64');return (b[2]<<8)|b[3];}),
    state:adaptiveMemory.get('irrigation')};
};
const firstRefusal=refusalTick(false);
assert.deepEqual(firstRefusal.ids,[306],
  'a pump start refused for its id must be resent once with the id the pump reported plus one');
assert.equal(firstRefusal.state.pending.retried,true,'the resend must be marked as the single retry');
assert.deepEqual(refusalTick(true).ids,[],'a second refusal must not resend again');

// --- a start pressed before the sequence is idle ----------------------------
assert(template.includes("state.phase === 'running' || !state.configured || !!state.check"),
  'Start must be available in every phase except running');
const startTick = (state, action) => {
  adaptiveMemory.set('irrigation', state);
  Date.now=()=>base;
  new Function('msg','flow','node','env','Buffer','global',control)(action,adaptiveFlow,node,env,Buffer,globalCtx);
  return adaptiveMemory.get('irrigation');
};
const teardown = extra => ({phase:'wait_field_close',phaseAt:base,selected:['valve1','valve2'],step:1,
  notice:'',pending:null,pendingStart:null,
  devices:{...commonDevices,valve1:{at:base,pcv_last_commanded:'open'},
    valve2:{at:base,pcv_last_commanded:'open'},
    pump:{at:base,communication_ok:true,configuration_valid:true,vfd_fault_code:0,running:false}},
  ...extra});

// Queued, and the teardown in progress is left exactly as it was.
const queued=startTick(teardown({}),{payload:{kind:'start',valve1:true}});
assert.deepEqual(queued.pendingStart,['valve1'],'a start mid-teardown must be queued');
assert.deepEqual(queued.selected,['valve1','valve2'],'queueing must not reshape the teardown in progress');
assert.equal(queued.step,1,'queueing must not move the close pass');
assert.equal(queued.phase,'wait_field_close','queueing must not start anything yet');

// A refusal must not reshape it either, whether the selection is empty or the
// safety conditions have moved since the operator pressed the button.
const emptySel=startTick(teardown({}),{payload:{kind:'start',valve1:false,valve2:false}});
assert.deepEqual(emptySel.selected,['valve1','valve2'],'an empty selection must not reshape the teardown');
assert.equal(emptySel.notice,'Select at least one field valve');
const notReady=startTick({...teardown({}),devices:{...teardown({}).devices,
  water:{at:base,pressure_valid:false,depth_m:0.25}}},{payload:{kind:'start',valve1:true}});
assert.deepEqual(notReady.selected,['valve1','valve2'],'a refused start must not reshape the teardown');
assert.equal(notReady.step,1,'a refused start must not move the close pass');
assert.equal(notReady.pendingStart,null,'a refused start must not be queued');

// It fires when the field valves are closed, and close_main is never entered.
const firing=startTick({...teardown({phase:'close_fields',step:-1,pendingStart:['valve1']}),
  devices:{...teardown({}).devices,
    main:{at:base,actuator_online:true,actuator_fault_code:0,overpressure:false,actual_angle_deg:0,valve_moving:false},
    valve1:{at:base,pcv_last_commanded:'closed'},valve2:{at:base,pcv_last_commanded:'closed'}}},
  {payload:{kind:'tick'}});
assert.equal(firing.phase,'open_main','a queued start must divert to opening, never to close_main');
assert.deepEqual(firing.selected,['valve1'],'the queued selection becomes the active one');
assert.equal(firing.pendingStart,null,'the queued start is consumed once');
// close_main is evaluated after close_fields in the same tick, so the diversion also
// skips it; the next tick issues the main-valve command.
assert.equal(startTick(firing,{payload:{kind:'tick'}}).phase,'wait_main_open',
  'the queued start must open the main valve, not close it');

// A start from fault retries the close pass, including the valve that failed.
const fromFault=startTick(teardown({phase:'fault'}),{payload:{kind:'start',valve2:true}});
assert.equal(fromFault.phase,'wait_field_close','a start from fault must retry the close pass');
assert.deepEqual(fromFault.selected,['valve1','valve2'],'the fault retry closes both field valves');
assert.deepEqual(fromFault.pendingStart,['valve2']);

// A safety stop cancels a queued start, so the pump is not restarted behind it.
const stopped=startTick({...teardown({phase:'running',runAt:base-14401000,pendingStart:['valve1']})},
  {payload:{kind:'tick'}});
assert.equal(stopped.pendingStart,null,'a safety stop must cancel a queued start');
assert.notEqual(stopped.phase,'running','the safety stop must begin the teardown');
assert.equal(stopped.stopReason,'Maximum watering duration reached');

// --- the zone selection survives leaving the page ---------------------------
// Kept on the server, not in the component, so leaving and returning to the page does
// not reset it -- but only from a phase where it is still editable. A run's valves are
// fixed when it starts, so a select arriving mid-run is ignored here as well as locked
// in the panel: a stale second tab that still shows idle must not reshape the next run.
const idleSelect=startTick({...teardown({phase:'idle',selected:[]}),selection:{valve1:true,valve2:true}},
  {payload:{kind:'select',valve1:false,valve2:true}});
assert.deepEqual(idleSelect.selection,{valve1:false,valve2:true},
  'the operator selection must be kept on the server so it survives navigation');
const faultSelect=startTick(teardown({phase:'fault'}),{payload:{kind:'select',valve1:true,valve2:false}});
assert.deepEqual(faultSelect.selection,{valve1:true,valve2:false},
  'a fault is not an active run: the zones for the retry stay selectable');
const midRunSelect=startTick(teardown({selection:{valve1:true,valve2:true}}),
  {payload:{kind:'select',valve1:false,valve2:true}});
assert.deepEqual(midRunSelect.selection,{valve1:true,valve2:true},
  'a select must not reach the next start while the sequence is active');
const started=startTick(teardown({}),{payload:{kind:'start',valve1:true,valve2:false}});
assert.equal(started.selection,undefined,
  'a start during a teardown must not record a selection the operator could not change');
assert(template.includes("zoneSelected('valve1')")&&template.includes("zoneSelected('valve2')"),
  'the checkboxes must read the server-side selection');
assert(template.includes("kind:'select'"),'changing a checkbox must tell the server');
if (/v-model="valve1"/.test(template)||/v-model="valve2"/.test(template)) {
  c.fail('a component-local checkbox resets as soon as the page is left');
}
assert(template.includes('{{ runText }}'),
  'the control panel must show which valves the current run is using');

// --- the zone boxes are locked for the run ----------------------------------
// A run's valves are fixed when it starts; the sequencer opens and later closes exactly
// state.selected. Ticking a box mid-run showed a set that was not watering and looked as
// if it had been accepted, so the boxes are disabled while the sequence is active and
// display state.selected rather than the stored selection for the next start.
assert(template.includes(':disabled="zonesLocked"'),
  'the zone checkboxes must be disabled while a sequence is active');
assert(template.includes("{'ir-zone-locked':zonesLocked}"),
  'a locked zone box must be styled as locked');
const component = new Function(vueScript.replace('export default', 'return'))();
const zoneCtx = phase => ({state:{phase,selection:{valve1:true,valve2:true},selected:['valve1']},
  zonesLocked:component.computed.zonesLocked.call({state:{phase}}),
  zoneSelected:component.methods.zoneSelected,
  send(){ throw new Error('a locked zone box must not send a selection'); }});
for (const phase of ['running','open_main','wait_main_open','wait_field_open','set_frequency',
                     'wait_pump_start','stop_pump','wait_pump_stop','close_fields','close_main']) {
  const ctx=zoneCtx(phase);
  assert.equal(ctx.zonesLocked,true,`the zone boxes must be locked in ${phase}`);
  assert.equal(ctx.zoneSelected.call(ctx,'valve1'),true,`${phase} must show the valve the run is using`);
  assert.equal(ctx.zoneSelected.call(ctx,'valve2'),false,`${phase} must not show a valve the run is not using`);
  component.methods.chooseZone.call(ctx,'valve2',true);   // must not send, must not throw
}
for (const phase of ['idle','fault',undefined]) {
  const ctx=zoneCtx(phase);
  assert.equal(ctx.zonesLocked,false,`the zone boxes must stay editable in ${phase||'no phase'}`);
  assert.equal(ctx.zoneSelected.call(ctx,'valve2'),true,`${phase||'no phase'} must show the stored selection`);
  ctx.send = m => { ctx.sent = m; };
  component.methods.chooseZone.call(ctx,'valve2',false);
  assert.deepEqual(ctx.sent.payload,{kind:'select',valve1:true,valve2:false},
    `${phase||'no phase'} must still record a selection on the server`);
}
// A fault is not an active run: Start from fault closes both field valves first and
// then opens the chosen set, so the panel must say so and keep the boxes usable.
assert.equal(component.computed.zonesNote.call({state:{phase:'fault'}}),
  'Set the zones for the retry; Start closes both field valves first.');
assert.equal(component.computed.zonesNote.call({state:{phase:'idle'}}),
  'Select one half of the field or both.');
assert(template.includes('{{ zonesLocked ? '),
  'the panel heading must say the zones are locked while a run is active');

// --- a valve refused only for its id ----------------------------------------
// Being refused for the id alone means the shared counter was behind the valve's own
// view of it, which this refusal uplink has just corrected, so resend once. A
// reported actuation failure is hardware and is never retried.
const valveRefusal = (reason,retried) => {
  globalMemory.set('cmd_next_id_valve1',5);
  adaptiveMemory.set('irrigation',{phase:'wait_field_close',phaseAt:base,selected:['valve1'],step:0,
    notice:'',pending:null,valveRetried:retried,
    devices:{...commonDevices,
      valve1:{at:base,pcv_last_commanded:'open',status_reason:reason,last_command_id:305},
      pump:{at:base,communication_ok:true,configuration_valid:true,vfd_fault_code:0,running:false}}});
  Date.now=()=>base;
  const out=new Function('msg','flow','node','env','Buffer','global',control)({payload:{kind:'tick'}},adaptiveFlow,node,env,Buffer,globalCtx);
  const downlinks=(Array.isArray(out[0])?out[0]:[]).filter(m=>typeof m.topic==='string'&&m.topic.endsWith('/command/down'));
  return {ids:downlinks.map(m=>JSON.parse(m.payload).object.command_id),
    state:adaptiveMemory.get('irrigation')};
};
const idRefused=valveRefusal('invalid_command_rejected',null);
assert.deepEqual(idRefused.ids,[306],
  'a valve refused for its id must be resent once with the id it reported plus one');
assert.equal(idRefused.state.valveRetried,'valve1','the resend must be marked so it happens once');
assert.equal(idRefused.state.phase,'wait_field_close','an id refusal must not fault the sequence');
assert.deepEqual(valveRefusal('invalid_command_rejected','valve1').ids,[],
  'a second id refusal must not resend again');
const hardwareFail=valveRefusal('pcv_actuation_failed',null);
assert.deepEqual(hardwareFail.ids,[],'an actuation failure must not be retried');
assert.equal(hardwareFail.state.phase,'fault','an actuation failure must still fault at once');

// --- a report proves nothing until the valve echoes our command id ------------
// In Class A the downlink lands in the RX window *after* an uplink, so the first report
// after we send still carries the previous state. Only the echoed last_command_id shows
// that the device has acted. Live, a stop pressed 10 s after an open skipped that valve
// in the close pass and left it open while the panel claimed "valves closed".
const valveRun = (state,action) => {
  adaptiveMemory.set('irrigation',state); Date.now=()=>base;
  const out=new Function('msg','flow','node','env','Buffer','global',control)(action,adaptiveFlow,node,env,Buffer,globalCtx);
  const downs=(Array.isArray(out[0])?out[0]:[]).filter(m=>typeof m.topic==='string'&&m.topic.endsWith('/command/down'));
  return {s:adaptiveMemory.get('irrigation'),
    sent:downs.map(m=>{const b=JSON.parse(m.payload);return b.object?b.object.pcv+':'+b.object.command_id:'binary';})};
};
const pumpOff={at:base,communication_ok:true,configuration_valid:true,vfd_fault_code:0,running:false};
const closingState = (valveCmd,valve2) => ({phase:'close_fields',phaseAt:base,selected:['valve1','valve2'],step:1,
  notice:'',pending:null,pendingStart:null,valveCmd,valveRetried:null,
  devices:{...commonDevices,valve1:{at:base,pcv_last_commanded:'open',last_command_id:53},
    valve2:valve2,pump:pumpOff}});

// The open is in flight: the valve still reports closed and has not echoed our id.
globalMemory.set('cmd_next_id_valve2',100);
const inFlightOpen=valveRun(closingState({valve2:21},{at:base,pcv_last_commanded:'closed',last_command_id:20}),
  {payload:{kind:'tick'}});
assert.equal(inFlightOpen.s.phase,'wait_field_close',
  'a valve whose open is in flight must not be skipped by the close pass');
assert.deepEqual(inFlightOpen.sent,['close:100'],
  'the close pass must command that valve, or it is left open by the stop');
// Echoed back, so it may be skipped.
const acknowledged=valveRun(closingState({valve2:22},{at:base,pcv_last_commanded:'closed',last_command_id:22}),
  {payload:{kind:'tick'}});
assert.deepEqual(acknowledged.sent,[],'a valve that echoed our close must be skipped');
assert.equal(acknowledged.s.step,0);

// The mirror image: a close in flight must not be skipped by the open pass, or the pump
// would be started against a valve that is closing.
globalMemory.set('cmd_next_id_valve1',101);
const closeInFlight=valveRun({phase:'open_fields',phaseAt:base,selected:['valve1'],step:0,notice:'',
  pending:null,valveCmd:{valve1:54},
  devices:{...commonDevices,valve1:{at:base,pcv_last_commanded:'open',last_command_id:53},pump:pumpOff}},
  {payload:{kind:'tick'}});
assert.equal(closeInFlight.s.phase,'wait_field_open',
  'a valve whose close is in flight must not be skipped by the open pass');
assert.deepEqual(closeInFlight.sent,['open:101'],'the open pass must command that valve');

// A close is confirmed only by a report that echoes it.
const waiting = echo => valveRun({phase:'wait_field_close',phaseAt:base,selected:['valve1'],step:0,notice:'',
  pending:null,valveCmd:{valve1:56},
  devices:{...commonDevices,valve1:{at:base,pcv_last_commanded:'closed',last_command_id:echo},pump:pumpOff}},
  {payload:{kind:'tick'}});
assert.equal(waiting(55).s.phase,'wait_field_close',
  'a report that has not echoed the close must not confirm it');
assert.equal(waiting(56).s.phase,'close_fields','a report echoing the close must confirm it');

// --- a refusal is only ours when the report is newer than our command ---------
// Every device reports the phase or reason of its LAST command, not of the one in
// flight, and keeps reporting it in later heartbeats until a newer event replaces it.
// A refusal from an earlier cycle is therefore still in the payload when the next
// command goes out. Acting on it failed a healthy open without the valve ever being
// given its chance to move, and then reported the close that followed as
// "close unconfirmed" with the valve still shut. Both were seen live on 2026-10-07.
// MainValve names the command it answers in reported_command_id, so that is the test.
const mainRefusal = (phase,op,main,nowOffsetMs) => {
  globalMemory.set('cmd_next_id_main',510);
  adaptiveMemory.set('irrigation',{phase,phaseAt:base,selected:['valve1','valve2'],
    step:phase==='wait_main_close'?-1:0,notice:'',
    pending:{device:'main',op,id:509,at:base,retried:false},
    devices:{...commonDevices,main:{at:base,actuator_online:true,actuator_fault_code:0,overpressure:false,
      actual_angle_deg:op==='close'?90:0,valve_moving:false,...main}}});
  Date.now=()=>base+(nowOffsetMs||0);
  const out=new Function('msg','flow','node','env','Buffer','global',control)({payload:{kind:'tick'}},adaptiveFlow,node,env,Buffer,globalCtx);
  const downlinks=(Array.isArray(out[0])?out[0]:[]).filter(m=>typeof m.topic==='string'&&m.topic.endsWith('/command/down'));
  // A failing open starts the teardown, which commands the field valves in the same
  // tick, so a plain downlink count cannot tell a mis-sent main command from it.
  const toMain=downlinks.filter(m=>m.topic.includes(env.get('MAIN_DEV_EUI')));
  return {s:adaptiveMemory.get('irrigation'),sent:downlinks.length,toMain:toMain.length};
};
const staleOpen=mainRefusal('wait_main_open','open',
  {command_phase:'rejected',reason:'pressure_interlock',reported_command_id:400,last_command_id:400});
assert.equal(staleOpen.s.phase,'wait_main_open','a refusal echoing another command id must not fail the open');
assert.equal(staleOpen.sent,0,'a refusal echoing another command id must not be resent');
assert.equal(staleOpen.s.notice,'','a stale refusal must not rewrite the notice');
const staleClose=mainRefusal('wait_main_close','close',
  {command_phase:'rejected',reason:'pressure_interlock',reported_command_id:400,last_command_id:400});
assert.equal(staleClose.s.phase,'wait_main_close','a refusal echoing another command id must not fault the close');
assert.equal(staleClose.s.notice,'','a stale refusal must not claim the close was unconfirmed');
const staleFailed=mainRefusal('wait_main_open','open',
  {command_phase:'failed',reason:'movement_timeout',reported_command_id:400,last_command_id:400});
assert.equal(staleFailed.s.phase,'wait_main_open','a failure from an earlier command must not fail this open');
// A report that predates the command cannot answer it, even if it echoes the right id.
const olderReport=mainRefusal('wait_main_open','open',
  {at:base-5000,command_phase:'rejected',reason:'invalid_command',reported_command_id:509,last_command_id:400});
assert.equal(olderReport.s.phase,'wait_main_open','a report older than the command must not act on it');
assert.equal(olderReport.sent,0,'a report older than the command must not be resent to');
// Ours, and refused for the id alone: the one case worth resending.
const idRefusedOpen=mainRefusal('wait_main_open','open',
  {command_phase:'rejected',reason:'invalid_command',reported_command_id:509,last_command_id:400});
assert.equal(idRefusedOpen.toMain,1,'an id refusal that echoes our command must be resent once');
assert.equal(idRefusedOpen.s.phase,'wait_main_open','resending must not move the sequence on');
assert.equal(idRefusedOpen.s.pending.retried,true,'the resend must be marked as the single retry');
// Ours, and refused by the hardware or the pressure interlock: never retried. The
// guide is explicit that a close blocked by upstream overpressure must not simply be
// resent, so the sequence faults and names the reason instead.
const interlock=mainRefusal('wait_main_close','close',
  {command_phase:'rejected',reason:'pressure_interlock',reported_command_id:509,last_command_id:400});
assert.equal(interlock.sent,0,'a pressure interlock must not be resent');
assert.equal(interlock.s.phase,'fault','a pressure interlock must fault the close');
assert.equal(interlock.s.notice,'Main valve close failed: pressure_interlock',
  'the fault must name the reason the valve reported');
const busy=mainRefusal('wait_main_open','open',
  {valve_moving:true,command_phase:'rejected',reason:'actuator_busy',reported_command_id:509,last_command_id:400});
assert.equal(busy.toMain,0,'a busy actuator must not be resent to');
assert.equal(busy.s.stopReason,'Main valve open failed: actuator_busy',
  'a busy actuator must start the shutdown and name the reason');
const failedOpen=mainRefusal('wait_main_open','open',
  {command_phase:'failed',reason:'actuator_fault',reported_command_id:509,last_command_id:509});
assert.equal(failedOpen.s.stopReason,'Main valve open failed: actuator_fault',
  'an actuator failure must be named in the notice');

// A valve refusal that arrived before our resend cannot be the answer to it, or the
// retry would fault on the very report that justified it. The valve has refused once
// (that report is still the newest thing it has said), we resent at base+500 and the
// valve has not answered yet.
const staleAfterRetry = () => {
  globalMemory.set('cmd_next_id_valve1',106);
  adaptiveMemory.set('irrigation',{phase:'wait_field_close',phaseAt:base,selected:['valve1'],step:0,
    notice:'',pending:null,valveRetried:'valve1',valveCmd:{valve1:106},valveSentAt:{valve1:base+500},
    devices:{...commonDevices,
      valve1:{at:base,pcv_last_commanded:'open',status_reason:'invalid_command_rejected',last_command_id:105},
      pump:pumpOff}});
  Date.now=()=>base+1000;
  const out=new Function('msg','flow','node','env','Buffer','global',control)({payload:{kind:'tick'}},adaptiveFlow,node,env,Buffer,globalCtx);
  const downlinks=(Array.isArray(out[0])?out[0]:[]).filter(m=>typeof m.topic==='string'&&m.topic.endsWith('/command/down'));
  return {s:adaptiveMemory.get('irrigation'),sent:downlinks.length};
};
assert.equal(staleAfterRetry().s.phase,'wait_field_close',
  'a refusal older than our resend must not fault the sequence');
assert.equal(staleAfterRetry().sent,0,'a refusal older than our resend must not be resent to again');

// The pump stores an id only for a command it accepted, so a refused command carries
// no id at all and freshness is the only test. A result that predates the command is
// not its answer and must not spend the single retry.
const stalePump = () => {
  globalMemory.set('cmd_next_id_pump',5);
  adaptiveMemory.set('irrigation',{phase:'wait_pump_start',phaseAt:base,selected:['valve1'],step:0,notice:'',
    pending:{device:'pump',op:'start',id:500,at:base,retried:false},
    devices:{...commonDevices,pump:{at:base-1000,communication_ok:true,configuration_valid:true,
      vfd_fault_code:0,running:false,frequency_armed:true,last_command_id:305,command_result:'invalid'}}});
  Date.now=()=>base;
  const out=new Function('msg','flow','node','env','Buffer','global',control)({payload:{kind:'tick'}},adaptiveFlow,node,env,Buffer,globalCtx);
  const downlinks=(Array.isArray(out[0])?out[0]:[]).filter(m=>typeof m.topic==='string'&&m.topic.endsWith('/command/down'));
  return {s:adaptiveMemory.get('irrigation'),sent:downlinks.length};
};
assert.equal(stalePump().sent,0,'a pump result older than the command must not be resent to');
assert.equal(stalePump().s.notice,'','a stale pump refusal must not rewrite the notice');

Date.now=realNow;
console.log('Irrigation sequence smoke test passed');
