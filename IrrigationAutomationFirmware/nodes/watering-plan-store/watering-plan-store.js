'use strict';

const crypto = require('crypto');
const fs = require('fs');
const path = require('path');
const sqlite3 = require('sqlite3');

const CLAIMABLE = ['scheduled', 'delayed'];
const ACTIVE = ['starting', 'running', 'stopping', 'stop_requested'];
const BLOCKING = ACTIVE.concat(['recovery_required']);

class StoreError extends Error {
  constructor(code, message, details) {
    super(message);
    this.code = code;
    this.details = details || [];
  }
}

function canonicalize(value) {
  if (value === null || typeof value === 'boolean' || typeof value === 'string') return value;
  if (typeof value === 'number') {
    if (!Number.isFinite(value)) throw new StoreError('invalid_payload', 'Canonical JSON cannot contain a non-finite number');
    return value;
  }
  if (Array.isArray(value)) return value.map(canonicalize);
  if (!value || typeof value !== 'object') throw new StoreError('invalid_payload', 'Plan must contain JSON values only');
  const result = {};
  Object.keys(value).sort().forEach(key => { result[key] = canonicalize(value[key]); });
  return result;
}

function canonicalDigest(plan) {
  const copy = canonicalize(plan);
  if (copy.targets && Array.isArray(copy.targets.field_ids)) copy.targets.field_ids.sort();
  const canonicalPayload = JSON.stringify(copy);
  return { canonicalPayload, payloadSha256: crypto.createHash('sha256').update(canonicalPayload).digest('hex') };
}

function now() { return new Date().toISOString().replace(/\.\d{3}Z$/, 'Z'); }
function normalizeTime(value) {
  const time = new Date(value);
  if (!value || Number.isNaN(time.getTime())) throw new StoreError('invalid_timestamp', 'Timestamp is invalid', [value]);
  return time.toISOString().replace(/\.\d{3}Z$/, 'Z');
}
function parseJson(value) { return JSON.parse(value); }
function planFromRow(row) {
  if (!row) return null;
  const plan = Object.assign({}, row);
  plan.payload = parseJson(plan.canonical_payload);
  plan.resolved_valves = parseJson(plan.resolved_valves_json);
  delete plan.canonical_payload;
  delete plan.resolved_valves_json;
  return plan;
}
function attemptFromRow(row) {
  if (!row) return null;
  const attempt = Object.assign({}, row);
  attempt.resolved_valves = parseJson(attempt.resolved_valves_json);
  delete attempt.resolved_valves_json;
  return attempt;
}
function cursorEncode(row) { return Buffer.from(JSON.stringify({ start_at: row.start_at, plan_id: row.plan_id })).toString('base64url'); }
function cursorDecode(cursor) {
  try {
    const value = JSON.parse(Buffer.from(cursor, 'base64url').toString('utf8'));
    if (!value || typeof value.start_at !== 'string' || typeof value.plan_id !== 'string') throw new Error('bad cursor');
    return value;
  } catch (_) { throw new StoreError('invalid_cursor', 'cursor is invalid'); }
}

