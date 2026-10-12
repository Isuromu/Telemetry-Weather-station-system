"""Transactional SQLite store for version-1 watering plans."""

from __future__ import annotations

import base64
import json
import sqlite3
import time
import uuid
from contextlib import contextmanager
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Dict, Iterator, Optional

try:
    from migrate_watering_plan_db import migrate
except ImportError:  # pragma: no cover - package invocation
    from .migrate_watering_plan_db import migrate


ROOT = Path(__file__).resolve().parents[1]
SCHEMA = ROOT / "docs" / "integrations" / "watering-plan" / "sqlite" / "001_initial.sql"
ACTIVE_PLAN_STATES = ("starting", "running", "stopping", "stop_requested")
BLOCKING_PLAN_STATES = ACTIVE_PLAN_STATES + ("recovery_required",)
CLAIMABLE_STATES = ("scheduled", "delayed")


def normalize_timestamp(value: str) -> str:
    """Normalize offset-bearing ISO timestamps before SQLite ordering/comparison."""
    parsed = datetime.fromisoformat(value.replace("Z", "+00:00"))
    return parsed.astimezone(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


class PlanStoreError(Exception):
    """Base store error with API-safe code."""

    def __init__(self, code: str, message: str, details: Optional[list] = None):
        super().__init__(message)
        self.code = code
        self.message = message
        self.details = details or []


class PlanNotFound(PlanStoreError):
    def __init__(self, plan_id: str):
        super().__init__("plan_not_found", "Watering plan was not found", [plan_id])


class WateringPlanStore:
    def __init__(self, database: str, schema_path: str = str(SCHEMA)):
        self.database = database
        self.schema_path = Path(schema_path)
        self.initialize()

    def _connection(self) -> sqlite3.Connection:
        connection = sqlite3.connect(self.database, timeout=10, isolation_level=None)
        connection.row_factory = sqlite3.Row
        connection.execute("PRAGMA foreign_keys = ON")
        connection.execute("PRAGMA journal_mode = WAL")
        connection.execute("PRAGMA synchronous = FULL")
        connection.execute("PRAGMA busy_timeout = 10000")
        return connection

    def initialize(self) -> None:
        # Always run the numbered migrations. This keeps an installed Phase 1/2
        # database compatible with Phase 3 without recreating its plan history.
        if self.schema_path == SCHEMA:
            migrate(self.database)
            return
        connection = self._connection()
        try:
            exists = connection.execute(
                "SELECT 1 FROM sqlite_master WHERE type='table' AND name='schema_migrations'"
            ).fetchone()
            if exists is None:
                connection.executescript(self.schema_path.read_text(encoding="utf-8"))
        finally:
            connection.close()

    @contextmanager
    def _transaction(self) -> Iterator[sqlite3.Connection]:
        connection = self._connection()
        try:
            connection.execute("BEGIN IMMEDIATE")
            yield connection
            connection.execute("COMMIT")
        except Exception:
            connection.execute("ROLLBACK")
            raise
        finally:
            connection.close()

    @staticmethod
    def _now() -> str:
        return time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())

    @staticmethod
    def _details(value: Any) -> str:
        return json.dumps(value if value is not None else {}, sort_keys=True, separators=(",", ":"))

    @staticmethod
    def _decode_json(value: str) -> Any:
        return json.loads(value)

    @classmethod
    def _plan(cls, row: sqlite3.Row) -> Dict[str, Any]:
        plan = dict(row)
        plan["payload"] = cls._decode_json(plan.pop("canonical_payload"))
        plan["resolved_valves"] = cls._decode_json(plan.pop("resolved_valves_json"))
        return plan

    @classmethod
    def _attempt(cls, row: sqlite3.Row) -> Dict[str, Any]:
        attempt = dict(row)
        attempt["resolved_valves"] = cls._decode_json(attempt.pop("resolved_valves_json"))
        return attempt

    def _active_attempt(self, connection: sqlite3.Connection, attempt_id: Optional[str]) -> Optional[Dict[str, Any]]:
        if not attempt_id:
            return None
        row = connection.execute(
            "SELECT * FROM watering_attempts WHERE attempt_id = ?", (attempt_id,)
        ).fetchone()
        return self._attempt(row) if row else None

    def get_plan(self, plan_id: str, include_history: bool = False) -> Dict[str, Any]:
        connection = self._connection()
        try:
            row = connection.execute(
                "SELECT * FROM watering_plans WHERE plan_id = ?", (plan_id,)
            ).fetchone()
            if row is None:
                raise PlanNotFound(plan_id)
            plan = self._plan(row)
            active_attempt = self._active_attempt(connection, plan.get("active_attempt_id"))
            if active_attempt:
                plan["attempt"] = active_attempt
            if include_history:
                plan["events"] = self.events(plan_id, connection=connection)
                plan["attempts"] = [
                    self._attempt(attempt)
                    for attempt in connection.execute(
                        "SELECT * FROM watering_attempts WHERE plan_id = ? ORDER BY attempt_number",
                        (plan_id,),
                    ).fetchall()
                ]
            return plan
        finally:
            connection.close()

    def events(self, plan_id: str, connection: Optional[sqlite3.Connection] = None) -> list:
        owns = connection is None
        connection = connection or self._connection()
        try:
            return [dict(row) for row in connection.execute(
                "SELECT * FROM watering_plan_events WHERE plan_id = ? ORDER BY event_id", (plan_id,)
            ).fetchall()]
        finally:
            if owns:
                connection.close()

    def accept_plan(self, validation: Dict[str, Any], accepted_at: Optional[str] = None) -> Dict[str, Any]:
        value = validation["value"]
        payload = value["plan"]
        plan_id = payload["plan_id"]
        now = normalize_timestamp(accepted_at) if accepted_at else self._now()
        state = "cancelled" if payload.get("status") == "cancelled" else "scheduled"
        fields = (
            payload.get("plan_group_id"), payload.get("summary"), payload["timezone"],
            normalize_timestamp(payload["start_at"]), normalize_timestamp(payload["valid_until"]),
            json.dumps(value["resolvedValveIds"], separators=(",", ":")), payload["stop"]["type"],
            payload["stop"]["value"], payload["stop"]["unit"], payload["hard_max_runtime_sec"],
            value["acceptedMaxRuntimeSec"], state, now, now,
        )
        with self._transaction() as connection:
            existing = connection.execute(
                "SELECT * FROM watering_plans WHERE plan_id = ?", (plan_id,)
            ).fetchone()
            if existing is not None:
                existing_plan = self._plan(existing)
                if existing["revision"] == payload["revision"]:
                    if existing["payload_sha256"] == value["payloadSha256"]:
                        existing_plan["idempotent"] = True
                        return existing_plan
                    raise PlanStoreError(
                        "revision_conflict", "Same revision contains different plan content", [plan_id]
                    )
                if existing["revision"] > payload["revision"]:
                    raise PlanStoreError("stale_revision", "Plan revision is older than stored revision", [plan_id])
                if existing["state"] not in CLAIMABLE_STATES:
                    raise PlanStoreError(
                        "active_revision_conflict",
                        "Higher revision may replace only scheduled or delayed plans",
                        [plan_id, existing["state"]],
                    )
                connection.execute(
                    """UPDATE watering_plans SET revision=?, canonical_payload=?, payload_sha256=?,
                       source=?, plan_group_id=?, summary=?, timezone=?, start_at=?, valid_until=?,
                       resolved_valves_json=?, stop_type=?, stop_value=?, stop_unit=?,
                       requested_max_runtime_sec=?, accepted_max_runtime_sec=?, state=?,
                       accepted_at=?, updated_at=?, last_delay_reason=NULL, active_attempt_id=NULL
                       WHERE plan_id=?""",
                    (payload["revision"], value["canonicalPayload"], value["payloadSha256"],
                     payload["source"], *fields, plan_id),
                )
                self._event(connection, plan_id, payload["revision"], "plan_replaced",
                            existing["state"], state, now, "higher_revision", {"previous_revision": existing["revision"]})
                result = self._plan(connection.execute(
                    "SELECT * FROM watering_plans WHERE plan_id = ?", (plan_id,)
                ).fetchone())
                result["replaced"] = True
                return result
            connection.execute(
                """INSERT INTO watering_plans(
                   plan_id, revision, canonical_payload, payload_sha256, source, plan_group_id,
                   summary, timezone, start_at, valid_until, resolved_valves_json, stop_type,
                   stop_value, stop_unit, requested_max_runtime_sec, accepted_max_runtime_sec,
                   state, accepted_at, updated_at)
                   VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)""",
                (plan_id, payload["revision"], value["canonicalPayload"], value["payloadSha256"],
                 payload["source"], *fields),
            )
            self._event(connection, plan_id, payload["revision"], "plan_accepted", None, state,
                        now, "accepted", {"resolved_valves": value["resolvedValveIds"]})
            result = self._plan(connection.execute(
                "SELECT * FROM watering_plans WHERE plan_id = ?", (plan_id,)
            ).fetchone())
            result["created"] = True
            return result

    def _event(self, connection: sqlite3.Connection, plan_id: str, revision: int, event_type: str,
               from_state: Optional[str], to_state: Optional[str], occurred_at: str,
               reason_code: Optional[str], details: Any = None, attempt_id: Optional[str] = None) -> None:
        connection.execute(
            """INSERT INTO watering_plan_events(
               plan_id, attempt_id, revision, event_type, from_state, to_state,
               occurred_at, reason_code, details_json)
               VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)""",
            (plan_id, attempt_id, revision, event_type, from_state, to_state,
             occurred_at, reason_code, self._details(details)),
        )

    def list_plans(self, from_at: Optional[str] = None, to_at: Optional[str] = None,
                   limit: int = 50, cursor: Optional[str] = None) -> Dict[str, Any]:
        limit = max(1, min(int(limit), 100))
        params = []
        clauses = []
        if from_at:
            clauses.append("start_at >= ?")
            params.append(from_at)
        if to_at:
            clauses.append("start_at <= ?")
            params.append(to_at)
        if cursor:
            try:
                decoded = json.loads(base64.urlsafe_b64decode(cursor.encode()).decode())
                clauses.append("(start_at > ? OR (start_at = ? AND plan_id > ?))")
                params.extend([decoded["start_at"], decoded["start_at"], decoded["plan_id"]])
            except (ValueError, KeyError, TypeError, json.JSONDecodeError):
                raise PlanStoreError("invalid_cursor", "cursor is invalid")
        where = (" WHERE " + " AND ".join(clauses)) if clauses else ""
        connection = self._connection()
        try:
            rows = connection.execute(
                "SELECT * FROM watering_plans" + where + " ORDER BY start_at, plan_id LIMIT ?",
                (*params, limit + 1),
            ).fetchall()
            more = len(rows) > limit
            rows = rows[:limit]
            data = [self._plan(row) for row in rows]
            for plan in data:
                attempt = connection.execute(
                    "SELECT * FROM watering_attempts WHERE plan_id = ? ORDER BY attempt_number DESC LIMIT 1",
                    (plan["plan_id"],),
                ).fetchone()
                if attempt is not None:
                    plan["attempt"] = self._attempt(attempt)
            next_cursor = None
            if more:
                last = data[-1]
                next_cursor = base64.urlsafe_b64encode(json.dumps({
                    "start_at": last["start_at"], "plan_id": last["plan_id"]
                }, separators=(",", ":")).encode()).decode()
            return {"items": data, "next_cursor": next_cursor}
        finally:
            connection.close()

    def cancel_plan(self, plan_id: str, now: Optional[str] = None) -> Dict[str, Any]:
        now = normalize_timestamp(now) if now else self._now()
        with self._transaction() as connection:
            row = connection.execute(
                "SELECT * FROM watering_plans WHERE plan_id = ?", (plan_id,)
            ).fetchone()
            if row is None:
                raise PlanNotFound(plan_id)
            state = row["state"]
            if state in CLAIMABLE_STATES:
                target = "cancelled"
                reason = "cancelled_by_client"
            elif state in ACTIVE_PLAN_STATES:
                target = "stop_requested"
                reason = "stop_requested_by_client"
            else:
                return self._plan(row)
            connection.execute(
                "UPDATE watering_plans SET state=?, updated_at=? WHERE plan_id=? AND state=?",
                (target, now, plan_id, state),
            )
            self._event(connection, plan_id, row["revision"], "plan_cancelled", state, target,
                        now, reason)
            return self._plan(connection.execute(
                "SELECT * FROM watering_plans WHERE plan_id = ?", (plan_id,)
            ).fetchone())

    def claim_next_due(self, now: str, owner_id: Optional[str] = None) -> Optional[Dict[str, Any]]:
        owner_id = owner_id or uuid.uuid4().hex
        now = normalize_timestamp(now)
        with self._transaction() as connection:
            active = connection.execute(
                "SELECT 1 FROM watering_plans WHERE state IN (?, ?, ?, ?, ?) LIMIT 1",
                BLOCKING_PLAN_STATES,
            ).fetchone()
            if active:
                return None
            row = connection.execute(
                """SELECT * FROM watering_plans
                   WHERE state IN ('scheduled', 'delayed') AND start_at <= ? AND valid_until > ?
                   ORDER BY start_at, accepted_at, plan_id LIMIT 1""", (now, now)
            ).fetchone()
            if row is None:
                return None
            attempt_id = uuid.uuid4().hex
            attempt_number = connection.execute(
                "SELECT COALESCE(MAX(attempt_number), 0) + 1 FROM watering_attempts WHERE plan_id = ?",
                (row["plan_id"],),
            ).fetchone()[0]
            connection.execute(
                """INSERT INTO watering_attempts(
                   attempt_id, plan_id, plan_revision, attempt_number, state, resolved_valves_json,
                   requested_stop_type, requested_stop_value, requested_stop_unit,
                   hard_max_runtime_sec, accepted_at, claimed_at, operation_id)
                   VALUES (?, ?, ?, ?, 'starting', ?, ?, ?, ?, ?, ?, ?, ?)""",
                (attempt_id, row["plan_id"], row["revision"], attempt_number, row["resolved_valves_json"],
                 row["stop_type"], row["stop_value"], row["stop_unit"], row["accepted_max_runtime_sec"],
                 row["accepted_at"], now, owner_id + ":" + attempt_id),
            )
            updated = connection.execute(
                """UPDATE watering_plans SET state='starting', active_attempt_id=?, updated_at=?
                   WHERE plan_id=? AND state IN ('scheduled', 'delayed')
                     AND start_at <= ? AND valid_until > ? AND active_attempt_id IS NULL""",
                (attempt_id, now, row["plan_id"], now, now),
            )
            if updated.rowcount != 1:
                return None
            self._event(connection, row["plan_id"], row["revision"], "plan_claimed",
                        row["state"], "starting", now, "claimed", {"owner_id": owner_id}, attempt_id)
            plan = self._plan(connection.execute(
                "SELECT * FROM watering_plans WHERE plan_id = ?", (row["plan_id"],)
            ).fetchone())
            plan["attempt"] = self._attempt(connection.execute(
                "SELECT * FROM watering_attempts WHERE attempt_id = ?", (attempt_id,)
            ).fetchone())
            return plan

    def release_claim(self, plan_id: str, reason: str, now: Optional[str] = None) -> Dict[str, Any]:
        """Return an undispatched claim to delayed state atomically."""
        now = normalize_timestamp(now) if now else self._now()
        with self._transaction() as connection:
            row = connection.execute("SELECT * FROM watering_plans WHERE plan_id=?", (plan_id,)).fetchone()
            if row is None:
                raise PlanNotFound(plan_id)
            if row["state"] != "starting" or row["active_attempt_id"] is None:
                return self._plan(row)
            attempt_id = row["active_attempt_id"]
            connection.execute(
                "UPDATE watering_attempts SET state='failed', actual_ended_at=?, final_reason=? WHERE attempt_id=?",
                (now, reason, attempt_id),
            )
            connection.execute(
                "UPDATE watering_plans SET state='delayed', active_attempt_id=NULL, last_delay_reason=?, updated_at=? WHERE plan_id=? AND state='starting'",
                (reason, now, plan_id),
            )
            self._event(connection, plan_id, row["revision"], "claim_delayed", "starting", "delayed",
                        now, reason, attempt_id=attempt_id)
            return self._plan(connection.execute("SELECT * FROM watering_plans WHERE plan_id=?", (plan_id,)).fetchone())

    def mark_delayed(self, plan_id: str, reason: str, now: Optional[str] = None) -> Dict[str, Any]:
        now = normalize_timestamp(now) if now else self._now()
        with self._transaction() as connection:
            row = connection.execute("SELECT * FROM watering_plans WHERE plan_id=?", (plan_id,)).fetchone()
            if row is None:
                raise PlanNotFound(plan_id)
            if row["state"] not in CLAIMABLE_STATES:
                return self._plan(row)
            connection.execute(
                "UPDATE watering_plans SET state='delayed', last_delay_reason=?, updated_at=? WHERE plan_id=?",
                (reason, now, plan_id),
            )
            self._event(connection, plan_id, row["revision"], "plan_delayed", row["state"], "delayed",
                        now, reason)
            return self._plan(connection.execute("SELECT * FROM watering_plans WHERE plan_id=?", (plan_id,)).fetchone())

    def expire_due(self, now: str) -> int:
        now = normalize_timestamp(now)
        with self._transaction() as connection:
            rows = connection.execute(
                "SELECT plan_id, revision, state FROM watering_plans WHERE state IN ('scheduled','delayed') AND valid_until <= ?",
                (now,),
            ).fetchall()
            for row in rows:
                connection.execute(
                    "UPDATE watering_plans SET state='expired', updated_at=? WHERE plan_id=?",
                    (now, row["plan_id"]),
                )
                self._event(connection, row["plan_id"], row["revision"], "plan_expired",
                            row["state"], "expired", now, "valid_until_elapsed")
            return len(rows)

    def transition_attempt(self, attempt_id: str, state: str, now: Optional[str] = None,
                           reason: Optional[str] = None, dashboard_phase: Optional[str] = None,
                           dashboard_notice: Optional[str] = None,
                           actual_runtime_ms: Optional[int] = None,
                           delivered_m3: Optional[float] = None) -> Dict[str, Any]:
        now = normalize_timestamp(now) if now else self._now()
        with self._transaction() as connection:
            attempt = connection.execute("SELECT * FROM watering_attempts WHERE attempt_id=?", (attempt_id,)).fetchone()
            if attempt is None:
                raise PlanStoreError("attempt_not_found", "Watering attempt was not found", [attempt_id])
            plan = connection.execute("SELECT * FROM watering_plans WHERE plan_id=?", (attempt["plan_id"],)).fetchone()
            if state not in ("starting", "running", "stopping", "stop_requested", "completed", "failed", "recovery_required"):
                raise PlanStoreError("invalid_state", "Unsupported attempt state", [state])
            if actual_runtime_ms is not None and actual_runtime_ms < 0:
                raise PlanStoreError("invalid_tracking", "actual runtime cannot be negative")
            if delivered_m3 is not None and delivered_m3 < 0:
                raise PlanStoreError("invalid_tracking", "delivered volume cannot be negative")
            plan_state = state
            if plan["state"] == "stop_requested" and state in ("starting", "running"):
                plan_state = "stop_requested"
            if state == "completed" and attempt["final_reason"] == "cancelled":
                plan_state = "completed"
            connection.execute(
                """UPDATE watering_attempts SET state=?, actual_started_at=CASE WHEN ?='running' AND actual_started_at IS NULL THEN ? ELSE actual_started_at END,
                   actual_ended_at=CASE WHEN ? IN ('completed','failed','recovery_required') THEN ? ELSE actual_ended_at END,
                   dashboard_phase=COALESCE(?, dashboard_phase), dashboard_notice=COALESCE(?, dashboard_notice), final_reason=COALESCE(final_reason, ?),
                   actual_runtime_ms=COALESCE(?, actual_runtime_ms), delivered_m3=COALESCE(?, delivered_m3)
                   WHERE attempt_id=?""",
                (state, state, now, state, now, dashboard_phase, dashboard_notice, reason,
                 actual_runtime_ms, delivered_m3, attempt_id),
            )
            connection.execute(
                "UPDATE watering_plans SET state=?, updated_at=?, active_attempt_id=CASE WHEN ? IN ('completed','failed','recovery_required') THEN NULL ELSE active_attempt_id END WHERE plan_id=?",
                (plan_state, now, state, plan["plan_id"]),
            )
            self._event(connection, plan["plan_id"], plan["revision"], "attempt_state",
                        plan["state"], plan_state, now, reason, {"dashboard_phase": dashboard_phase}, attempt_id)
            return self._plan(connection.execute("SELECT * FROM watering_plans WHERE plan_id=?", (plan["plan_id"],)).fetchone())

    def set_meter_baseline(self, attempt_id: str, baseline_m3: float,
                           now: Optional[str] = None) -> Dict[str, Any]:
        """Persist the commissioned positive-total reading before dispatching a volume run."""
        if baseline_m3 < 0:
            raise PlanStoreError("invalid_tracking", "meter baseline cannot be negative")
        now = normalize_timestamp(now) if now else self._now()
        with self._transaction() as connection:
            attempt = connection.execute("SELECT * FROM watering_attempts WHERE attempt_id=?", (attempt_id,)).fetchone()
            if attempt is None:
                raise PlanStoreError("attempt_not_found", "Watering attempt was not found", [attempt_id])
            if attempt["meter_baseline_m3"] is None:
                connection.execute("UPDATE watering_attempts SET meter_baseline_m3=? WHERE attempt_id=?",
                                   (baseline_m3, attempt_id))
                self._event(connection, attempt["plan_id"], attempt["plan_revision"], "meter_baseline_captured",
                            attempt["state"], attempt["state"], now, "commissioned_positive_total",
                            {"meter_baseline_m3": baseline_m3}, attempt_id)
            return self._attempt(connection.execute(
                "SELECT * FROM watering_attempts WHERE attempt_id=?", (attempt_id,)
            ).fetchone())

    def start_attempt_tracking(self, attempt_id: str, monotonic_start_ms: int,
                               now: Optional[str] = None) -> Dict[str, Any]:
        """Persist the monotonic start point once the dashboard reports running."""
        if monotonic_start_ms < 0:
            raise PlanStoreError("invalid_tracking", "monotonic start cannot be negative")
        now = normalize_timestamp(now) if now else self._now()
        with self._transaction() as connection:
            attempt = connection.execute("SELECT * FROM watering_attempts WHERE attempt_id=?", (attempt_id,)).fetchone()
            if attempt is None:
                raise PlanStoreError("attempt_not_found", "Watering attempt was not found", [attempt_id])
            if attempt["monotonic_start_ms"] is None:
                connection.execute("UPDATE watering_attempts SET monotonic_start_ms=? WHERE attempt_id=?",
                                   (monotonic_start_ms, attempt_id))
                self._event(connection, attempt["plan_id"], attempt["plan_revision"], "target_tracking_started",
                            attempt["state"], attempt["state"], now, "dashboard_running",
                            {"monotonic_start_ms": monotonic_start_ms}, attempt_id)
            return self._attempt(connection.execute(
                "SELECT * FROM watering_attempts WHERE attempt_id=?", (attempt_id,)
            ).fetchone())

    def record_attempt_progress(self, attempt_id: str, actual_runtime_ms: int,
                                delivered_m3: Optional[float] = None) -> Dict[str, Any]:
        """Keep restart-visible target evidence current without changing plan state."""
        if actual_runtime_ms < 0 or (delivered_m3 is not None and delivered_m3 < 0):
            raise PlanStoreError("invalid_tracking", "Attempt progress cannot be negative")
        with self._transaction() as connection:
            attempt = connection.execute("SELECT * FROM watering_attempts WHERE attempt_id=?", (attempt_id,)).fetchone()
            if attempt is None:
                raise PlanStoreError("attempt_not_found", "Watering attempt was not found", [attempt_id])
            connection.execute(
                "UPDATE watering_attempts SET actual_runtime_ms=?, delivered_m3=COALESCE(?, delivered_m3) WHERE attempt_id=?",
                (actual_runtime_ms, delivered_m3, attempt_id),
            )
            return self._attempt(connection.execute(
                "SELECT * FROM watering_attempts WHERE attempt_id=?", (attempt_id,)
            ).fetchone())

    def reconcile_active_attempts(self, now: Optional[str] = None) -> int:
        now = normalize_timestamp(now) if now else self._now()
        with self._transaction() as connection:
            rows = connection.execute(
                "SELECT plan_id, revision, state, active_attempt_id FROM watering_plans WHERE state IN ('starting','running','stopping','stop_requested')"
            ).fetchall()
            for row in rows:
                connection.execute(
                    "UPDATE watering_plans SET state='recovery_required', updated_at=? WHERE plan_id=?",
                    (now, row["plan_id"]),
                )
                if row["active_attempt_id"]:
                    connection.execute(
                        "UPDATE watering_attempts SET state='recovery_required', actual_ended_at=?, final_reason='restart_reconciliation' WHERE attempt_id=?",
                        (now, row["active_attempt_id"]),
                    )
                self._event(connection, row["plan_id"], row["revision"], "restart_reconciliation",
                            row["state"], "recovery_required", now, "restart_reconciliation", attempt_id=row["active_attempt_id"])
            return len(rows)

    def active_plan(self) -> Optional[Dict[str, Any]]:
        connection = self._connection()
        try:
            row = connection.execute(
                "SELECT * FROM watering_plans WHERE state IN ('starting','running','stopping','stop_requested') ORDER BY updated_at LIMIT 1"
            ).fetchone()
            if row is None:
                return None
            plan = self._plan(row)
            attempt = self._active_attempt(connection, plan.get("active_attempt_id"))
            if attempt:
                plan["attempt"] = attempt
            return plan
        finally:
            connection.close()


__all__ = ["WateringPlanStore", "PlanStoreError", "PlanNotFound"]
