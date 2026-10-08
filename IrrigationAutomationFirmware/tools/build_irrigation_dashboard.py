"""Build the single-page Node-RED Dashboard 2 irrigation flow."""

import json
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "examples" / "IntegratedDashboard" / "irrigation_dashboard_flow.json"
sys.path.insert(0, str(Path(__file__).resolve().parent))

# The water tile goes stale on the WaterLevel node's own schedule, and that
# interval changes between bench and field, so the limit is derived rather than
# typed in. See tools/water_level_interval.py.
from water_level_interval import stale_after_seconds as water_stale_after_seconds


def node(node_id, kind, **kwargs):
    return {"id": node_id, "type": kind, **kwargs}


SETTINGS = r'''// Edit all site thresholds, limits and timeouts here (seconds unless noted).
// waterMaxAgeSec is generated to match the WaterLevel node's reporting interval;
// change that interval in tools/water_level_interval.py and regenerate.
const settings = {
  startLevelCm: 20, stopLevelCm: 19,
  startMoisturePercent: 70, stopMoisturePercent: 90,
  mainOpenMinDeg: 45, mainOpenMaxDeg: 90, mainOpenTargetDeg: 90,
  mainClosedMaxDeg: 5,
  pumpFrequencyHz: 25, pumpMinFrequencyHz: 10, pumpMaxFrequencyHz: 50,
  pumpFrequencyToleranceHz: 0.2,
  waterMaxAgeSec: ''' + str(water_stale_after_seconds()) + r''', soilMaxAgeSec: 1200,
  // Clears two of MainValve's 60 s heartbeats (STATUS_INTERVAL_MS in
  // examples/MainValve/src/main.cpp), so one lost uplink does not mark a healthy
  // valve stale at build_irrigation_dashboard.py:89 or stop a run at :154. That
  // heartbeat is what keeps this limit workable: at the 300 s it used to be, this
  // value could never clear two uplinks. Keep the heartbeat at 2x this value or
  // less if either number is ever changed.
  mainMaxAgeSec: 150,
  // The pump's stopped state carries two limits, because two questions get asked of
  // it. pumpStoppedMaxAgeSec is the one the stop sequence uses to decide it may trust
  // the last "stopped" report enough to close the valves; it stays tight, because the
  // cost of being wrong is closing the main valve onto a running pump. Everywhere that
  // only asks whether the pump is alive -- the Start gate and the card -- reads
  // pumpOnlineMaxAgeSec instead. While stopped the node reports once a minute
  // (STATUS_INTERVAL_STOPPED_MS in examples/PumpControl/src/main.cpp), so 75 s is
  // 1.25 intervals and one lost uplink sends a fresh stop command before valve closure;
  // 180 s is three intervals for the Start gate and card.
  pumpRunningMaxAgeSec: 45, pumpStoppedMaxAgeSec: 75, pumpOnlineMaxAgeSec: 180,
  // The field valves split by state, like the pump above: 45 s is three of the
  // 15 s cadence a PCV uses while its valve is open
  // (VALVE_OPEN_REPORT_INTERVAL_SECONDS in PressureNodeConfig.h and
  // PressureNode2Config.h), 180 s is three of the 60 s interval it is configured
  // with while closed. Change either firmware cadence and one of these must move.
  valveOpenMaxAgeSec: 45, valveClosedMaxAgeSec: 180,
  // These two are failure detectors, not retries: neither wait_field phase ever
  // re-sends, so this is only how long a sequence that looks frozen runs before
  // the operator is told the downlink was lost. fieldValveCommandTimeoutSec is
  // twice the field valves' 60 s production interval: an open command waits for
  // the node's next wake, so the deadline has to clear the interval the node is
  // actually sleeping, and at exactly 2x one lost downlink spends the whole
  // budget. Raise it if an operator moves a valve off the 60 s interval by
  // downlink. The 2100 s this used to carry meant 35 minutes of silence with the
  // main valve open, in both the open and the close case.
  mainCommandTimeoutSec: 300, fieldValveCommandTimeoutSec: 120,
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
// pumpFresh judges the pump's condition: it is the limit the stop sequence uses to
// decide it may believe the last "stopped" report and close the valves, so it stays
// tight. pumpLive only asks whether the pump is talking to us at all, and reads the
// looser stopped-state limit; the running side of both is the same 45 s.
const pumpFresh = () => fresh('pump',pumpMaxAge());
const pumpLive = () => fresh('pump',device('pump').running===true?cfg.pumpRunningMaxAgeSec:cfg.pumpOnlineMaxAgeSec);
const isValve = k => k==='valve1'||k==='valve2';
// The PCV codec names the uplink states open/closed while the downlink action is
// open/close, so a state comparison must not reuse the action word.
const pcvState = op => op==='close' ? 'closed' : op;
// The reasons a PCV reports when it refused the command outright: the actuation
// failed, or the id was one it had already accepted, which means this counter fell
// behind the valve's own dashboard. Either way the state will never change.
const valveRefused = d => d.status_reason==='pcv_actuation_failed'||d.status_reason==='invalid_command_rejected';
// Being refused for the id alone means the shared counter was behind the valve's own
// view of it, which the refusal uplink has now corrected, so resend once. A reported
// actuation failure is hardware and is not retried.
const valveIdRefused = d => d.status_reason==='invalid_command_rejected';
const retryValveOnce=(k,op)=>{if (s.valveRetried===k) return false;command(k,op);s.valveRetried=k;return true;};
// The field valves report on a short fixed cadence while a valve is open and on
// their configured interval while closed, so one limit cannot fit both states.
// The last commanded state is the discriminator, as it is on the device.
const valveMaxAge = k => device(k).pcv_last_commanded === 'open' ? cfg.valveOpenMaxAgeSec : cfg.valveClosedMaxAgeSec;
const valveFresh = k => fresh(k,valveMaxAge(k));
// A PCV reports the state it last *commanded*, not where it is, and in Class A the
// downlink arrives in the RX window *after* an uplink: the first report following our
// command still carries the previous state. A timestamp cannot tell that report from
// one the device has acted on. The device echoes the id of the last command it
// accepted, so only an echo at least as new as the id we sent proves it acted. Without
// that, a close pass skips a valve whose open is in flight -- leaving it open -- and a
// close wait confirms a close the device never made.
const valveReported = (k,d,state) => {
  if (!valveFresh(k)||d.pcv_last_commanded!==state) return false;
  const sent=s.valveCmd?.[k];
  if (sent===undefined) return true;
  const echo=num(d.last_command_id);
  return echo!==null&&!idAhead(sent,echo);
};
const num = v => typeof v === 'number' && Number.isFinite(v) ? v : null;
// One counter per device, in global rather than flow context: five dashboards
// command the same hardware, flow context is scoped to a tab, and the device
// refuses an id it has already seen, so per-dashboard counters drift apart as soon
// as one dashboard is used more than another.
const nextKey = k => 'cmd_next_id_'+k;
// The pump accepts an id only when it is 1..32767 ahead of the last one it took --
// its rule is a signed 16-bit difference, so 65534 -> 0 counts as ahead -- and an
// equal id is refused. Compare modularly; "greater than" is wrong across the wrap.
const idAhead = (n,last) => {const d=(n-last+65535)%65535;return d!==0&&d<32768;};
const nextId = k => {const last=num(device(k).last_command_id);let n=global.get(nextKey(k));
  if (!Number.isInteger(n)||n<0||n>65534||(last!==null&&!idAhead(n,last))) n=last===null?1:(last+1)%65535;
  global.set(nextKey(k),(n+1)%65535);return n;};
function command(k,op) {
  const id=nextId(k);
  // The codec rejects a downlink without an integer command_id, so a valve still
  // gets one; only the main valve and the pump answer with an id we have to match.
  // A PCV is confirmed by the state it reports, so it carries no pending entry.
  if (isValve(k)) {emit(k,30,{object:{pcv:op,command_id:id}});if(!s.valveCmd)s.valveCmd={};s.valveCmd[k]=id;}
  else {
    s.pending={device:k,op,id,at:now,retried:false};
    if (k==='main') {const angle=op==='open'?cfg.mainOpenTargetDeg:0, a=Math.round(angle*10);emit(k,30,{data:Buffer.from([1,1,a>>8,a&255,id>>8,id&255]).toString('base64')});}
    else {const code={stop:1,estop:2,frequency:3,start:4}[op];const arg=op==='frequency'?Math.round(cfg.pumpFrequencyHz*100):0;emit(k,50,{data:Buffer.from([1,code,id>>8,id&255,arg>>8,arg&255]).toString('base64')});}
  }
  s.notice=`${k}: ${op} sent; awaiting device result`;
}
// A device refuses an id it has already taken, which happens when another dashboard
// advanced the shared counter before this one saw the uplink. The refusal carries the
// id the device actually holds, so nextId() now catches up: resend once rather than
// wait out the deadline. Once only, so a genuinely bad command cannot loop.
function retryRefused(k,op) {
  const q=s.pending;
  if (!q||q.retried) return false;
  command(k,op);
  s.pending.retried=true;
  return true;
}
function advance(phase) {s.phase=phase;if (!phase.startsWith('wait_')) {s.pending=null;s.valveRetried=null;}s.phaseAt=now;}
// A start pressed while the sequence is busy is queued rather than refused, and fires
// here once the field valves are closed -- before close_main, so the main valve never
// has to close and reopen. ready() re-runs because the conditions may have moved.
function beginQueuedStart() {
  if (!s.pendingStart) return false;
  const problem=ready(s.pendingStart);
  if (problem) {s.notice='Queued start cancelled: '+problem;s.pendingStart=null;return false;}
  s.selected=s.pendingStart;s.pendingStart=null;s.step=0;advance('open_main');s.notice='Start checks passed';
  return true;
}
function stop(reason) {s.notice=reason;s.stopReason=reason;s.pendingStart=null;if (s.phase==='idle') s.selected=['valve1','valve2']; if (s.phase!=='stop_pump'&&s.phase!=='wait_pump_stop'&&s.phase!=='close_fields'&&s.phase!=='wait_field_close'&&s.phase!=='close_main'&&s.phase!=='wait_main_close') advance('stop_pump');}
function ready(list) {
  const w=device('water'), t=device('soil'), m=device('main'), p=device('pump');
  if (!configured) return 'Set IRRIGATION_APP_ID and all six DEV_EUI environment variables';
  if (!fresh('water',cfg.waterMaxAgeSec)||w.pressure_valid!==true||num(w.depth_m)===null) return 'Water level unavailable or stale';
  if (w.depth_m*100<cfg.startLevelCm) return 'Reservoir below start level';
  if (!fresh('soil',cfg.soilMaxAgeSec)||t.sensor_valid!==true||num(t.vwc_percent)===null) return 'Soil moisture unavailable or stale';
  if (t.vwc_percent>=cfg.startMoisturePercent) return 'Soil is not dry enough';
  if (!fresh('main',cfg.mainMaxAgeSec)||m.actuator_online!==true||m.overpressure===true||num(m.actuator_fault_code)!==0) return 'Main valve offline, stale or faulted';
  if (!pumpLive()||p.communication_ok!==true||p.configuration_valid!==true||num(p.vfd_fault_code)!==0||p.running===true) return 'Pump offline, running or faulted';
  if (!p.frequency_armed && !(cfg.pumpFrequencyHz>=cfg.pumpMinFrequencyHz&&cfg.pumpFrequencyHz<=cfg.pumpMaxFrequencyHz)) return 'Pump frequency invalid';
  for (const k of (list||s.selected)) if (!valveFresh(k)) return k+' status unavailable or stale';
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
  const d={communication_ok:!!(b[1]&1),configuration_valid:!!(b[1]&2),running:!!(b[1]&4),frequency_armed:!!(b[1]&8),lorawan_active:!!(b[1]&16),class_c_active:!!(b[1]&32),manual_mode:!!(b[1]&64),command_result:results[b[2]]||'unknown',last_command_id:u16(3)===65535?null:u16(3),commanded_frequency_hz:u16(5)/100,actual_frequency_hz:u16(7)/100,motor_current_a:u16(9)/100,vfd_fault_code:u16(11),output_voltage_v:u16(13)/10,run_state:states[b[15]]||'unknown',communication_error_code:b[16]};
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
  // s.selected is deliberately not touched until the start is honoured: this branch
  // can return, and a teardown in progress is walking s.selected right now.
  const sel=['valve1','valve2'].filter(k=>msg.payload[k]===true);
  // Remember the operator's choice on the server, so the checkboxes survive leaving
  // and returning to the dashboard page.
  s.selection={valve1:msg.payload.valve1===true,valve2:msg.payload.valve2===true};
  if (s.phase==='running') s.notice='Irrigation is already active';
  else if (!sel.length) s.notice='Select at least one field valve';
  else {const problem=ready(sel);if(problem) s.notice=problem;
    else if (s.phase==='idle') {s.selected=sel;s.step=0;s.pendingStart=null;advance('open_main');s.notice='Start checks passed';}
    else if (s.phase==='fault') {s.pendingStart=sel;s.selected=['valve1','valve2'];s.step=s.selected.length-1;s.notice='Start queued; retrying the failed close pass first';advance('close_fields');}
    else {s.pendingStart=sel;s.notice='Start queued; watering begins once the field valves are closed';}}
} else if (msg.payload&&msg.payload.kind==='select') {
  s.selection={valve1:msg.payload.valve1===true,valve2:msg.payload.valve2===true};
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
  return d.command_result==='accepted'&&(op==='stop'||op==='estop'?d.running===false:op==='start'?d.running===true:op==='frequency'?d.frequency_armed===true:false);};
const timed=(seconds)=>s.pending&&now-s.pending.at>seconds*1000;
// A field valve wait has no pending entry to age, so it ages from the phase it
// entered, which advance() set immediately after the command was emitted. Fails
// closed: a missing phaseAt reads as expired rather than as an endless wait.
const timedPhase=(seconds)=>!s.phaseAt||now-s.phaseAt>seconds*1000;
if (s.phase==='running') {
  if (now-s.runAt>cfg.maxWateringSec*1000) stop('Maximum watering duration reached');
  else if (!fresh('water',cfg.waterMaxAgeSec)||w.pressure_valid!==true||num(w.depth_m)===null||w.depth_m*100<=cfg.stopLevelCm) stop('Water level low or unavailable');
  else if (!fresh('soil',cfg.soilMaxAgeSec)||t.sensor_valid!==true||num(t.vwc_percent)===null||t.vwc_percent>=cfg.stopMoisturePercent) stop('Soil wet enough or reading unavailable');
  else if (!fresh('main',cfg.mainMaxAgeSec)||!mainOpen()||m.overpressure===true) stop('Main valve unsafe');
  else if (!pumpFresh()||p.communication_ok!==true||num(p.vfd_fault_code)!==0||p.running!==true) stop('Pump status unsafe');
  else if (s.selected.some(k=>!valveFresh(k)||device(k).pcv_last_commanded!=='open')) stop('Field valve status unsafe');
}
if (s.phase==='open_main') {if(mainOpen()) advance('open_fields');else {command('main','open');advance('wait_main_open');}}
if (s.phase==='wait_main_open') {if(done('main','open')) advance('open_fields');
  else if(m.command_phase==='rejected'&&retryRefused('main','open')) s.notice='Main valve refused the command; resending with a fresh id';
  else if(timed(cfg.mainCommandTimeoutSec)||m.command_phase==='failed'||m.command_phase==='rejected') stop('Main valve open failed');}
if (s.phase==='open_fields') {if(s.step>=s.selected.length) advance('set_frequency');else {const k=s.selected[s.step];if(valveReported(k,device(k),pcvState('open'))) {s.step++;}else {command(k,'open');advance('wait_field_open');}}}
if (s.phase==='wait_field_open') {const k=s.selected[s.step],d=device(k);
  if(valveReported(k,d,pcvState('open'))) {s.step++;advance('open_fields');}
  else if(valveFresh(k)&&d.at>=s.phaseAt&&valveIdRefused(d)&&retryValveOnce(k,'open')) s.notice=k+' refused the command id; resending with a fresh one';
  else if(valveFresh(k)&&d.at>=s.phaseAt&&valveRefused(d)) stop('Field valve open failed: '+k+' reported '+d.status_reason);
  else if(timedPhase(cfg.fieldValveCommandTimeoutSec)) stop('Field valve open unconfirmed: '+k);}
if (s.phase==='set_frequency') {const problem=ready();if(problem){stop('Start recheck: '+problem);}else if(p.frequency_armed===true&&Math.abs((num(p.commanded_frequency_hz)||0)-cfg.pumpFrequencyHz)<cfg.pumpFrequencyToleranceHz) advance('start_pump');else {command('pump','frequency');advance('wait_frequency');}}
if (s.phase==='wait_frequency') {if(done('pump','frequency')) advance('start_pump');else if(p.command_result==='invalid'&&retryRefused('pump','frequency')) s.notice='Pump refused the frequency command; resending with a fresh id';else if(timed(cfg.pumpCommandTimeoutSec)) stop('Pump frequency command unconfirmed');}
if (s.phase==='start_pump') {const problem=ready();if(problem) stop('Start recheck: '+problem);else if(!mainOpen()||s.selected.some(k=>device(k).pcv_last_commanded!=='open')) stop('Valve readiness lost');else {command('pump','start');advance('wait_pump_start');}}
if (s.phase==='wait_pump_start') {if(done('pump','start')) {advance('running');s.runAt=now;s.notice='Watering';}
  else if(p.command_result==='invalid'&&retryRefused('pump','start')) s.notice='Pump refused the start; resending with a fresh id';
  else if(p.command_result==='in_progress'&&num(p.last_command_id)===s.pending?.id)s.notice='Pump start accepted; waiting for final VFD condition';
  else if(timed(cfg.pumpCommandTimeoutSec)) stop('Pump start unconfirmed');}
if (s.phase==='stop_pump') {if(p.running===false&&pumpFresh()) {s.step=s.selected.length-1;advance('close_fields');}else {command('pump','stop');advance('wait_pump_stop');}}
if (s.phase==='wait_pump_stop') {if((done('pump','stop')||done('pump','estop'))&&pumpFresh()) {s.step=s.selected.length-1;advance('close_fields');}else if(p.command_result==='invalid'&&retryRefused('pump',s.pending.op)) s.notice='Pump refused the stop; resending with a fresh id';
else if(p.command_result==='in_progress'&&num(p.last_command_id)===s.pending?.id)s.notice='Pump stop accepted; waiting for final VFD condition';else if(timed(cfg.pumpStopTimeoutSec)){if(s.pending.op==='stop'){command('pump','estop');s.notice='Pump stop unconfirmed; emergency stop sent';}else {s.phase='fault';s.notice='Pump stop unconfirmed. Valves left open; inspect pump.';}}}
if (s.phase==='close_fields') {if(s.step<0) {if(!beginQueuedStart()) advance('close_main');}else {const k=s.selected[s.step];if(valveReported(k,device(k),pcvState('close'))) s.step--;else {command(k,'close');advance('wait_field_close');}}}
if (s.phase==='wait_field_close') {const k=s.selected[s.step],d=device(k);
  if(valveReported(k,d,pcvState('close'))) {s.step--;advance('close_fields');}
  else if(valveFresh(k)&&d.at>=s.phaseAt&&valveIdRefused(d)&&retryValveOnce(k,'close')) s.notice=k+' refused the command id; resending with a fresh one';
  else if(valveFresh(k)&&d.at>=s.phaseAt&&valveRefused(d)) {s.phase='fault';s.notice='Field valve close failed: '+k+' reported '+d.status_reason;}
  else if(timedPhase(cfg.fieldValveCommandTimeoutSec)) {s.phase='fault';s.notice='Field valve close unconfirmed: '+k;}}
if (s.phase==='close_main') {if(num(m.actual_angle_deg)!==null&&m.actual_angle_deg<=cfg.mainClosedMaxDeg&&m.valve_moving===false) {if(!beginQueuedStart()) advance('idle');}else {command('main','close');advance('wait_main_close');}}
if (s.phase==='wait_main_close') {if(done('main','close')) {if(!beginQueuedStart()) {advance('idle');s.notice='Watering stopped; valves closed';}}
  else if(m.command_phase==='rejected'&&retryRefused('main','close')) s.notice='Main valve refused the command; resending with a fresh id';
  else if(timed(cfg.mainCommandTimeoutSec)||m.command_phase==='rejected'){s.phase='fault';s.notice='Main valve close unconfirmed; inspect system';}}
s.configured=configured;s.settings=cfg;s.check=ready();s.devices=Object.fromEntries(keys.map(k=>[k,{...device(k),stale:k==='pump'?!pumpLive():isValve(k)?!valveFresh(k):!fresh(k,cfg[k+'MaxAgeSec'])}]));
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
// No context guard here. The inject that feeds this node fires once per deploy,
// which is the whole idempotence needed -- and a guard on stored context would
// actively break a redeploy, because the context survives it: the guard would
// match, return early, and leave the mqtt in node with no subscription.
node.status({fill:'green',shape:'dot',text:topic});
return {action:'subscribe',topic,qos:0};'''