class WateringPlanStore {
  constructor(options) {
    this.databasePath = options.databasePath;
    this.migrationsDir = options.migrationsDir;
    if (!this.databasePath) throw new StoreError('invalid_configuration', 'databasePath is required');
    if (!this.migrationsDir) throw new StoreError('invalid_configuration', 'migrationsDir is required');
  }
  open() {
    fs.mkdirSync(path.dirname(this.databasePath), { recursive: true });
    const db = new sqlite3.Database(this.databasePath);
    return new Promise((resolve, reject) => {
      db.serialize(() => {
        db.run('PRAGMA foreign_keys = ON'); db.run('PRAGMA journal_mode = WAL'); db.run('PRAGMA synchronous = FULL'); db.run('PRAGMA busy_timeout = 10000');
        db.get('SELECT 1', error => error ? reject(error) : resolve(db));
      });
    });
  }
  static run(db, sql, params) { return new Promise((resolve, reject) => db.run(sql, params || [], function(error) { error ? reject(error) : resolve(this); })); }
  static get(db, sql, params) { return new Promise((resolve, reject) => db.get(sql, params || [], (error, row) => error ? reject(error) : resolve(row))); }
  static all(db, sql, params) { return new Promise((resolve, reject) => db.all(sql, params || [], (error, rows) => error ? reject(error) : resolve(rows))); }
  static exec(db, sql) { return new Promise((resolve, reject) => db.exec(sql, error => error ? reject(error) : resolve())); }
  static close(db) { return new Promise(resolve => db.close(() => resolve())); }
  async initialize() {
    const files = fs.readdirSync(this.migrationsDir).filter(name => /^\d+_.+\.sql$/.test(name)).sort();
    if (!files.length) throw new StoreError('migration_not_found', 'No numbered SQL migrations found', [this.migrationsDir]);
    const db = await this.open();
    try {
      await WateringPlanStore.exec(db, 'CREATE TABLE IF NOT EXISTS schema_migrations (version INTEGER PRIMARY KEY, applied_at TEXT NOT NULL)');
      const applied = await WateringPlanStore.all(db, 'SELECT version FROM schema_migrations');
      const versions = new Set(applied.map(row => row.version));
      for (const file of files) {
        const version = Number(/^\d+/.exec(file)[0]);
        // Each migration file records its own version in schema_migrations, so
        // this loop only runs the files the table does not already list.
        if (!versions.has(version)) await WateringPlanStore.exec(db, fs.readFileSync(path.join(this.migrationsDir, file), 'utf8'));
      }
    } finally { await WateringPlanStore.close(db); }
  }
  async transaction(fn) {
    const db = await this.open();
    try { await WateringPlanStore.exec(db, 'BEGIN IMMEDIATE'); const result = await fn(db); await WateringPlanStore.exec(db, 'COMMIT'); return result; }
    catch (error) { try { await WateringPlanStore.exec(db, 'ROLLBACK'); } catch (_) {} throw error; }
    finally { await WateringPlanStore.close(db); }
  }
  async event(db, planId, revision, type, fromState, toState, occurredAt, reason, details, attemptId) {
    await WateringPlanStore.run(db, 'INSERT INTO watering_plan_events (plan_id, attempt_id, revision, event_type, from_state, to_state, occurred_at, reason_code, details_json) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)', [planId, attemptId || null, revision, type, fromState, toState, occurredAt, reason || null, JSON.stringify(details || {})]);
  }
  validateAccept(input) {
    const plan = input.plan;
    if (!plan || typeof plan !== 'object' || Array.isArray(plan)) throw new StoreError('invalid_payload', 'msg.plan must be an object');
    if (typeof plan.plan_id !== 'string' || !plan.plan_id) throw new StoreError('invalid_plan_id', 'plan.plan_id is required');
    if (!Number.isInteger(plan.revision) || plan.revision < 1) throw new StoreError('invalid_revision', 'plan.revision must be a positive integer');
    if (plan.status !== 'scheduled' && plan.status !== 'cancelled') throw new StoreError('invalid_status', 'plan.status must be scheduled or cancelled');
    if (!['human', 'ai'].includes(plan.source)) throw new StoreError('invalid_source', 'plan.source must be human or ai');
    if (!plan.stop || !['duration', 'volume'].includes(plan.stop.type) || typeof plan.stop.value !== 'number' || plan.stop.value <= 0 || typeof plan.stop.unit !== 'string' || !plan.stop.unit) throw new StoreError('invalid_stop_rule', 'plan.stop is invalid');
    if (!Number.isInteger(plan.hard_max_runtime_sec) || plan.hard_max_runtime_sec < 1) throw new StoreError('invalid_hard_max_runtime', 'plan.hard_max_runtime_sec must be positive');
    if (!Array.isArray(input.resolvedValveIds) || !input.resolvedValveIds.length || input.resolvedValveIds.some(id => typeof id !== 'string' || !id)) throw new StoreError('invalid_resolved_valves', 'msg.resolvedValveIds must be a non-empty string array');
    if (!Number.isInteger(input.acceptedMaxRuntimeSec) || input.acceptedMaxRuntimeSec < 1 || input.acceptedMaxRuntimeSec > plan.hard_max_runtime_sec) throw new StoreError('invalid_accepted_runtime', 'msg.acceptedMaxRuntimeSec must be positive and no greater than plan.hard_max_runtime_sec');
    return plan;
  }
  async accept(input) {
    const plan = this.validateAccept(input); const digest = canonicalDigest(plan); const acceptedAt = input.acceptedAt ? normalizeTime(input.acceptedAt) : now();
    const state = plan.status === 'cancelled' ? 'cancelled' : 'scheduled'; const valves = JSON.stringify([...input.resolvedValveIds].sort());
    return this.transaction(async db => {
      const existing = await WateringPlanStore.get(db, 'SELECT * FROM watering_plans WHERE plan_id=?', [plan.plan_id]);
      if (existing) {
        if (existing.revision === plan.revision) {
          if (existing.payload_sha256 === digest.payloadSha256) { const result = planFromRow(existing); result.idempotent = true; return result; }
          throw new StoreError('revision_conflict', 'Same revision contains different plan content', [plan.plan_id]);
        }
        if (existing.revision > plan.revision) throw new StoreError('stale_revision', 'Plan revision is older than stored revision', [plan.plan_id]);
        if (!CLAIMABLE.includes(existing.state)) throw new StoreError('active_revision_conflict', 'Higher revision may replace only scheduled or delayed plans', [plan.plan_id, existing.state]);
        await WateringPlanStore.run(db, `UPDATE watering_plans SET revision=?, canonical_payload=?, payload_sha256=?, source=?, plan_group_id=?, summary=?, timezone=?, start_at=?, valid_until=?, resolved_valves_json=?, stop_type=?, stop_value=?, stop_unit=?, requested_max_runtime_sec=?, accepted_max_runtime_sec=?, state=?, accepted_at=?, updated_at=?, last_delay_reason=NULL, active_attempt_id=NULL WHERE plan_id=?`, [plan.revision, digest.canonicalPayload, digest.payloadSha256, plan.source, plan.plan_group_id || null, plan.summary || null, plan.timezone, normalizeTime(plan.start_at), normalizeTime(plan.valid_until), valves, plan.stop.type, plan.stop.value, plan.stop.unit, plan.hard_max_runtime_sec, input.acceptedMaxRuntimeSec, state, acceptedAt, acceptedAt, plan.plan_id]);
        await this.event(db, plan.plan_id, plan.revision, 'plan_replaced', existing.state, state, acceptedAt, 'higher_revision', { previous_revision: existing.revision });
        const result = planFromRow(await WateringPlanStore.get(db, 'SELECT * FROM watering_plans WHERE plan_id=?', [plan.plan_id])); result.replaced = true; return result;
      }
      await WateringPlanStore.run(db, `INSERT INTO watering_plans (plan_id, revision, canonical_payload, payload_sha256, source, plan_group_id, summary, timezone, start_at, valid_until, resolved_valves_json, stop_type, stop_value, stop_unit, requested_max_runtime_sec, accepted_max_runtime_sec, state, accepted_at, updated_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)`, [plan.plan_id, plan.revision, digest.canonicalPayload, digest.payloadSha256, plan.source, plan.plan_group_id || null, plan.summary || null, plan.timezone, normalizeTime(plan.start_at), normalizeTime(plan.valid_until), valves, plan.stop.type, plan.stop.value, plan.stop.unit, plan.hard_max_runtime_sec, input.acceptedMaxRuntimeSec, state, acceptedAt, acceptedAt]);
      await this.event(db, plan.plan_id, plan.revision, 'plan_accepted', null, state, acceptedAt, 'accepted', { resolved_valves: JSON.parse(valves) });
      const result = planFromRow(await WateringPlanStore.get(db, 'SELECT * FROM watering_plans WHERE plan_id=?', [plan.plan_id])); result.created = true; return result;
    });
  }
  async get(planId, includeHistory) {
    const db = await this.open(); try { const row = await WateringPlanStore.get(db, 'SELECT * FROM watering_plans WHERE plan_id=?', [planId]); if (!row) throw new StoreError('plan_not_found', 'Watering plan was not found', [planId]); const plan = planFromRow(row); if (includeHistory) { plan.events = await WateringPlanStore.all(db, 'SELECT * FROM watering_plan_events WHERE plan_id=? ORDER BY event_id', [planId]); plan.attempts = (await WateringPlanStore.all(db, 'SELECT * FROM watering_attempts WHERE plan_id=? ORDER BY attempt_number', [planId])).map(attemptFromRow); } return plan; } finally { await WateringPlanStore.close(db); }
  }
  async list(query) {
    query = query || {}; const limit = Math.max(1, Math.min(Number.isInteger(query.limit) ? query.limit : 50, 100)); const clauses = []; const params = [];
    if (query.fromAt) { clauses.push('start_at >= ?'); params.push(normalizeTime(query.fromAt)); } if (query.toAt) { clauses.push('start_at <= ?'); params.push(normalizeTime(query.toAt)); } if (query.cursor) { const cursor = cursorDecode(query.cursor); clauses.push('(start_at > ? OR (start_at = ? AND plan_id > ?))'); params.push(cursor.start_at, cursor.start_at, cursor.plan_id); }
    const db = await this.open(); try { const rows = await WateringPlanStore.all(db, `SELECT * FROM watering_plans${clauses.length ? ' WHERE ' + clauses.join(' AND ') : ''} ORDER BY start_at, plan_id LIMIT ?`, params.concat([limit + 1])); const more = rows.length > limit; const items = rows.slice(0, limit).map(planFromRow); return { items, next_cursor: more ? cursorEncode(items[items.length - 1]) : null }; } finally { await WateringPlanStore.close(db); }
  }
  async cancel(planId, at) {
    const occurredAt = at ? normalizeTime(at) : now(); return this.transaction(async db => { const row = await WateringPlanStore.get(db, 'SELECT * FROM watering_plans WHERE plan_id=?', [planId]); if (!row) throw new StoreError('plan_not_found', 'Watering plan was not found', [planId]); let target; let reason; if (CLAIMABLE.includes(row.state)) { target = 'cancelled'; reason = 'cancelled_by_client'; } else if (ACTIVE.includes(row.state)) { target = 'stop_requested'; reason = 'stop_requested_by_client'; } else return planFromRow(row); await WateringPlanStore.run(db, 'UPDATE watering_plans SET state=?, updated_at=? WHERE plan_id=? AND state=?', [target, occurredAt, planId, row.state]); await this.event(db, planId, row.revision, 'plan_cancelled', row.state, target, occurredAt, reason); return planFromRow(await WateringPlanStore.get(db, 'SELECT * FROM watering_plans WHERE plan_id=?', [planId])); });
  }
  async claim(at, ownerId) {
    const occurredAt = at ? normalizeTime(at) : now(); const owner = ownerId || crypto.randomUUID();
    return this.transaction(async db => {
      const active = await WateringPlanStore.get(db, `SELECT 1 FROM watering_plans WHERE state IN (${BLOCKING.map(() => '?').join(',')}) LIMIT 1`, BLOCKING);
      if (active) return null;
      const row = await WateringPlanStore.get(db, "SELECT * FROM watering_plans WHERE state IN ('scheduled','delayed') AND start_at <= ? AND valid_until > ? ORDER BY start_at, accepted_at, plan_id LIMIT 1", [occurredAt, occurredAt]);
      if (!row) return null;
      const attemptId = crypto.randomUUID();
      const number = (await WateringPlanStore.get(db, 'SELECT COALESCE(MAX(attempt_number), 0) + 1 AS value FROM watering_attempts WHERE plan_id=?', [row.plan_id])).value;
      await WateringPlanStore.run(db, "INSERT INTO watering_attempts (attempt_id, plan_id, plan_revision, attempt_number, state, resolved_valves_json, requested_stop_type, requested_stop_value, requested_stop_unit, hard_max_runtime_sec, accepted_at, claimed_at, operation_id) VALUES (?, ?, ?, ?, 'starting', ?, ?, ?, ?, ?, ?, ?, ?)", [attemptId, row.plan_id, row.revision, number, row.resolved_valves_json, row.stop_type, row.stop_value, row.stop_unit, row.accepted_max_runtime_sec, row.accepted_at, occurredAt, owner + ':' + attemptId]);
      const update = await WateringPlanStore.run(db, "UPDATE watering_plans SET state='starting', active_attempt_id=?, updated_at=? WHERE plan_id=? AND state IN ('scheduled','delayed') AND start_at <= ? AND valid_until > ? AND active_attempt_id IS NULL", [attemptId, occurredAt, row.plan_id, occurredAt, occurredAt]);
      if (update.changes !== 1) throw new StoreError('claim_conflict', 'Due plan changed before it could be claimed', [row.plan_id]);
      await this.event(db, row.plan_id, row.revision, 'plan_claimed', row.state, 'starting', occurredAt, 'claimed', { owner_id: owner }, attemptId);
      const plan = planFromRow(await WateringPlanStore.get(db, 'SELECT * FROM watering_plans WHERE plan_id=?', [row.plan_id])); plan.attempt = attemptFromRow(await WateringPlanStore.get(db, 'SELECT * FROM watering_attempts WHERE attempt_id=?', [attemptId])); return plan;
    });
  }
  async recover(at) {
    const occurredAt = at ? normalizeTime(at) : now(); return this.transaction(async db => { const rows = await WateringPlanStore.all(db, `SELECT * FROM watering_plans WHERE state IN (${ACTIVE.map(() => '?').join(',')})`, ACTIVE); for (const row of rows) { await WateringPlanStore.run(db, "UPDATE watering_plans SET state='recovery_required', updated_at=? WHERE plan_id=?", [occurredAt, row.plan_id]); if (row.active_attempt_id) await WateringPlanStore.run(db, "UPDATE watering_attempts SET state='recovery_required', actual_ended_at=?, final_reason='restart_reconciliation' WHERE attempt_id=?", [occurredAt, row.active_attempt_id]); await this.event(db, row.plan_id, row.revision, 'restart_reconciliation', row.state, 'recovery_required', occurredAt, 'restart_reconciliation', {}, row.active_attempt_id); } return { recovered: rows.length }; });
  }
}

