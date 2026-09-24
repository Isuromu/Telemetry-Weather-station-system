"""Build the single-page Node-RED Dashboard 2 irrigation flow."""

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "examples" / "IntegratedDashboard" / "irrigation_dashboard_flow.json"


def node(node_id, kind, **kwargs):
    return {"id": node_id, "type": kind, **kwargs}


SETTINGS = r'''// Edit all site thresholds, limits and timeouts here (seconds unless noted).
const settings = {
  startLevelCm: 20, stopLevelCm: 19,
  startMoisturePercent: 70, stopMoisturePercent: 90,
  mainOpenMinDeg: 45, mainOpenMaxDeg: 90, mainOpenTargetDeg: 90,
  mainClosedMaxDeg: 5,
  pumpFrequencyHz: 25, pumpMinFrequencyHz: 10, pumpMaxFrequencyHz: 50,
  pumpFrequencyToleranceHz: 0.2,
  waterMaxAgeSec: 1200, soilMaxAgeSec: 1200,
  mainMaxAgeSec: 150,
  pumpRunningMaxAgeSec: 45, pumpStoppedMaxAgeSec: 75,
  valve1MaxAgeSec: 1200, valve2MaxAgeSec: 180,
  mainCommandTimeoutSec: 300, fieldValveCommandTimeoutSec: 2100,
  pumpCommandTimeoutSec: 150, pumpStopTimeoutSec: 150,
  maxWateringSec: 14400
};
flow.set('irrigation_settings', settings);
msg.payload = {kind:'tick'};
return msg;'''


