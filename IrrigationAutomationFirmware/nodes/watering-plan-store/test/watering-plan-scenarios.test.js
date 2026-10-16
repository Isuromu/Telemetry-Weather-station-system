'use strict';

// Replays docs/integrations/watering-plan/conformance-scenarios.json against the
// JavaScript store and scheduler — the twin of tools/fake_watering_plan_backend.py,
// which runs the same vectors against the Python reference. A backend that passes
// one set and fails the other has drifted; API_PROTOCOL_SPEC.md requires a new
// backend to pass these vectors, so they are the shared contract, not a scratch
// test. Virtual time changes only WHEN ticks happen, never what a rule does.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const test = require('node:test');
const { WateringPlanStore } = require('../watering-plan-store');
const { WateringPlanScheduler } = require('../watering-plan-scheduler');

const docs = path.resolve(__dirname, '../../../docs/integrations/watering-plan');
const migrationsDir = path.join(docs, 'sqlite');
const scenarioFile = JSON.parse(fs.readFileSync(path.join(docs, 'conformance-scenarios.json'), 'utf8'));

const TERMINAL = ['completed', 'failed', 'expired', 'cancelled', 'recovery_required'];
const BASE_MS = Date.parse('2026-01-01T00:00:00Z');

function iso(ms) { return new Date(ms).toISOString().replace(/\.\d{3}Z$/, 'Z'); }

class VirtualClock {
  constructor() { this.wallMs = BASE_MS; this.monoMs = 1000; }
  now() { return iso(this.wallMs); }
  advance(seconds) { this.wallMs += seconds * 1000; this.monoMs += seconds * 1000; }
  regressMonotonic(ms) { this.monoMs -= ms; }
}

// Stand-in for the gateway sequencer: it reports the dashboard phases the
// scheduler reconciles against and a growing valve1 total so a volume plan has
// real meter evidence.
class SimulatedDashboard {
  constructor(clock) {
    this.clock = clock; this.running = false; this.runningSinceMs = null; this.stopRequestedAtMs = null;
    this.faultPhase = null; this.meterUnavailable = false; this.meterTotalOverride = null;
    this.calls = []; this.safeStops = 0; this.flowM3PerSecond = 0.001; this.teardownSeconds = 0;
  }
  elapsedMs() { return this.runningSinceMs === null ? 0 : Math.max(0, this.clock.monoMs - this.runningSinceMs); }
  start() { this.calls.push('start'); this.running = true; this.runningSinceMs = this.clock.monoMs; this.stopRequestedAtMs = null; }
  stop() { this.calls.push('stop'); this.stopRequestedAtMs = this.clock.monoMs; }
  safeStop() { this.calls.push('safe_stop'); this.safeStops += 1; this.running = false; this.stopRequestedAtMs = null; }
  phase() {
    if (this.faultPhase) return this.faultPhase;
    if (!this.running) return 'stopped';
    if (this.stopRequestedAtMs !== null) {
      if (this.clock.monoMs - this.stopRequestedAtMs >= this.teardownSeconds * 1000) { this.running = false; return 'stopped'; }
      return 'stopping';
    }
    return 'running';
  }
  meterStatus() {
    const total = this.meterTotalOverride !== null ? this.meterTotalOverride
      : 10 + this.flowM3PerSecond * (this.elapsedMs() / 1000);
    return { healthy: !this.meterUnavailable, fresh: !this.meterUnavailable, decoder: 'commissioned', positive_total_m3: total, sampled_ms: this.clock.wallMs };
  }
}

function factsFor({ clock, dashboard, gate, bootId, enabled, clockSynchronized }) {
  // CONTROL reports 'idle' at rest; the simulated dashboard says 'stopped'.
  const raw = gate ? 'running' : dashboard.phase();
  const phase = raw === 'stopped' ? 'idle' : raw;
  return {
    enabled: enabled !== false, clock_synchronized: clockSynchronized !== false, boot_id: bootId || 'boot-1', now_ms: clock.wallMs,
    state: {
      present: true, age_ms: 0, phase, notice: null, stop_reason: null, plan_operation: null,
      ready_sets: { 'valve1': gate || '', 'valve1,valve2': gate || '', 'valve2': '' }
    },
    meter_status: { valve1: dashboard.meterStatus() },
    config: { max_state_age_sec: 15, delay_retry_sec: 60, meter_max_age_sec: 60 }
  };
}

function planFor(scenario, clock) {
  const base = Object.assign({}, scenarioFile.plan_defaults, scenario.plan || {});
  delete base.valid_offset;
  const offset = (scenario.plan && scenario.plan.valid_offset) || 600;
  return Object.assign(base, {
    plan_id: 'plan-1',
    created_at: clock.now(),
    start_at: clock.now(),
    valid_until: iso(clock.wallMs + offset * 1000)
  });
}