module.exports = function(RED) {
  function WateringPlanStoreNode(config) {
    RED.nodes.createNode(this, config); const node = this;
    const migrationsDir = config.migrationsDir || process.env.WATERING_PLAN_MIGRATIONS_DIR || path.resolve(__dirname, '../../docs/integrations/watering-plan/sqlite');
    const store = new WateringPlanStore({ databasePath: config.databasePath || '/var/lib/node-red/watering-plans.sqlite3', migrationsDir });
    let ready = store.initialize(); ready.catch(error => node.error(error));
    node.on('input', async (msg, send, done) => {
      try { await ready; const action = msg.action; let result;
        if (action === 'accept') result = await store.accept(msg);
        else if (action === 'get') result = await store.get(msg.planId, msg.includeHistory === true);
        else if (action === 'list') result = await store.list(msg.query);
        else if (action === 'cancel') result = await store.cancel(msg.planId, msg.at);
        else if (action === 'claim') result = await store.claim(msg.at, msg.ownerId);
        else if (action === 'recover') result = await store.recover(msg.at);
        else throw new StoreError('invalid_action', 'msg.action must be accept, get, list, cancel, claim, or recover');
        msg.payload = { ok: true, action, result }; send(msg); if (done) done();
      } catch (error) { const safe = { code: error.code || 'store_error', message: error.message || 'Watering plan store failed', details: error.details || [] }; msg.payload = { ok: false, action: msg.action, error: safe }; node.error(error, msg); send(msg); if (done) done(error); }
    });
  }
  RED.nodes.registerType('watering-plan-store', WateringPlanStoreNode);
};
module.exports.WateringPlanStore = WateringPlanStore;
module.exports.StoreError = StoreError;
module.exports.canonicalDigest = canonicalDigest;
