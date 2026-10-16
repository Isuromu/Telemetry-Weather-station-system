'use strict';

// Policy tests, ported from tools/test_watering_plan_scheduler.py and extended
// with the gateway-specific gates (stale snapshot, operator run, restart
// evidence). Everything here is offline: fake clock, fake monotonic, fake meter.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const test = require('node:test');
const { WateringPlanStore } = require('../watering-plan-store');
const { WateringPlanScheduler, FAILED_STOP_REASONS } = require('../watering-plan-scheduler');

const migrationsDir = path.resolve(__dirname, '../../../docs/integrations/watering-plan/sqlite');
const rules = JSON.parse(fs.readFileSync(path.resolve(__dirname, '../../../docs/integrations/watering-plan/api-protocol.rules.json'), 'utf8'));

const CLOCK = '2026-01-01T01:00:00Z';

function plan(overrides) {
  return Object.assign({
    schema_version: 1, plan_id: 'plan-1', revision: 1, status: 'scheduled', source: 'human',
    timezone: 'UTC', created_at: '2026-01-01T00:00:00Z', start_at: '2026-01-01T01:00:00Z', valid_until: '2026-01-01T02:00:00Z',
    targets: { mode: 'selected', field_ids: ['field-001'] }, run_mode: 'parallel',
    stop: { type: 'duration', value: 30, unit: 'seconds' }, hard_max_runtime_sec: 900
  }, overrides || {});
}

function facts(overrides) {
  return Object.assign({
    enabled: true, clock_synchronized: true, boot_id: 'boot-1', now_ms: Date.parse(CLOCK),
    state: {
      present: true, age_ms: 0, phase: 'idle', notice: null, stop_reason: null, plan_operation: null,
      ready_sets: { 'valve1': '', 'valve2': '', 'valve1,valve2': '' }, restart_evidence: {}
    },
    meter_status: { valve1: { healthy: true, fresh: true, decoder: 'commissioned', positive_total_m3: 10, sampled_ms: Date.parse(CLOCK) } },
    config: { max_state_age_sec: 15, delay_retry_sec: 60, meter_max_age_sec: 60 }
  }, overrides || {});
}

async function withStore(run, stopOverride) {
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'watering-plan-scheduler-'));
  const store = new WateringPlanStore({ databasePath: path.join(directory, 'plans.sqlite3'), migrationsDir });
  const monotonic = { value: 1000 };
  const clock = { value: CLOCK };
  const scheduler = new WateringPlanScheduler({ store, clock: () => clock.value, monotonicMs: () => monotonic.value, ownerId: 'scheduler' });
  try {
    await store.initialize();
    await store.accept({ plan: stopOverride || plan(), resolvedValveIds: ['valve1'], acceptedMaxRuntimeSec: 600 });
    return await run({ store, scheduler, monotonic, clock });
  } finally { await store.close(); fs.rmSync(directory, { recursive: true, force: true }); }
}

const runningFacts = extra => facts(Object.assign({
  state: { present: true, age_ms: 0, phase: 'running', notice: null, stop_reason: null, plan_operation: null, ready_sets: { 'valve1': '' }, restart_evidence: {} }
}, extra || {}));

test('a disabled tick writes nothing at all', async () => {
  await withStore(async ({ store, scheduler }) => {
    const result = await scheduler.tick(facts({ enabled: false }));
    assert.equal(result.action, 'disabled');
    assert.equal((await store.get('plan-1', true)).events.length, 1);
    assert.equal(await store.activePlan(), null);
  });
});

test('an untrusted clock neither expires nor claims', async () => {
  await withStore(async ({ store, scheduler }) => {
    const result = await scheduler.tick(facts({ clock_synchronized: false, now_ms: Date.parse('2026-01-01T03:00:00Z') }));
    assert.equal(result.action, 'clock_unsynchronized');
    assert.equal((await store.get('plan-1')).state, 'scheduled');
  });
});

