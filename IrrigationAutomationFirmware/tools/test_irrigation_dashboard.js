// Safety-sequence smoke test for the generated Node-RED function.
const fs = require('fs');
const assert = require('assert');
const nodes = JSON.parse(fs.readFileSync('examples/IntegratedDashboard/irrigation_dashboard_flow.json', 'utf8'));
const byId = Object.fromEntries(nodes.map(n => [n.id, n]));
for (const group of nodes.filter(n => n.type === 'ui-group')) {
  const page = byId[group.page];
  assert(page && page.type === 'ui-page', `ui-group ${group.id} has no valid page`);
  const base = byId[page.ui];
  assert(base && base.type === 'ui-base', `ui-page ${page.id} has no valid base`);
}
const settings = nodes.find(n => n.id === 'irrigation_settings').func;
const control = nodes.find(n => n.id === 'irrigation_control').func;
const template = nodes.find(n => n.id === 'irrigation_ui').format;
const subscribeNode = byId.irrigation_subscribe;
assert.deepEqual(byId.irrigation_tick.wires[0], ['irrigation_settings', 'irrigation_subscribe']);
assert.equal(byId.irrigation_mqtt_in.inputs, 1);
assert.equal(byId.irrigation_mqtt_in.topic, '');
assert.deepEqual(subscribeNode.wires[0], ['irrigation_mqtt_in']);
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
adaptiveMemory.set('irrigation',{phase:'idle',selected:[],step:0,notice:'Ready',pending:null,devices:{...commonDevices,pump:{at:base,communication_ok:true,configuration_valid:true,vfd_fault_code:0,running:false}}});
Date.now=()=>base+74000;
new Function('msg','flow','node','env','Buffer',control)({payload:{kind:'tick'}},adaptiveFlow,node,env,Buffer);
assert.equal(adaptiveMemory.get('irrigation').devices.pump.stale,false);
Date.now=()=>base+76000;
new Function('msg','flow','node','env','Buffer',control)({payload:{kind:'tick'}},adaptiveFlow,node,env,Buffer);
assert.equal(adaptiveMemory.get('irrigation').devices.pump.stale,true);
Date.now=realNow;
console.log('Irrigation sequence smoke test passed');