CONTROL = r'''// Single owner of irrigation sequencing and safety decisions.
const cfg = flow.get('irrigation_settings');
if (!cfg) { node.status({fill:'red',shape:'ring',text:'settings missing'}); return null; }
const keys = ['water','soil','main','valve1','valve2','pump'];
const app = String(env.get('IRRIGATION_APP_ID')||'').toLowerCase();
const ids = {};
for (const k of keys) {
  ids[k] = {eui:String(env.get(k.toUpperCase()+'_DEV_EUI')||'').toLowerCase()};
}
const configured = /^[a-f0-9-]{36}$/.test(app) &&
  keys.every(k => /^[a-f0-9]{16}$/.test(ids[k].eui));
let s = flow.get('irrigation') || {phase:'idle',devices:{},selected:[],step:0,notice:'Waiting for telemetry',pending:null};
const now = Date.now();
const downlinks = [];
const emit = (k,port,body) => downlinks.push({topic:`application/${app}/device/${ids[k].eui}/command/down`,payload:JSON.stringify({devEui:ids[k].eui,confirmed:false,fPort:port,...body}),qos:'0',retain:false});
const device = k => s.devices[k] || {};
const fresh = (k,age) => !!device(k).at && now-device(k).at <= age*1000;
const pumpMaxAge = () => device('pump').running === true ? cfg.pumpRunningMaxAgeSec : cfg.pumpStoppedMaxAgeSec;
const pumpFresh = () => fresh('pump',pumpMaxAge());
const num = v => typeof v === 'number' && Number.isFinite(v) ? v : null;
const nextId = k => {let n=flow.get('irrigation_id_'+k); if (!Number.isInteger(n)||n<0||n>65534) {const last=num(device(k).last_command_id);n=last===null?1:(last+1)%65535;} flow.set('irrigation_id_'+k,(n+1)%65535);return n;};
function command(k,op) {
  const id=nextId(k); s.pending={device:k,op,id,at:now};
  if (k==='main') {const angle=op==='open'?cfg.mainOpenTargetDeg:0, a=Math.round(angle*10);emit(k,30,{data:Buffer.from([1,1,a>>8,a&255,id>>8,id&255]).toString('base64')});}
  else if (k==='pump') {const code={stop:1,estop:2,frequency:3,start:4}[op];const arg=op==='frequency'?Math.round(cfg.pumpFrequencyHz*100):0;emit(k,50,{data:Buffer.from([1,code,id>>8,id&255,arg>>8,arg&255]).toString('base64')});}
  else emit(k,30,{object:{pcv:op,command_id:id}});
  s.notice=`${k}: ${op} sent; awaiting device result`;
}
function advance(phase) {s.phase=phase;if (!phase.startsWith('wait_')) s.pending=null;s.phaseAt=now;}
function stop(reason) {s.notice=reason;s.stopReason=reason;if (s.phase==='idle') s.selected=['valve1','valve2']; if (s.phase!=='stop_pump'&&s.phase!=='wait_pump_stop'&&s.phase!=='close_fields'&&s.phase!=='wait_field_close'&&s.phase!=='close_main'&&s.phase!=='wait_main_close') advance('stop_pump');}
function ready() {
  const w=device('water'), t=device('soil'), m=device('main'), p=device('pump');
  if (!configured) return 'Set IRRIGATION_APP_ID and all six DEV_EUI environment variables';
  if (!fresh('water',cfg.waterMaxAgeSec)||w.pressure_valid!==true||num(w.depth_m)===null) return 'Water level unavailable or stale';
  if (w.depth_m*100<cfg.startLevelCm) return 'Reservoir below start level';
  if (!fresh('soil',cfg.soilMaxAgeSec)||t.sensor_valid!==true||num(t.vwc_percent)===null) return 'Soil moisture unavailable or stale';
  if (t.vwc_percent>=cfg.startMoisturePercent) return 'Soil is not dry enough';
  if (!fresh('main',cfg.mainMaxAgeSec)||m.actuator_online!==true||m.overpressure===true||num(m.actuator_fault_code)!==0) return 'Main valve offline, stale or faulted';
  if (!pumpFresh()||p.communication_ok!==true||p.configuration_valid!==true||num(p.vfd_fault_code)!==0||p.running===true) return 'Pump offline, running or faulted';
  if (!p.frequency_armed && !(cfg.pumpFrequencyHz>=cfg.pumpMinFrequencyHz&&cfg.pumpFrequencyHz<=cfg.pumpMaxFrequencyHz)) return 'Pump frequency invalid';
  for (const k of s.selected) if (!fresh(k,cfg[k+'MaxAgeSec'])) return k+' status unavailable or stale';
  return '';
}
function decodePump(e) {
  let b;
  try { b=Buffer.from(e.data||'', 'base64'); } catch (_) { return null; }
  if (!((b.length===17&&b[0]===1)||(b.length===22&&b[0]===2))) return null;
  const u16=i=>(b[i]<<8)|b[i+1];
  const s16=i=>{const v=u16(i);return v>=32768?v-65536:v;};
  const results=['none','accepted','failed','duplicate','invalid','storage_error','in_progress'];
  const states=['unknown','forward','reverse','stopped'];
  const d={communication_ok:!!(b[1]&1),configuration_valid:!!(b[1]&2),running:!!(b[1]&4),frequency_armed:!!(b[1]&8),lorawan_active:!!(b[1]&16),class_c_active:!!(b[1]&32),command_result:results[b[2]]||'unknown',last_command_id:u16(3)===65535?null:u16(3),commanded_frequency_hz:u16(5)/100,actual_frequency_hz:u16(7)/100,motor_current_a:u16(9)/100,vfd_fault_code:u16(11),output_voltage_v:u16(13)/10,run_state:states[b[15]]||'unknown',communication_error_code:b[16]};
  if (b[0]===2) {const joinError=s16(17);d.previous_join_error_code=joinError;d.previous_join_error=joinError===-1116?'No OTAA JoinAccept received in RX1/RX2':joinError===0?'None':'RadioLib error '+joinError;d.join_attempt_count=b[19];d.previous_join_retry_seconds=u16(20);}
  return d;
}
function decode(k,e) {
  let d=e.object;
  if (k==='pump') {
    const raw=decodePump(e);
    if (raw) {
      if (!d||typeof d!=='object') d=raw;
      else {const decodedResult=d.command_result;d={...raw,...d};if(decodedResult==='unknown'&&raw.command_result==='in_progress')d.command_result='in_progress';}
    }
  }
  if (!d||typeof d!=='object') return null;
  if ((k==='water'&&e.fPort!==40)||(k==='soil'&&e.fPort!==10)||(k==='main'&&e.fPort!==31)||(k==='pump'&&e.fPort!==51)||((k==='valve1'||k==='valve2')&&e.fPort!==31)) return null;
  return d;
}
if (msg.topic && msg.topic.startsWith('application/')) {
  const p=msg.topic.toLowerCase().split('/');
  if (p.length===6&&p[0]==='application'&&p[2]==='device'&&p[4]==='event'&&p[5]==='up') {
    const k=keys.find(x=>app===p[1]&&ids[x].eui===p[3]);
    if (k) {let e=msg.payload;try {if(Buffer.isBuffer(e)) e=JSON.parse(e.toString('utf8'));else if(typeof e==='string') e=JSON.parse(e);}catch(err){e=null;}
      const d=e&&decode(k,e);if(d){s.devices[k]={...s.devices[k],...d,at:now,last_seen:e.time||new Date(now).toISOString(),fPort:e.fPort};if(k==='pump'&&s.pumpRefresh?.pending)s.pumpRefresh={...s.pumpRefresh,pending:false,result:'Status received',completedAt:now};}}
  }
} else if (msg.payload&&msg.payload.kind==='start') {
  if (s.phase!=='idle') s.notice='Irrigation is already active';
  else {s.selected=['valve1','valve2'].filter(k=>msg.payload[k]===true);s.step=0;
    if (!s.selected.length) s.notice='Select at least one field valve';
    else {const problem=ready();if(problem) s.notice=problem;else {advance('open_main');s.notice='Start checks passed';}}}
} else if (msg.payload&&msg.payload.kind==='stop') stop('Operator requested stop');
else if (msg.payload&&msg.payload.kind==='refresh_pump') {
  const previous=Number(s.pumpRefresh?.requestedAt||0);
  if (!configured) s.pumpRefresh={pending:false,result:'Configure the application ID and all six DevEUIs first'};
  else if (now-previous<10000) s.pumpRefresh={...s.pumpRefresh,result:`Refresh available in ${Math.ceil((10000-(now-previous))/1000)} s`};
  else if (s.pumpRefresh?.pending&&now-previous<30000) s.pumpRefresh={...s.pumpRefresh,result:'A status request is already pending'};
  else {emit('pump',50,{data:Buffer.from([1,5]).toString('base64')});s.pumpRefresh={pending:true,requestedAt:now,result:'Requesting status'};}
}
if (msg.status && typeof msg.status.text==='string') s.mqtt_status=msg.status.text;
if (s.pumpRefresh?.pending&&now-Number(s.pumpRefresh.requestedAt||0)>=30000) s.pumpRefresh={...s.pumpRefresh,pending:false,result:'No status response received'};
const w=device('water'),t=device('soil'),m=device('main'),p=device('pump');
if (s.phase==='idle'&&p.running===true&&pumpFresh()) stop('Pump running outside dashboard sequence; stopping');
const mainOpen=()=>num(m.actual_angle_deg)!==null&&m.actual_angle_deg>=cfg.mainOpenMinDeg&&m.actual_angle_deg<=cfg.mainOpenMaxDeg&&m.valve_moving===false&&m.actuator_online===true&&num(m.actuator_fault_code)===0;
const done=(k,op)=>{const q=s.pending,d=device(k);if(!q||q.device!==k||q.op!==op||num(d.last_command_id)!==q.id||d.at<q.at)return false;
  if(k==='main') return d.reported_command_id===q.id&&d.command_phase==='finished'&&d.valve_moving===false&&(op==='open'?mainOpen():num(d.actual_angle_deg)!==null&&d.actual_angle_deg<=cfg.mainClosedMaxDeg);
  if(k==='pump') return d.command_result==='accepted'&&(op==='stop'||op==='estop'?d.running===false:op==='start'?d.running===true:op==='frequency'?d.frequency_armed===true:false);
  return d.pcv_last_commanded===op&&d.status_reason!=='invalid_command';};
const timed=(seconds)=>s.pending&&now-s.pending.at>seconds*1000;
if (s.phase==='running') {
  if (now-s.runAt>cfg.maxWateringSec*1000) stop('Maximum watering duration reached');
  else if (!fresh('water',cfg.waterMaxAgeSec)||w.pressure_valid!==true||num(w.depth_m)===null||w.depth_m*100<=cfg.stopLevelCm) stop('Water level low or unavailable');
  else if (!fresh('soil',cfg.soilMaxAgeSec)||t.sensor_valid!==true||num(t.vwc_percent)===null||t.vwc_percent>=cfg.stopMoisturePercent) stop('Soil wet enough or reading unavailable');
  else if (!fresh('main',cfg.mainMaxAgeSec)||!mainOpen()||m.overpressure===true) stop('Main valve unsafe');
  else if (!pumpFresh()||p.communication_ok!==true||num(p.vfd_fault_code)!==0||p.running!==true) stop('Pump status unsafe');
  else if (s.selected.some(k=>!fresh(k,cfg[k+'MaxAgeSec'])||device(k).pcv_last_commanded!=='open')) stop('Field valve status unsafe');
}
if (s.phase==='open_main') {if(mainOpen()) advance('open_fields');else {command('main','open');advance('wait_main_open');}}
if (s.phase==='wait_main_open') {if(done('main','open')) advance('open_fields');else if(timed(cfg.mainCommandTimeoutSec)||m.command_phase==='failed'||m.command_phase==='rejected') stop('Main valve open failed');}
if (s.phase==='open_fields') {if(s.step>=s.selected.length) advance('set_frequency');else {const k=s.selected[s.step];if(device(k).pcv_last_commanded==='open'&&fresh(k,cfg[k+'MaxAgeSec'])) {s.step++;}else {command(k,'open');advance('wait_field_open');}}}
if (s.phase==='wait_field_open') {if(done(s.selected[s.step],'open')) {s.step++;advance('open_fields');}else if(timed(cfg.fieldValveCommandTimeoutSec)) stop('Field valve open unconfirmed');}
if (s.phase==='set_frequency') {const problem=ready();if(problem){stop('Start recheck: '+problem);}else if(p.frequency_armed===true&&Math.abs((num(p.commanded_frequency_hz)||0)-cfg.pumpFrequencyHz)<cfg.pumpFrequencyToleranceHz) advance('start_pump');else {command('pump','frequency');advance('wait_frequency');}}
if (s.phase==='wait_frequency') {if(done('pump','frequency')) advance('start_pump');else if(timed(cfg.pumpCommandTimeoutSec)) stop('Pump frequency command unconfirmed');}
if (s.phase==='start_pump') {const problem=ready();if(problem) stop('Start recheck: '+problem);else if(!mainOpen()||s.selected.some(k=>device(k).pcv_last_commanded!=='open')) stop('Valve readiness lost');else {command('pump','start');advance('wait_pump_start');}}
if (s.phase==='wait_pump_start') {if(done('pump','start')) {advance('running');s.runAt=now;s.notice='Watering';}else if(p.command_result==='in_progress'&&num(p.last_command_id)===s.pending?.id)s.notice='Pump start accepted; waiting for final VFD condition';else if(timed(cfg.pumpCommandTimeoutSec)) stop('Pump start unconfirmed');}
if (s.phase==='stop_pump') {if(p.running===false&&pumpFresh()) {s.step=s.selected.length-1;advance('close_fields');}else {command('pump','stop');advance('wait_pump_stop');}}
if (s.phase==='wait_pump_stop') {if((done('pump','stop')||done('pump','estop'))&&pumpFresh()) {s.step=s.selected.length-1;advance('close_fields');}else if(p.command_result==='in_progress'&&num(p.last_command_id)===s.pending?.id)s.notice='Pump stop accepted; waiting for final VFD condition';else if(timed(cfg.pumpStopTimeoutSec)){if(s.pending.op==='stop'){command('pump','estop');s.notice='Pump stop unconfirmed; emergency stop sent';}else {s.phase='fault';s.notice='Pump stop unconfirmed. Valves left open; inspect pump.';}}}
if (s.phase==='close_fields') {if(s.step<0) advance('close_main');else {const k=s.selected[s.step];if(device(k).pcv_last_commanded==='close'&&fresh(k,cfg[k+'MaxAgeSec'])) s.step--;else {command(k,'close');advance('wait_field_close');}}}
if (s.phase==='wait_field_close') {if(done(s.selected[s.step],'close')) {s.step--;advance('close_fields');}else if(timed(cfg.fieldValveCommandTimeoutSec)){s.phase='fault';s.notice='Field valve close unconfirmed; inspect system';}}
if (s.phase==='close_main') {if(num(m.actual_angle_deg)!==null&&m.actual_angle_deg<=cfg.mainClosedMaxDeg&&m.valve_moving===false) advance('idle');else {command('main','close');advance('wait_main_close');}}
if (s.phase==='wait_main_close') {if(done('main','close')) {advance('idle');s.notice='Watering stopped; valves closed';}else if(timed(cfg.mainCommandTimeoutSec)){s.phase='fault';s.notice='Main valve close unconfirmed; inspect system';}}
s.configured=configured;s.settings=cfg;s.check=ready();s.devices=Object.fromEntries(keys.map(k=>[k,{...device(k),stale:k==='pump'?!pumpFresh():!fresh(k,cfg[k+'MaxAgeSec'])}]));
flow.set('irrigation',s);
return [downlinks.length ? downlinks : null, {payload:{kind:'state',state:s}}];'''


