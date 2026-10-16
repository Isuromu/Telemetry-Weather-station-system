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