async function runScenario(scenario) {
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'watering-plan-scenario-'));
  const clock = new VirtualClock();
  const dashboard = new SimulatedDashboard(clock);
  const store = new WateringPlanStore({ databasePath: path.join(directory, 'plans.sqlite3'), migrationsDir });
  let gate = null;
  let bootId = 'boot-1';
  let scheduler = new WateringPlanScheduler({ store, clock: () => clock.now(), monotonicMs: () => clock.monoMs, ownerId: 'scheduler' });
  const planId = 'plan-1';

  // A dispatch is applied by the sequencer and observed immediately, the way the
  // reference sequencer reports the phase in its start/stop response. The real
  // gateway cannot: CONTROL walks open_main → … → running over the next ticks,
  // so on the Pi a start is observed one tick later, not in the claiming tick.
  const tick = async () => {
    const result = await scheduler.tick(factsFor({ clock, dashboard, gate, bootId }));
    if (result.dispatch && result.dispatch.kind === 'start') dashboard.start();
    if (result.dispatch && result.dispatch.kind === 'stop') dashboard.stop();
    if (result.action === 'restart_safe_stop') dashboard.safeStop();
    if (result.dispatch) return scheduler.tick(factsFor({ clock, dashboard, gate, bootId }));
    return result;
  };
  const state = async () => (await store.get(planId)).state;

  try {
    await store.initialize();
    await store.accept({ plan: planFor(scenario, clock), resolvedValveIds: ['valve1'], acceptedMaxRuntimeSec: Math.min(planFor(scenario, clock).hard_max_runtime_sec, 14400) });
    for (const action of scenario.actions) {
      if (action.advance !== undefined) {
        for (let step = 0; step < action.advance; step += 1) { clock.advance(1); await tick(); }
      } else if (action.assert_state) {
        assert.equal(await state(), action.assert_state, scenario.name + ': state after ticks');
      } else if (action.assert_delay_reason) {
        assert.equal((await store.get(planId)).last_delay_reason, action.assert_delay_reason, scenario.name + ': delay reason');
      } else if (action.run_until_terminal) {
        for (let step = 0; step < 900; step += 1) { clock.advance(1); await tick(); if (TERMINAL.includes(await state())) break; }
      } else if (action.cancel) {
        await store.cancel(planId, clock.now());
      } else if (action.gate) {
        gate = action.gate;
      } else if (action.clear_gate) {
        gate = null;
      } else if (action.inject && action.inject.meter_unavailable) {
        dashboard.meterUnavailable = true;
      } else if (action.regress_monotonic) {
        clock.regressMonotonic(action.regress_monotonic);
      } else if (action.restart) {
        // A process restart: a fresh scheduler with a new boot id, same store.
        bootId = 'boot-2';
        scheduler = new WateringPlanScheduler({ store, clock: () => clock.now(), monotonicMs: () => clock.monoMs, ownerId: 'scheduler' });
        await tick();
      } else {
        throw new Error(scenario.name + ': unsupported action ' + JSON.stringify(action));
      }
    }

    const plan = await store.get(planId, true);
    const attempt = plan.attempts[0] || {};
    assert.equal(plan.state, scenario.expect.final_state, scenario.name + ': final state');
    if (scenario.expect.execution_stop_reason) assert.equal(attempt.final_reason, scenario.expect.execution_stop_reason, scenario.name + ': stop reason');
    if (scenario.expect.actual_runtime_ms !== undefined) assert.equal(attempt.actual_runtime_ms, scenario.expect.actual_runtime_ms, scenario.name + ': runtime');
    if (scenario.expect.delivered_m3 !== undefined) assert.ok(Math.abs((attempt.delivered_m3 || 0) - scenario.expect.delivered_m3) < 1e-9, scenario.name + ': delivered volume');
    if (scenario.expect.safe_stops_min !== undefined) assert.ok(dashboard.safeStops >= scenario.expect.safe_stops_min, scenario.name + ': safe stops');
    return { scenario: scenario.name, state: plan.state, calls: dashboard.calls.slice() };
  } finally { await store.close(); fs.rmSync(directory, { recursive: true, force: true }); }
}

for (const scenario of scenarioFile.scenarios) {
  test('conformance: ' + scenario.name, async () => {
    await runScenario(scenario);
  });
}

test('the vector set is not silently empty', () => {
  assert.ok(scenarioFile.scenarios.length >= 9, 'expected the shared vectors to be present');
});
