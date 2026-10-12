"""Single-owner scheduler policy for watering-plan execution."""

from __future__ import annotations

from datetime import datetime, timedelta, timezone
from typing import Any, Callable, Dict, Optional
from urllib.error import HTTPError
from urllib.request import Request, urlopen
import json
import uuid

try:
    from watering_plan_store import WateringPlanStore, PlanStoreError
except ImportError:  # pragma: no cover - package and script invocation
    from .watering_plan_store import WateringPlanStore, PlanStoreError


ACTIVE_STATES = {"starting", "running", "stopping", "stop_requested"}


def parse_time(value: str) -> datetime:
    return datetime.fromisoformat(value.replace("Z", "+00:00")).astimezone(timezone.utc)


class DashboardHttpSequencer:
    """Call only generated internal HTTP seam; never publish device commands."""

    def __init__(self, base_url: str, token: str, timeout: float = 5.0):
        self.base_url = base_url.rstrip("/")
        self.token = token
        self.timeout = timeout

    def _request(self, method: str, path: str, body: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
        data = None if body is None else json.dumps(body).encode("utf-8")
        request = Request(self.base_url + path, data=data, method=method,
                          headers={"Authorization": "Bearer " + self.token,
                                   "Content-Type": "application/json"})
        try:
            with urlopen(request, timeout=self.timeout) as response:
                payload = json.loads(response.read().decode("utf-8"))
        except HTTPError as error:
            raise RuntimeError("sequencer HTTP status %s" % error.code) from error
        if "error" in payload:
            raise RuntimeError(payload["error"].get("code", "sequencer_error"))
        return payload.get("data", payload)

    @staticmethod
    def _state(response: Dict[str, Any]) -> Dict[str, Any]:
        return response.get("state", response) if isinstance(response, dict) else {}

    def start(self, plan: Dict[str, Any]) -> Dict[str, Any]:
        attempt = plan.get("attempt", {})
        response = self._request("POST", "/internal/irrigation/command", {
            "operation_id": attempt.get("operation_id") or uuid.uuid4().hex,
            "action": "start",
            "valves": {valve: True for valve in plan.get("resolved_valves", [])},
        })
        return self._state(response)

    def stop(self, plan: Dict[str, Any]) -> Dict[str, Any]:
        attempt = plan.get("attempt", {})
        response = self._request("POST", "/internal/irrigation/command", {
            "operation_id": (attempt.get("operation_id") or uuid.uuid4().hex) + ":stop",
            "action": "stop",
        })
        return self._state(response)

    def state(self, plan: Dict[str, Any]) -> Dict[str, Any]:
        return self._state(self._request("GET", "/internal/irrigation/state"))

    def fresh_state(self) -> Dict[str, Any]:
        return self.state({})

    def safe_stop(self) -> Dict[str, Any]:
        return self._state(self._request("POST", "/internal/irrigation/command", {
            "operation_id": "recovery:" + uuid.uuid4().hex, "action": "stop",
        }))


class WateringPlanScheduler:
    def __init__(self, store: WateringPlanStore, sequencer: Any,
                 gates: Optional[Callable[[], Dict[str, Any]]] = None,
                 clock: Optional[Callable[[], str]] = None,
                 owner_id: str = "scheduler"):
        self.store = store
        self.sequencer = sequencer
        self.gates = gates or (lambda: {"clock_synchronized": True, "executor_ready": True})
        self.clock = clock or (lambda: datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"))
        self.owner_id = owner_id
        self.started = False

    def startup_reconcile(self) -> int:
        """Never resume active runs; safe-stop only after fresh state observation."""
        count = self.store.reconcile_active_attempts(self.clock())
        if count and hasattr(self.sequencer, "fresh_state") and hasattr(self.sequencer, "safe_stop"):
            try:
                phase = self._phase(self.sequencer.fresh_state())
                if phase and phase not in ("idle", "stopped", "complete", "completed"):
                    self.sequencer.safe_stop()
            except Exception:
                pass
        self.started = True
        return count

    def tick(self) -> Dict[str, Any]:
        now = self.clock()
        if not self.started:
            self.startup_reconcile()
        expired = self.store.expire_due(now)
        active = self.store.active_plan()
        if active:
            return {"expired": expired, "action": self._reconcile_active(active, now)}

        claimed = self.store.claim_next_due(now, self.owner_id)
        if claimed is None:
            return {"expired": expired, "action": "idle"}
        gates = self.gates() or {}
        reason = self._gate_reason(gates)
        if reason:
            self.store.release_claim(claimed["plan_id"], reason, now)
            return {"expired": expired, "action": "delayed", "reason": reason}
        try:
            response = self.sequencer.start(claimed)
            phase = self._phase(response)
            if phase in ("fault", "error"):
                self.store.transition_attempt(claimed["attempt"]["attempt_id"], "failed", now,
                                              reason="sequencer_fault", dashboard_phase=phase)
                return {"expired": expired, "action": "failed", "phase": phase}
            if phase == "running":
                self.store.transition_attempt(claimed["attempt"]["attempt_id"], "running", now,
                                              dashboard_phase=phase)
            return {"expired": expired, "action": "started", "phase": phase or "starting"}
        except Exception as error:
            self.store.transition_attempt(claimed["attempt"]["attempt_id"], "recovery_required", now,
                                          reason="sequencer_dispatch_uncertain", dashboard_notice=str(error))
            return {"expired": expired, "action": "recovery_required", "reason": "sequencer_dispatch_uncertain"}

    @staticmethod
    def _gate_reason(gates: Dict[str, Any]) -> Optional[str]:
        if gates.get("clock_synchronized") is not True:
            return "clock_unsynchronized"
        if gates.get("executor_ready") is not True:
            return str(gates.get("reason") or "executor_gate_failed")
        return None

    @staticmethod
    def _phase(response: Any) -> Optional[str]:
        if isinstance(response, str):
            return response
        if isinstance(response, dict):
            return response.get("phase") or response.get("state")
        return None

    def _reconcile_active(self, plan: Dict[str, Any], now: str) -> str:
        attempt_id = plan.get("active_attempt_id")
        if not attempt_id:
            return "recovery_required"
        try:
            if plan["state"] == "stop_requested":
                response = self.sequencer.stop(plan)
                phase = self._phase(response) or "stopping"
                self.store.transition_attempt(attempt_id, "stopping", now,
                                              reason="cancel_stop_dispatched", dashboard_phase=phase)
                return "stop_dispatched"
            response = self.sequencer.state(plan)
            phase = self._phase(response)
            if phase in ("fault", "error"):
                self.store.transition_attempt(attempt_id, "failed", now,
                                              reason="sequencer_fault", dashboard_phase=phase)
                return "failed"
            if phase in ("idle", "stopped", "complete", "completed"):
                if plan["state"] == "starting":
                    self.store.transition_attempt(attempt_id, "recovery_required", now,
                                                  reason="start_outcome_uncertain", dashboard_phase=phase)
                    return "recovery_required"
                self.store.transition_attempt(attempt_id, "completed", now,
                                              reason="cancelled" if plan["state"] == "stopping" else "sequence_completed",
                                              dashboard_phase=phase)
                return "completed"
            if phase == "running" and plan["state"] == "starting":
                self.store.transition_attempt(attempt_id, "running", now, dashboard_phase=phase)
                return "running"
            if plan["state"] == "running":
                claimed = parse_time(plan["updated_at"])
                if parse_time(now) - claimed >= timedelta(seconds=plan["accepted_max_runtime_sec"]):
                    self.sequencer.stop(plan)
                    self.store.transition_attempt(attempt_id, "stopping", now,
                                                  reason="hard_runtime_limit", dashboard_phase=phase)
                    return "hard_stop_dispatched"
            return "observed"
        except Exception:
            self.store.transition_attempt(attempt_id, "recovery_required", now,
                                          reason="sequencer_observation_failed")
            return "recovery_required"


__all__ = ["WateringPlanScheduler", "DashboardHttpSequencer"]