# These ids are the live canvas's, not the builder's own. Node-RED's importer
# preserves incoming ids and raises an import conflict when one is already
# taken, so a matching id is reused or explicitly replaced, while an id matching
# nothing is added as a *second* node. This flow is maintained by re-exporting
# it from Node-RED, so the builder reproduces that export: a re-import updates
# the running dashboard in place instead of doubling every node. Re-export
# rather than hand-editing, and refresh these ids whenever the canvas changes.
ID_CANVAS = "dc658b3ab3f2917e"          # canvas group
ID_TICK = "fd327e43632c19d6"
ID_SETTINGS = "0da71259acd21508"
ID_SUBSCRIBE_TICK = "9b727c5b0531676e"
ID_SUBSCRIBE = "405c9e29995e8efe"
ID_MQTT_IN = "ed30a564d5bb461a"
ID_STATUS = "50c3e5351eac102c"
ID_CONTROL = "1406698c9cd792c5"
ID_MQTT_OUT = "03424753ad790e6b"
ID_UI = "d490a04f3084b9da"
ID_GLOBAL_CONFIG = "c7c03261e21b3272"

# z only. No tab node ships, so the import dialog's "Import to" list decides
# which flow the nodes join and no second tab is ever created.
FLOW_ID = "irrigation_single_page"
# The workspace's shared ChirpStack broker, as MainValve uses. No mqtt-broker
# node ships: one would arrive as a duplicate server to delete by hand, and
# re-declaring the shared id can overwrite the host, port and TLS settings.
BROKER = "ae0178f3742ff530"
PAGE = "irrigation_page"
GROUP = "irrigation_group"
UI_BASE = "f53e93e9ba219e63"   # shared "My Dashboard"
UI_THEME = "e49416861823a329"  # shared "MyTheme"