# Do not rename the `cardValue` method to `value`: Dashboard 2's ui-template render
# context already binds `value`, which shadows a user method of that name and throws
# "TypeError: value is not a function" during render - the widget then renders empty,
# which looks like a completely blank page with no visible error in the dashboard.
# Keep visual changes separate from the safety sequencer. Dashboard 2.0 binds
# `value` in a ui-template render context; do not name a Vue method `value`.
TEMPLATE = (ROOT / "examples/IntegratedDashboard/docs/dashboard_template.html").read_text(encoding="utf-8")

SUBSCRIBE = r'''// Subscribe only to the configured ChirpStack application.
const app = String(env.get('IRRIGATION_APP_ID') || '').toLowerCase();
if (!/^[a-f0-9-]{36}$/.test(app)) {
  node.status({fill:'red',shape:'ring',text:'IRRIGATION_APP_ID missing'});
  return null;
}
const topic = `application/${app}/device/+/event/up`;
if (context.get('topic') === topic) return null;
context.set('topic', topic);
node.status({fill:'green',shape:'dot',text:topic});
return {action:'subscribe',topic,qos:0};'''


def build():
    flow_id = "irrigation_single_page"
    broker = "irrigation_broker"
    page = "irrigation_page"
    group = "irrigation_group"
    nodes = [node(flow_id, "tab", label="Irrigation system", disabled=False, info="Single-page safety sequencer for six LoRaWAN nodes.")]
    nodes += [
        node("irrigation_tick", "inject", z=flow_id, name="Refresh safety and settings", props=[{"p":"payload"}], repeat="5", crontab="", once=True, onceDelay="0.5", topic="", payload="", payloadType="date", x=140, y=80, wires=[["irrigation_settings", "irrigation_subscribe"]]),
        node("irrigation_settings", "function", z=flow_id, name="Irrigation settings — edit limits here", func=SETTINGS, outputs=1, timeout=0, noerr=0, initialize="", finalize="", libs=[], x=400, y=80, wires=[["irrigation_control"]]),
        node("irrigation_subscribe", "function", z=flow_id, name="Subscribe to configured ChirpStack application", func=SUBSCRIBE, outputs=1, timeout=0, noerr=0, initialize="", finalize="", libs=[], x=400, y=120, wires=[["irrigation_mqtt_in"]]),
        node("irrigation_mqtt_in", "mqtt in", z=flow_id, name="All six ChirpStack uplinks", topic="", qos="0", datatype="auto-detect", broker=broker, nl=False, rap=True, rh=0, inputs=1, x=150, y=140, wires=[["irrigation_control"]]),
        node("irrigation_mqtt_status", "status", z=flow_id, name="MQTT connection", scope=["irrigation_mqtt_in", "irrigation_mqtt_out"], x=150, y=200, wires=[["irrigation_control"]]),
        node("irrigation_control", "function", z=flow_id, name="Safety checks and irrigation sequence", func=CONTROL, outputs=2, timeout=0, noerr=0, initialize="", finalize="", libs=[], x=430, y=160, wires=[["irrigation_mqtt_out"],["irrigation_ui"]]),
        node("irrigation_mqtt_out", "mqtt out", z=flow_id, name="ChirpStack downlinks", topic="", qos="", retain="", respTopic="", contentType="", userProps="", correl="", expiry="", broker=broker, x=750, y=120, wires=[]),
        node("irrigation_ui", "ui-template", z=flow_id, group=group, page="", ui="", name="Compact irrigation dashboard", order=1, width="12", height="14", head="", format=TEMPLATE, storeOutMessages=True, passthru=False, resendOnRefresh=True, templateScope="local", className="", x=760, y=200, wires=[["irrigation_control"]]),
        node(broker, "mqtt-broker", name="ChirpStack MQTT", broker="localhost", port="1883", clientid="", autoConnect=True, usetls=False, protocolVersion="4", keepalive="60", cleansession=True, autoUnsubscribe=True, birthTopic="", birthQos="0", birthPayload="", birthMsg={"topic":"","payload":""}, closeTopic="", closeQos="0", closePayload="", closeMsg={"topic":"","payload":""}, willTopic="", willQos="0", willPayload="", willMsg={"topic":"","payload":""}),
        node(group, "ui-group", name="Irrigation", page=page, width="12", height="14", order=1, showTitle=False, className="", visible=True, disabled=False, groupType="default"),
        node(page, "ui-page", name="Irrigation", ui="irrigation_base", path="/irrigation", icon="water", layout="grid", theme="irrigation_theme", order=1, className="", visible=True, disabled=False),
    ]
    source = json.loads((ROOT / "examples/PumpControl/include/pump_control_dashboard2_flow.json").read_text(encoding="utf-8"))
    for old_type, new_id in [("ui-base", "irrigation_base"), ("ui-theme", "irrigation_theme"), ("ui-breakpoint", "irrigation_breakpoint")]:
        original = next((x for x in source if x.get("type") == old_type), None)
        if original:
            clone = {**original, "id": new_id}
            if old_type == "ui-base":
                clone["name"] = "Irrigation dashboard"
                clone["path"] = "/irrigation-dashboard"
            nodes.append(clone)
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(json.dumps(nodes, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(OUTPUT)


if __name__ == "__main__":
    build()
