-- Initial durable store for the Raspberry Pi watering-plan prototype.
-- Keep plan payloads as canonical JSON text so this schema does not require
-- SQLite's optional JSON extension. Apply once to a new database.

PRAGMA foreign_keys = ON;
PRAGMA journal_mode = WAL;
PRAGMA synchronous = FULL;

BEGIN IMMEDIATE;

CREATE TABLE IF NOT EXISTS schema_migrations (
    version       INTEGER PRIMARY KEY,
    applied_at    TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS watering_plans (
    plan_id                 TEXT PRIMARY KEY,
    revision                INTEGER NOT NULL CHECK (revision > 0),
    canonical_payload       TEXT NOT NULL,
    payload_sha256          TEXT NOT NULL,
    source                  TEXT NOT NULL CHECK (source IN ('human', 'ai')),
    plan_group_id           TEXT,
    summary                 TEXT,
    timezone                TEXT NOT NULL,
    start_at                TEXT NOT NULL,
    valid_until             TEXT NOT NULL,
    resolved_valves_json    TEXT NOT NULL,
    stop_type               TEXT NOT NULL CHECK (stop_type IN ('duration', 'volume')),
    stop_value              REAL NOT NULL CHECK (stop_value > 0),
    stop_unit               TEXT NOT NULL,
    requested_max_runtime_sec INTEGER NOT NULL CHECK (requested_max_runtime_sec > 0),
    accepted_max_runtime_sec  INTEGER NOT NULL CHECK (accepted_max_runtime_sec > 0),
    state                   TEXT NOT NULL CHECK (state IN (
                                'scheduled', 'delayed', 'starting', 'running',
                                'stopping', 'stop_requested', 'completed',
                                'failed', 'expired', 'cancelled', 'recovery_required'
                            )),
    accepted_at             TEXT NOT NULL,
    updated_at              TEXT NOT NULL,
    last_delay_reason       TEXT,
    active_attempt_id       TEXT REFERENCES watering_attempts(attempt_id),
    CHECK (accepted_max_runtime_sec <= requested_max_runtime_sec)
);

CREATE INDEX IF NOT EXISTS watering_plans_due_order
    ON watering_plans(state, start_at, accepted_at, plan_id);

CREATE INDEX IF NOT EXISTS watering_plans_group
    ON watering_plans(plan_group_id, start_at);

CREATE TABLE IF NOT EXISTS watering_attempts (
    attempt_id              TEXT PRIMARY KEY,
    plan_id                 TEXT NOT NULL REFERENCES watering_plans(plan_id),
    plan_revision           INTEGER NOT NULL CHECK (plan_revision > 0),
    attempt_number          INTEGER NOT NULL CHECK (attempt_number > 0),
    state                   TEXT NOT NULL CHECK (state IN (
                                'starting', 'running', 'stopping', 'stop_requested',
                                'completed', 'failed', 'recovery_required'
                            )),
    resolved_valves_json    TEXT NOT NULL,
    requested_stop_type     TEXT NOT NULL CHECK (requested_stop_type IN ('duration', 'volume')),
    requested_stop_value    REAL NOT NULL CHECK (requested_stop_value > 0),
    requested_stop_unit     TEXT NOT NULL,
    hard_max_runtime_sec    INTEGER NOT NULL CHECK (hard_max_runtime_sec > 0),
    accepted_at             TEXT NOT NULL,
    claimed_at              TEXT,
    actual_started_at       TEXT,
    actual_ended_at         TEXT,
    monotonic_start_ms      INTEGER,
    meter_baseline_m3       REAL,
    delivered_m3            REAL,
    dashboard_phase         TEXT,
    dashboard_notice        TEXT,
    operation_id            TEXT,
    final_reason            TEXT,
    UNIQUE(plan_id, attempt_number)
);

CREATE INDEX IF NOT EXISTS watering_attempts_plan
    ON watering_attempts(plan_id, attempt_number);

CREATE UNIQUE INDEX IF NOT EXISTS watering_attempts_operation
    ON watering_attempts(operation_id)
    WHERE operation_id IS NOT NULL;

-- This is the append-only audit stream for API decisions, plan revisions,
-- scheduler transitions, executor observations, and recovery decisions.
CREATE TABLE IF NOT EXISTS watering_plan_events (
    event_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    plan_id         TEXT NOT NULL REFERENCES watering_plans(plan_id),
    attempt_id      TEXT REFERENCES watering_attempts(attempt_id),
    revision        INTEGER NOT NULL CHECK (revision > 0),
    event_type      TEXT NOT NULL,
    from_state      TEXT,
    to_state        TEXT,
    occurred_at     TEXT NOT NULL,
    reason_code     TEXT,
    details_json    TEXT NOT NULL DEFAULT '{}'
);

CREATE INDEX IF NOT EXISTS watering_plan_events_history
    ON watering_plan_events(plan_id, event_id);

CREATE TRIGGER IF NOT EXISTS watering_plan_events_no_update
BEFORE UPDATE ON watering_plan_events
BEGIN
    SELECT RAISE(ABORT, 'watering_plan_events is append-only');
END;

CREATE TRIGGER IF NOT EXISTS watering_plan_events_no_delete
BEFORE DELETE ON watering_plan_events
BEGIN
    SELECT RAISE(ABORT, 'watering_plan_events is append-only');
END;

INSERT OR IGNORE INTO schema_migrations(version, applied_at)
VALUES (1, strftime('%Y-%m-%dT%H:%M:%fZ', 'now'));

COMMIT;
