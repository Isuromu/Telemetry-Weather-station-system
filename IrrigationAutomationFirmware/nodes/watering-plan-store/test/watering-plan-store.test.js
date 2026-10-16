'use strict';

const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const test = require('node:test');
const { WateringPlanStore, canonicalDigest } = require('../watering-plan-store');

const migrationsDir = path.resolve(__dirname, '../../../docs/integrations/watering-plan/sqlite');
function plan(revision, fields) {
  return {
    schema_version: 1, plan_id: 'plan-1', revision, status: 'scheduled', source: 'human',
    timezone: 'UTC', created_at: '2026-01-01T00:00:00Z', start_at: '2026-01-01T01:00:00Z', valid_until: '2026-01-01T02:00:00Z',
    targets: { mode: 'selected', field_ids: fields || ['field-b', 'field-a'] }, run_mode: 'parallel',
    stop: { type: 'duration', value: 10, unit: 'min' }, hard_max_runtime_sec: 900
  };
}

test('canonical digest, migrations, acceptance revisions, get/list/cancel', async () => {
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'watering-plan-store-'));
  const store = new WateringPlanStore({ databasePath: path.join(directory, 'plans.sqlite3'), migrationsDir });
  try {
    await store.initialize();
    assert.equal(canonicalDigest(plan(1, ['field-b', 'field-a'])).payloadSha256, canonicalDigest(plan(1, ['field-a', 'field-b'])).payloadSha256);
    const created = await store.accept({ plan: plan(1), resolvedValveIds: ['valve2', 'valve1'], acceptedMaxRuntimeSec: 600 });
    assert.equal(created.created, true); assert.deepEqual(created.resolved_valves, ['valve1', 'valve2']);
    const duplicate = await store.accept({ plan: plan(1, ['field-a', 'field-b']), resolvedValveIds: ['valve1', 'valve2'], acceptedMaxRuntimeSec: 600 });
    assert.equal(duplicate.idempotent, true);
    await assert.rejects(() => store.accept({ plan: Object.assign(plan(1), { summary: 'different' }), resolvedValveIds: ['valve1'], acceptedMaxRuntimeSec: 600 }), error => error.code === 'revision_conflict');
    const listed = await store.list({ limit: 1 }); assert.equal(listed.items.length, 1);
    const cancelled = await store.cancel('plan-1'); assert.equal(cancelled.state, 'cancelled');
    const fetched = await store.get('plan-1', true); assert.equal(fetched.events.length, 2);
  } finally { await store.close(); fs.rmSync(directory, { recursive: true, force: true }); }
});

test('a sqlite error event is reported instead of crashing the runtime', async () => {
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'watering-plan-store-'));
  const reported = [];
  const store = new WateringPlanStore({ databasePath: path.join(directory, 'plans.sqlite3'), migrationsDir, onError: error => reported.push(error.message) });
  try {
    await store.initialize();
    const db = await store.open();
    // sqlite3 raises this out of band, with no callback to reject, when a WAL
    // write fails (a full disk). Unhandled it terminates the Node-RED process.
    db.emit('error', new Error('SQLITE_IOERR: disk I/O error'));
    assert.deepEqual(reported, ['SQLITE_IOERR: disk I/O error']);
    assert.equal((await store.list({ limit: 1 })).items.length, 0);
  } finally { await store.close(); fs.rmSync(directory, { recursive: true, force: true }); }
});

test('lifecycle tracking: expiry, delay, attempt transitions, write-once anchors', async () => {
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'watering-plan-store-'));
  const store = new WateringPlanStore({ databasePath: path.join(directory, 'plans.sqlite3'), migrationsDir });
  try {
    await store.initialize();
    await store.accept({ plan: plan(1), resolvedValveIds: ['valve1'], acceptedMaxRuntimeSec: 600 });

    // A due-but-blocked plan is delayed without ever creating an attempt row.
    const delayed = await store.markDelayed('plan-1', 'pump_run_active', '2026-01-01T01:00:05Z', 60);
    assert.equal(delayed.state, 'delayed');
    assert.equal(delayed.last_delay_reason, 'pump_run_active');
    assert.equal((await store.list({ limit: 5 })).items.length, 1);
    assert.equal((await store.activePlan()), null);
    const eventsAfterDelay = (await store.get('plan-1', true)).events.length;
    // Same reason inside the retry window must not append another event.
    await store.markDelayed('plan-1', 'pump_run_active', '2026-01-01T01:00:20Z', 60);
    assert.equal((await store.get('plan-1', true)).events.length, eventsAfterDelay);
    await store.markDelayed('plan-1', 'executor_gate_failed', '2026-01-01T01:00:20Z', 60);
    const changed = await store.get('plan-1', true);
    assert.equal(changed.events.length, eventsAfterDelay + 1);
    assert.equal(changed.last_delay_reason, 'executor_gate_failed');

    // Peek is read-only, claim is the single writer.
    assert.equal((await store.nextDue('2026-01-01T01:00:00Z')).plan_id, 'plan-1');
    const claimed = await store.claim('2026-01-01T01:00:00Z', 'scheduler');
    assert.equal(claimed.state, 'starting');
    assert.equal(claimed.attempt.state, 'starting');
    const attemptId = claimed.attempt.attempt_id;

    // Tracking anchors are write-once so a retry cannot move the start line.
    await store.startAttemptTracking(attemptId, 1000, '2026-01-01T01:00:01Z');
    await store.startAttemptTracking(attemptId, 9999, '2026-01-01T01:00:02Z');
    await store.setMeterBaseline(attemptId, 10, '2026-01-01T01:00:01Z');
    await store.setMeterBaseline(attemptId, 99, '2026-01-01T01:00:02Z');
    const running = await store.transitionAttempt(attemptId, 'running', '2026-01-01T01:00:01Z', undefined, 'running');
    assert.equal(running.state, 'running');
    assert.equal(running.attempt.monotonic_start_ms, 1000);
    assert.equal(running.attempt.meter_baseline_m3, 10);
    assert.equal(running.attempt.actual_started_at, '2026-01-01T01:00:01Z');

    await store.recordAttemptProgress(attemptId, 5000, 0.01);
    const stopping = await store.transitionAttempt(attemptId, 'stopping', '2026-01-01T01:00:06Z', 'duration_target_reached', 'running', null, 5000);
    assert.equal(stopping.state, 'stopping');
    assert.equal(stopping.attempt.actual_runtime_ms, 5000);
    const completed = await store.transitionAttempt(attemptId, 'completed', '2026-01-01T01:00:07Z', 'sequence_completed', 'idle');
    assert.equal(completed.state, 'completed');
    assert.equal(completed.active_attempt_id, null);
    assert.equal(completed.attempt.actual_ended_at, '2026-01-01T01:00:07Z');
    // final_reason is COALESCEd, so it keeps the reason the run ended for.
    assert.equal(completed.attempt.final_reason, 'duration_target_reached');
    assert.equal(completed.attempt.delivered_m3, 0.01);
  } finally { await store.close(); fs.rmSync(directory, { recursive: true, force: true }); }
});