CANVAS_STYLE = {"stroke": "#999999", "stroke-opacity": "1", "fill": "none",
                "fill-opacity": "1", "label": True, "label-position": "nw",
                "color": "#767676"}
BREAKPOINTS = [{"name": "Default", "px": "0", "cols": "3"},
               {"name": "Tablet", "px": "576", "cols": "6"},
               {"name": "Small Desktop", "px": "768", "cols": "9"},
               {"name": "Desktop", "px": "1024", "cols": "12"}]


def build():
    canvas_nodes = [ID_TICK, ID_SETTINGS, ID_SUBSCRIBE_TICK, ID_SUBSCRIBE,
                    ID_MQTT_IN, ID_STATUS, ID_CONTROL, ID_MQTT_OUT, ID_UI]
    nodes = [
        node(ID_CANVAS, "group", z=FLOW_ID, style=CANVAS_STYLE, nodes=canvas_nodes,
             x=74, y=39, w=1502, h=322),
        node(ID_TICK, "inject", z=FLOW_ID, g=ID_CANVAS, name="Refresh safety and settings", props=[{"p":"payload"}], repeat="5", crontab="", once=True, onceDelay="0.5", topic="", payload="", payloadType="date", x=720, y=80, wires=[[ID_SETTINGS]]),
        node(ID_SETTINGS, "function", z=FLOW_ID, g=ID_CANVAS, name="Irrigation settings — edit limits here", func=SETTINGS, outputs=1, timeout=0, noerr=0, initialize="", finalize="", libs=[], x=860, y=140, wires=[[ID_CONTROL]]),
        node(ID_SUBSCRIBE_TICK, "inject", z=FLOW_ID, g=ID_CANVAS, name="Refresh ChirpStack subscription", props=[{"p":"payload"}], repeat="", crontab="", once=True, onceDelay="0.5", topic="", payload="", payloadType="date", x=250, y=100, wires=[[ID_SUBSCRIBE]]),
        node(ID_SUBSCRIBE, "function", z=FLOW_ID, g=ID_CANVAS, name="Subscribe to configured ChirpStack application", func=SUBSCRIBE, outputs=1, timeout=0, noerr=0, initialize="", finalize="", libs=[], x=400, y=160, wires=[[ID_MQTT_IN]]),
        node(ID_MQTT_IN, "mqtt in", z=FLOW_ID, g=ID_CANVAS, name="All six ChirpStack uplinks", topic="", qos="0", datatype="auto-detect", broker=BROKER, nl=False, rap=True, rh=0, inputs=1, x=730, y=220, wires=[[ID_CONTROL]]),
        node(ID_STATUS, "status", z=FLOW_ID, g=ID_CANVAS, name="MQTT connection", scope=[ID_MQTT_IN, ID_MQTT_OUT], x=710, y=300, wires=[[ID_CONTROL]]),
        node(ID_CONTROL, "function", z=FLOW_ID, g=ID_CANVAS, name="Safety checks and irrigation sequence", func=CONTROL, outputs=2, timeout=0, noerr=0, initialize="", finalize="", libs=[], x=1090, y=280, wires=[[ID_MQTT_OUT],[ID_UI]]),
        node(ID_MQTT_OUT, "mqtt out", z=FLOW_ID, g=ID_CANVAS, name="ChirpStack downlinks", topic="", qos="", retain="", respTopic="", contentType="", userProps="", correl="", expiry="", broker=BROKER, x=1410, y=240, wires=[]),
        node(ID_UI, "ui-template", z=FLOW_ID, g=ID_CANVAS, group=GROUP, page="", ui="", name="Compact irrigation dashboard", order=1, width="12", height="17", head="", format=TEMPLATE, storeOutMessages=True, passthru=False, resendOnRefresh=True, templateScope="local", className="", x=1420, y=320, wires=[[ID_CONTROL]]),
        node(GROUP, "ui-group", z=FLOW_ID, name="Irrigation", page=PAGE, width="12", height="1", order=1, showTitle=False, className="", visible="true", disabled="false", groupType="default"),
        node(PAGE, "ui-page", z=FLOW_ID, name="Irrigation", ui=UI_BASE, path="/irrigation", icon="water", layout="grid", theme=UI_THEME, breakpoints=BREAKPOINTS, order=4, className="", visible=True, disabled=False),
        # Shared singletons, shipped so a fresh canvas gets them. Their ids match
        # what the workspace owns, so the import reuses or replaces that node
        # rather than adding another base or theme.
        node(UI_BASE, "ui-base", name="My Dashboard", path="/dashboard", appIcon="", includeClientData=True, acceptsClientConfig=["ui-notification", "ui-control"], showPathInSidebar=False, headerContent="page", navigationStyle="fixed", titleBarStyle="default", showReconnectNotification=True, notificationDisplayTime=1, showDisconnectNotification=True, allowInstall=True),
        node(UI_THEME, "ui-theme", name="MyTheme", colors={"surface": "#ffffff", "primary": "#0094ce", "bgPage": "#eeeeee", "groupBg": "#ffffff", "groupOutline": "#cccccc"}, sizes={"density": "default", "pagePadding": "12px", "groupGap": "12px", "groupBorderRadius": "4px", "widgetGap": "12px"}),
        node(ID_GLOBAL_CONFIG, "global-config", env=[], modules={"@flowfuse/node-red-dashboard": "1.30.2"}),
    ]
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(json.dumps(nodes, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(OUTPUT)


if __name__ == "__main__":
    build()