test('a due plan claims and dispatches a start for its valves', async () => {
  await withStore(async ({ store, scheduler }) => {
    const result = await scheduler.tick(facts());
    assert.equal(result.action, 'started');
    assert.equal(result.dispatch.kind, 'start');
    assert.equal(result.dispatch.valve1, true);
    assert.equal(result.dispatch.valve2, false);
    const claimed = await store.activePlan();
    assert.equal(claimed.state, 'starting');
    assert.equal(result.dispatch.operation_id, claimed.attempt.operation_id);
  });
});

test('a stale snapshot delays the plan without creating an attempt row', async () => {
  await withStore(async ({ store, scheduler }) => {
    const result = await scheduler.tick(facts({ state: { present: true, age_ms: 60000, phase: 'idle', ready_sets: { 'valve1': '' }, restart_evidence: {} } }));
    assert.equal(result.action, 'delayed');
    assert.equal(result.reason, 'executor_gate_failed');
    const planRow = await store.get('plan-1', true);
    assert.equal(planRow.state, 'delayed');
    assert.equal(planRow.last_delay_reason, 'executor_gate_failed');
    assert.equal(planRow.attempts.length, 0, 'a blocked plan must not write an attempt per tick');
  });
});

test('a valve that is not ready delays the plan', async () => {
  await withStore(async ({ store, scheduler }) => {
    const result = await scheduler.tick(facts({ state: { present: true, age_ms: 0, phase: 'idle', ready_sets: { 'valve1': 'Field valve 1 is stale' } } }));
    assert.equal(result.reason, 'executor_gate_failed');
    assert.equal((await store.get('plan-1')).state, 'delayed');
  });
});

test('an operator run delays a due plan', async () => {
  await withStore(async ({ scheduler }) => {
    const result = await scheduler.tick(facts({ state: { present: true, age_ms: 0, phase: 'running', ready_sets: { 'valve1': '' } } }));
    assert.equal(result.reason, 'pump_run_active');
  });
});

test('a running start anchors tracking, then stops on the duration target', async () => {
  await withStore(async ({ store, scheduler, monotonic }) => {
    const started = await scheduler.tick(facts());
    const attemptId = (await store.activePlan()).attempt.attempt_id;
    const running = await scheduler.tick(runningFacts({ plan_operation: started.dispatch.operation_id }));
    assert.equal(running.action, 'running');
    const attempt = (await store.get('plan-1', true)).attempts[0];
    assert.equal(attempt.monotonic_start_ms, 1000, 'the anchor is the confirmed running moment');
    assert.equal(attempt.state, 'running');
    assert.equal(attempt.final_reason, null, 'nothing may consume final_reason before the run ends');

    monotonic.value = 31000;
    const stopping = await scheduler.tick(runningFacts({ plan_operation: started.dispatch.operation_id }));
    assert.equal(stopping.action, 'duration_target_reached_stop_dispatched');
    assert.equal(stopping.dispatch.kind, 'stop');
    assert.equal(stopping.dispatch.operation_id, started.dispatch.operation_id + ':stop');
    assert.equal((await store.get('plan-1')).state, 'stopping');

    const completed = await scheduler.tick(facts());
    assert.equal(completed.action, 'completed');
    const finalAttempt = (await store.get('plan-1', true)).attempts[0];
    assert.equal(finalAttempt.state, 'completed');
    assert.equal(finalAttempt.actual_runtime_ms, 30000);
    assert.equal(finalAttempt.final_reason, 'duration_target_reached');
    assert.equal(attemptId, finalAttempt.attempt_id);
  });
});

test('an active cancel dispatches the ordered stop then completes as cancelled', async () => {
  await withStore(async ({ store, scheduler }) => {
    const started = await scheduler.tick(facts());
    await scheduler.tick(runningFacts({ plan_operation: started.dispatch.operation_id }));
    await store.cancel('plan-1');
    const stopping = await scheduler.tick(runningFacts());
    assert.equal(stopping.reason, 'cancel_stop_dispatched');
    const completed = await scheduler.tick(facts());
    assert.equal(completed.action, 'completed');
    assert.equal(completed.reason, 'cancelled');
    assert.equal((await store.get('plan-1', true)).attempts[0].final_reason, 'cancel_stop_dispatched');
  });
});