test('expiry and recovery_cleared leave no plan stuck', async () => {
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'watering-plan-store-'));
  const store = new WateringPlanStore({ databasePath: path.join(directory, 'plans.sqlite3'), migrationsDir });
  try {
    await store.initialize();
    await store.accept({ plan: plan(1), resolvedValveIds: ['valve1'], acceptedMaxRuntimeSec: 600 });
    assert.equal(await store.expireDue('2026-01-01T01:59:59Z'), 0);
    assert.equal(await store.expireDue('2026-01-01T02:00:01Z'), 1);
    assert.equal((await store.get('plan-1')).state, 'expired');

    // recovery_required blocks every claim until an operator cancels it.
    await store.accept({ plan: Object.assign(plan(1), { plan_id: 'plan-2' }), resolvedValveIds: ['valve1'], acceptedMaxRuntimeSec: 600 });
    const claimed = await store.claim('2026-01-01T01:10:00Z', 'scheduler');
    assert.equal((await store.recover('2026-01-01T01:10:01Z')).recovered, 1);
    assert.equal((await store.claim('2026-01-01T01:10:02Z', 'scheduler')), null);
    const cleared = await store.cancel('plan-2', '2026-01-01T01:10:03Z');
    assert.equal(cleared.state, 'cancelled');
    const history = await store.get('plan-2', true);
    assert.equal(history.events[history.events.length - 1].reason_code, 'recovery_cleared');
    // The attempt keeps the recovery_required record for audit.
    assert.equal(history.attempts[0].state, 'recovery_required');
    assert.equal(claimed.attempt.attempt_id, history.attempts[0].attempt_id);
  } finally { await store.close(); fs.rmSync(directory, { recursive: true, force: true }); }
});

test('overlapping writes share one connection without SQLITE_BUSY', async () => {
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'watering-plan-store-'));
  const store = new WateringPlanStore({ databasePath: path.join(directory, 'plans.sqlite3'), migrationsDir });
  try {
    await store.initialize();
    await Promise.all(Array.from({ length: 12 }, (_, index) => store.accept({
      plan: Object.assign(plan(1), { plan_id: 'plan-' + index, targets: { mode: 'selected', field_ids: ['field-' + index] } }),
      resolvedValveIds: ['valve1'], acceptedMaxRuntimeSec: 600
    })));
    assert.equal((await store.list({ limit: 50 })).items.length, 12);
  } finally { await store.close(); fs.rmSync(directory, { recursive: true, force: true }); }
});

test('recover marks active plan and its attempt for reconciliation', async () => {
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'watering-plan-store-'));
  const store = new WateringPlanStore({ databasePath: path.join(directory, 'plans.sqlite3'), migrationsDir });
  try {
    await store.initialize();
    await store.accept({ plan: plan(1), resolvedValveIds: ['valve1'], acceptedMaxRuntimeSec: 600 });
    const claimed = await store.claim('2026-01-01T01:10:00Z', 'scheduler');
    assert.equal(claimed.state, 'starting');
    assert.equal(claimed.attempt.state, 'starting');
    assert.deepEqual(await store.recover('2026-01-01T01:10:01Z'), { recovered: 1 });
    const recovered = await store.get('plan-1', true);
    assert.equal(recovered.state, 'recovery_required');
    assert.equal(recovered.attempts[0].state, 'recovery_required');
  } finally { await store.close(); fs.rmSync(directory, { recursive: true, force: true }); }
});
