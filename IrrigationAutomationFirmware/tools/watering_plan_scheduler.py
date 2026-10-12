"""Single-owner scheduler policy for watering-plan execution."""

from __future__ import annotations

from datetime import datetime, timezone
from typing import Any, Callable, Dict, Optional
from urllib.error import HTTPError
from urllib.request import Request, urlopen
import json
import math
import time
import uuid

try:
    from watering_plan_store import WateringPlanStore, PlanStoreError
except ImportError:  # pragma: no cover - package and script invocation
    from .watering_plan_store import WateringPlanStore, PlanStoreError


FAILED_STOP_REASONS = {
    "meter_unavailable",
    "meter_total_regressed",
    "monotonic_clock_regressed",
    "hard_runtime_limit",
    "unsupported_volume_target",
}


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
                 meter_status: Optional[Callable[[Dict[str, Any]], Dict[str, Any]]] = None,
                 monotonic_ms: Optional[Callable[[], int]] = None,
                 owner_id: str = "scheduler"):
        self.store = store
        self.sequencer = sequencer
        self.gates = gates or (lambda: {"clock_synchronized": True, "executor_ready": True})
        self.clock = clock or (lambda: datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"))
        # The meter provider is deliberately separate from the dashboard state.
        # It must expose the commissioned valve1 positive total, not an inferred
        # valve position or a simulated valve2 reading.
        self.meter_status = meter_status or (lambda _plan: {})
        self.monotonic_ms = monotonic_ms or (lambda: time.monotonic_ns() // 1_000_000)
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
            if claimed["stop_type"] == "volume":
                baseline, reason = self._commissioned_meter_total(claimed)
                if reason:
                    self.store.release_claim(claimed["plan_id"], reason, now)
                    return {"expired": expired, "action": "delayed", "reason": reason}
                self.store.set_meter_baseline(claimed["attempt"]["attempt_id"], baseline, now)
            response = self.sequencer.start(claimed)
            phase = self._phase(response)
            notice = self._notice(response)
            if phase in ("fault", "error"):
                self.store.transition_attempt(claimed["attempt"]["attempt_id"], "failed", now,
                                              reason="sequencer_fault", dashboard_phase=phase,
                                              dashboard_notice=notice)
                return {"expired": expired, "action": "failed", "phase": phase}
            if phase == "running":
                self._mark_running(claimed, now, phase, notice)
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

    @staticmethod
    def _notice(response: Any) -> Optional[str]:
        return response.get("notice") if isinstance(response, dict) else None

    def _mark_running(self, plan: Dict[str, Any], now: str, phase: str,
                      notice: Optional[str] = None) -> None:
        """Anchor target timing at dashboard-confirmed pump running, not dispatch."""
        attempt = plan.get("attempt") or {}
        attempt_id = attempt.get("attempt_id") or plan.get("active_attempt_id")
        if not attempt_id:
            raise PlanStoreError("attempt_not_found", "Active plan has no execution attempt")
        self.store.start_attempt_tracking(attempt_id, int(self.monotonic_ms()), now)
        self.store.transition_attempt(attempt_id, "running", now, dashboard_phase=phase,
                                      dashboard_notice=notice)

    @staticmethod
    def _duration_target_ms(plan: Dict[str, Any]) -> int:
        unit = str(plan["stop_unit"]).lower()
        multiplier = {"second": 1, "seconds": 1, "minute": 60, "minutes": 60,
                      "hour": 3600, "hours": 3600}.get(unit)
        if multiplier is None:
            raise PlanStoreError("unsupported_duration_unit", "No duration conversion is configured", [unit])
        return int(float(plan["stop_value"]) * multiplier * 1000)

    @staticmethod
    def _volume_target_m3(plan: Dict[str, Any]) -> float:
        unit = str(plan["stop_unit"]).lower()
        multiplier = {"m3": 1.0, "liter": 0.001, "liters": 0.001}.get(unit)
        if multiplier is None:
            raise PlanStoreError("unsupported_volume_unit", "No volume conversion is configured", [unit])
        return float(plan["stop_value"]) * multiplier

    def _commissioned_meter_total(self, plan: Dict[str, Any]) -> tuple[Optional[float], Optional[str]]:
        """Return only fresh, commissioned valve1 positive-total evidence."""
        if plan.get("resolved_valves") != ["valve1"]:
            return None, "unsupported_volume_target"
        try:
            meter = self.meter_status(plan) or {}
        except Exception:
            return None, "meter_unavailable"
        total = meter.get("positive_total_m3")
        if (meter.get("healthy") is not True or meter.get("fresh") is not True or
                meter.get("decoder") != "commissioned" or not isinstance(total, (int, float)) or
                isinstance(total, bool) or not math.isfinite(float(total)) or total < 0):
            return None, "meter_unavailable"
        return float(total), None

    def _stop_for_reason(self, plan: Dict[str, Any], now: str, phase: Optional[str],
                         reason: str, runtime_ms: Optional[int] = None,
                         delivered_m3: Optional[float] = None) -> str:
        """Always request the dashboard's ordered stop sequence; never command hardware here."""
        attempt_id = plan.get("active_attempt_id")
        if not attempt_id:
            raise PlanStoreError("attempt_not_found", "Active plan has no execution attempt")
        response = self.sequencer.stop(plan)
        self.store.transition_attempt(
            attempt_id, "stopping", now, reason=reason,
            dashboard_phase=self._phase(response) or phase,
            dashboard_notice=self._notice(response), actual_runtime_ms=runtime_ms,
            delivered_m3=delivered_m3,
        )
        return reason + "_stop_dispatched"

    def _reconcile_active(self, plan: Dict[str, Any], now: str) -> str:
        attempt_id = plan.get("active_attempt_id")
        if not attempt_id:
            return "recovery_required"
        try:
            if plan["state"] == "stop_requested":
                self._stop_for_reason(plan, now, None, "cancel_stop_dispatched")
                return "stop_dispatched"
            response = self.sequencer.state(plan)
            phase = self._phase(response)
            notice = self._notice(response)
            if phase in ("fault", "error"):
                self.store.transition_attempt(attempt_id, "failed", now,
                                              reason="sequencer_fault", dashboard_phase=phase,
                                              dashboard_notice=notice)
                return "failed"
            if phase in ("idle", "stopped", "complete", "completed"):
                if plan["state"] == "starting":
                    self.store.transition_attempt(attempt_id, "recovery_required", now,
                                                  reason="start_outcome_uncertain", dashboard_phase=phase,
                                                  dashboard_notice=notice)
                    return "recovery_required"
                stop_reason = (plan.get("attempt") or {}).get("final_reason")
                if stop_reason in FAILED_STOP_REASONS:
                    self.store.transition_attempt(attempt_id, "failed", now, dashboard_phase=phase,
                                                  dashboard_notice=notice)
                    return "failed"
                self.store.transition_attempt(attempt_id, "completed", now,
                                              reason="cancelled" if stop_reason == "cancel_stop_dispatched" else "sequence_completed",
                                              dashboard_phase=phase, dashboard_notice=notice)
                return "completed"
            if phase == "running" and plan["state"] == "starting":
                self._mark_running(plan, now, phase, notice)
                return "running"
            if plan["state"] == "running":
                return self._track_target(plan, now, phase, notice)
            return "observed"
        except Exception:
            self.store.transition_attempt(attempt_id, "recovery_required", now,
                                          reason="sequencer_observation_failed")
            return "recovery_required"

    def _track_target(self, plan: Dict[str, Any], now: str, phase: Optional[str],
                      notice: Optional[str]) -> str:
        attempt = plan.get("attempt") or {}
        start = attempt.get("monotonic_start_ms")
        if not isinstance(start, int):
            self.store.transition_attempt(plan["active_attempt_id"], "recovery_required", now,
                                          reason="tracking_start_missing", dashboard_phase=phase,
                                          dashboard_notice=notice)
            return "recovery_required"
        elapsed_ms = int(self.monotonic_ms()) - start
        if elapsed_ms < 0:
            return self._stop_for_reason(plan, now, phase, "monotonic_clock_regressed")
        delivered_m3 = None
        if plan["stop_type"] == "volume":
            total, reason = self._commissioned_meter_total(plan)
            if reason:
                return self._stop_for_reason(plan, now, phase, reason, elapsed_ms)
            baseline = attempt.get("meter_baseline_m3")
            if not isinstance(baseline, (int, float)):
                self.store.transition_attempt(plan["active_attempt_id"], "recovery_required", now,
                                              reason="meter_baseline_missing", dashboard_phase=phase,
                                              dashboard_notice=notice)
                return "recovery_required"
            delivered_m3 = total - float(baseline)
            if delivered_m3 < 0:
                return self._stop_for_reason(plan, now, phase, "meter_total_regressed", elapsed_ms)
            self.store.record_attempt_progress(plan["active_attempt_id"], elapsed_ms, delivered_m3)
            target_m3 = self._volume_target_m3(plan)
            # SQLite/Python floats can represent 10.04 - 10.0 just below 0.04.
            # This is numerical representation tolerance only, not a hydraulic
            # over-delivery allowance.
            if delivered_m3 >= target_m3 or math.isclose(delivered_m3, target_m3,
                                                          rel_tol=1e-12, abs_tol=1e-12):
                return self._stop_for_reason(plan, now, phase, "volume_target_reached", elapsed_ms, delivered_m3)
        else:
            self.store.record_attempt_progress(plan["active_attempt_id"], elapsed_ms)
            if elapsed_ms >= self._duration_target_ms(plan):
                return self._stop_for_reason(plan, now, phase, "duration_target_reached", elapsed_ms)
        if elapsed_ms >= int(plan["accepted_max_runtime_sec"]) * 1000:
            return self._stop_for_reason(plan, now, phase, "hard_runtime_limit", elapsed_ms, delivered_m3)
        return "observed"


__all__ = ["WateringPlanScheduler", "DashboardHttpSequencer"]