test('a volume run captures its baseline before dispatch and stops on the meter delta', async () => {
  const volumePlan = plan({ plan_id: 'plan-1', stop: { type: 'volume', value: 0.04, unit: 'm3' } });
  const meterAt = total => ({ valve1: { healthy: true, fresh: true, decoder: 'commissioned', positive_total_m3: total, sampled_ms: Date.parse(CLOCK) } });
  await withStore(async ({ store, scheduler }) => {
    const started = await scheduler.tick(facts());
    assert.equal(started.action, 'started');
    assert.equal((await store.get('plan-1', true)).attempts[0].meter_baseline_m3, 10, 'the baseline is captured before dispatch');
    await scheduler.tick(facts({ state: { present: true, age_ms: 0, phase: 'running', plan_operation: started.dispatch.operation_id } }));

    const partial = await scheduler.tick(facts({ meter_status: meterAt(10.01), state: { present: true, age_ms: 0, phase: 'running', plan_operation: started.dispatch.operation_id } }));
    assert.equal(partial.action, 'observed', 'a delta below the target keeps running');

    const reached = await scheduler.tick(facts({ meter_status: meterAt(10.04), state: { present: true, age_ms: 0, phase: 'running', plan_operation: started.dispatch.operation_id } }));
    assert.equal(reached.action, 'volume_target_reached_stop_dispatched');
    assert.equal(reached.dispatch.kind, 'stop');

    const completed = await scheduler.tick(facts());
    assert.equal(completed.action, 'completed');
    const attempt = (await store.get('plan-1', true)).attempts[0];
    assert.equal(attempt.meter_baseline_m3, 10);
    assert.ok(Math.abs(attempt.delivered_m3 - 0.04) < 1e-9);
  }, volumePlan);
});

test('a lost meter stops in order and then fails', async () => {
  const volumePlan = plan({ plan_id: 'plan-1', stop: { type: 'volume', value: 0.04, unit: 'm3' } });
  await withStore(async ({ store, scheduler }) => {
    const started = await scheduler.tick(facts());
    await scheduler.tick(runningFacts({ plan_operation: started.dispatch.operation_id }));
    const lost = facts({ meter_status: { valve1: { healthy: false, fresh: true, decoder: 'commissioned', positive_total_m3: 10, sampled_ms: Date.parse(CLOCK) } } });
    const stopping = await scheduler.tick(Object.assign({}, runningFacts(), { meter_status: lost.meter_status }));
    assert.equal(stopping.reason, 'meter_unavailable');
    const failed = await scheduler.tick(facts());
    assert.equal(failed.action, 'failed');
    assert.equal((await store.get('plan-1', true)).attempts[0].final_reason, 'meter_unavailable');
  }, volumePlan);
});

test('a volume plan with only static meter JSON delays instead of running', async () => {
  const volumePlan = plan({ plan_id: 'plan-1', stop: { type: 'volume', value: 0.04, unit: 'm3' } });
  await withStore(async ({ store, scheduler }) => {
    const stale = facts({ meter_status: { valve1: { healthy: true, fresh: true, decoder: 'commissioned', positive_total_m3: 10, sampled_ms: Date.parse(CLOCK) - 3600000 } } });
    const result = await scheduler.tick(stale);
    assert.equal(result.action, 'delayed');
    assert.equal(result.reason, 'meter_unavailable');
    assert.equal((await store.get('plan-1', true)).attempts.length, 0);
  }, volumePlan);
});

test('a plan past valid_until expires instead of running', async () => {
  await withStore(async ({ store, scheduler, clock }) => {
    clock.value = '2026-01-01T03:00:00Z';
    const result = await scheduler.tick(facts({ now_ms: Date.parse(clock.value) }));
    assert.equal(result.action, 'idle');
    assert.equal((await store.get('plan-1')).state, 'expired');
  });
});

test('a restart commands nothing while the snapshot is stale', async () => {
  await withStore(async ({ store, scheduler }) => {
    await store.claim(CLOCK, 'before-restart');
    const result = await scheduler.tick(facts({ boot_id: 'boot-2', state: { present: true, age_ms: 60000, phase: 'running' } }));
    assert.equal(result.dispatch, undefined, 'an unobserved field must not be commanded');
    assert.equal((await store.get('plan-1')).state, 'recovery_required');
  });
});

test('a restart sends one ordered stop when a fresh sequencer is still busy', async () => {
  await withStore(async ({ store, scheduler }) => {
    await store.claim(CLOCK, 'before-restart');
    const result = await scheduler.tick(facts({ boot_id: 'boot-2', state: { present: true, age_ms: 0, phase: 'running' } }));
    assert.equal(result.action, 'restart_safe_stop');
    assert.equal(result.dispatch.kind, 'stop');
    assert.equal(result.dispatch.operation_id.startsWith('recovery:'), true);
    assert.equal((await store.get('plan-1')).state, 'recovery_required');
  });
});

test('a restart sends no stop when the sequencer was already at rest', async () => {
  await withStore(async ({ store, scheduler }) => {
    await store.claim(CLOCK, 'before-restart');
    const result = await scheduler.tick(facts({ boot_id: 'boot-2', state: { present: true, age_ms: 0, phase: 'idle' } }));
    assert.equal(result.dispatch, undefined);
    assert.equal(result.action, 'idle');
    assert.equal((await store.get('plan-1')).state, 'recovery_required');
  });
});

test('a start the sequencer aborted before running fails, and an unobserved start stays uncertain', async () => {
  await withStore(async ({ store, scheduler }) => {
    await scheduler.tick(facts());
    const aborted = facts({ state: { present: true, age_ms: 0, phase: 'idle', stop_reason: 'Water level too low', ready_sets: { 'valve1': '' } } });
    const failed = await scheduler.tick(aborted);
    assert.equal(failed.action, 'failed');
    assert.equal(failed.reason, 'sequencer_fault');
  });
  await withStore(async ({ store, scheduler }) => {
    await scheduler.tick(facts());
    const silent = await scheduler.tick(facts({ state: { present: true, age_ms: 0, phase: 'idle', stop_reason: null, ready_sets: { 'valve1': '' } } }));
    assert.equal(silent.action, 'recovery_required');
    assert.equal((await store.get('plan-1')).state, 'recovery_required');
  });
});

test('a fault phase fails the run', async () => {
  await withStore(async ({ store, scheduler }) => {
    await scheduler.tick(facts());
    const result = await scheduler.tick(facts({ state: { present: true, age_ms: 0, phase: 'fault', notice: 'Field valve close failed', ready_sets: { 'valve1': '' } } }));
    assert.equal(result.action, 'failed');
    assert.equal((await store.get('plan-1')).state, 'failed');
  });
});

test('every reason the scheduler can emit exists in the protocol vocabulary', () => {
  const delayReasons = ['clock_unsynchronized', 'executor_gate_failed', 'pump_run_active', 'meter_unavailable', 'unsupported_volume_target', 'unsupported_duration_unit', 'unsupported_volume_unit'];
  const stopReasons = ['duration_target_reached', 'volume_target_reached', 'cancel_stop_dispatched'];
  for (const reason of delayReasons) assert.ok(rules.delay_reasons.includes(reason), 'unknown delay reason: ' + reason);
  for (const reason of stopReasons) assert.ok(rules.stop_reasons.includes(reason), 'unknown stop reason: ' + reason);
  for (const reason of FAILED_STOP_REASONS) assert.ok(rules.failure_reasons.includes(reason), 'unknown failure reason: ' + reason);
  assert.ok(rules.failure_reasons.includes('sequencer_fault'));
  assert.ok(rules.failure_reasons.includes('start_outcome_uncertain'));
});
