"""Reference fake backend for mobile watering-plan testing (TEMPORARY).

This is a harness, not a rule engine. It wires the real store, scheduler and
service together with a virtual clock and a simulated sequencer, so the mobile
app can drive a complete plan lifecycle without Node-RED, a Raspberry Pi, or
waiting in real time. Every rule it applies comes from
``docs/integrations/watering-plan/api-protocol.rules.json`` (transport and
lifecycle) and ``tools/watering_plan_contract.js`` (payload acceptance), so a
mobile test exercises the same behaviour the real backend must implement.

Temporary: this file and ``tools/mock_watering_plan_api.py`` are test
scaffolding. The portable artifacts are the rules file, ``API_PROTOCOL_SPEC.md``
and ``tools/test_watering_plan_backend.py``.
"""

from __future__ import annotations

import os
import shutil
import tempfile
from datetime import datetime, timedelta, timezone
from typing import Any, Dict, List, Optional

try:
    from watering_plan_api import WateringPlanService
    from watering_plan_rules import load_rules
    from watering_plan_scheduler import WateringPlanScheduler
    from watering_plan_store import WateringPlanStore
except ImportError:  # pragma: no cover - package invocation
    from .watering_plan_api import WateringPlanService
    from .watering_plan_rules import load_rules
    from .watering_plan_scheduler import WateringPlanScheduler
    from .watering_plan_store import WateringPlanStore

ISO = "%Y-%m-%dT%H:%M:%SZ"

# The prototype's one commissioned, metered field. A plan may target it with a
# duration or a volume stop rule; the meter is declared commissioned at
# acceptance and simulated at run time.
DEFAULT_VALIDATOR_OPTIONS: Dict[str, Any] = {
    "clockSynchronized": True,
    "gatewayMaxRuntimeSec": 3600,
    "fields": [{"field_id": "field-001", "display_name": "Field 1", "valve_id": "valve1",
                "enabled": True, "flow_meter": "commissioned"}],
    "allowedValveSets": [["valve1"]],
    "durationUnits": ["seconds", "minutes", "hours"],
    "volumeUnits": ["m3", "liters"],
    "meterStatusByValve": {"valve1": {"healthy": True, "fresh": True, "decoder": "commissioned"}},
}


class VirtualClock:
    """Deterministic clock. Wall time and monotonic time move independently.

    By default it starts at the real current time so a plan authored with a real
    ``start_at`` is due once the server advances; pass ``base_epoch`` for a fixed
    base when a test wants one.
    """

    def __init__(self, base_epoch: Optional[str] = None):
        if base_epoch is None:
            self._start = datetime.now(timezone.utc).replace(microsecond=0)
        else:
            self._start = datetime.strptime(base_epoch, ISO).replace(tzinfo=timezone.utc)
        self._offset = 0.0
        self._monotonic_ms = 0

    def now(self) -> str:
        return (self._start + timedelta(seconds=self._offset)).strftime(ISO)

    def now_plus(self, seconds: float) -> str:
        return (self._start + timedelta(seconds=self._offset + seconds)).strftime(ISO)

    def monotonic_ms(self) -> int:
        return self._monotonic_ms

    def advance(self, seconds: float) -> None:
        self._offset += seconds
        self._monotonic_ms += int(seconds * 1000)

    def regress_wall(self, seconds: float) -> None:
        self._offset -= seconds

    def regress_monotonic(self, milliseconds: int) -> None:
        self._monotonic_ms -= milliseconds


class SimulatedSequencer:
    """Duck-typed stand-in for the Node-RED sequencer seam.

    It reports the dashboard phases the scheduler reconciles against, and offers
    a growing valve1 positive total so a volume plan has real meter evidence.
    """

    def __init__(self, clock: VirtualClock, meter_start_m3: float = 10.0,
                 flow_m3_per_sec: float = 0.001, teardown_seconds: float = 0):
        self.clock = clock
        self.meter_start_m3 = meter_start_m3
        self.flow_m3_per_sec = flow_m3_per_sec
        self.teardown_seconds = teardown_seconds
        self.running = False
        self.running_since_ms: Optional[int] = None
        self.stop_requested_at_ms: Optional[int] = None
        self.fault_phase: Optional[str] = None
        self.meter_unavailable = False
        self.meter_total_override: Optional[float] = None
        self.calls: List[tuple] = []
        self.safe_stops = 0

    def _elapsed_ms(self) -> int:
        if self.running_since_ms is None:
            return 0
        return max(0, self.clock.monotonic_ms() - self.running_since_ms)

    def start(self, plan: Dict[str, Any]) -> Dict[str, Any]:
        self.calls.append(("start", plan.get("plan_id")))
        if self.fault_phase:
            return {"phase": self.fault_phase}
        self.running = True
        self.running_since_ms = self.clock.monotonic_ms()
        self.stop_requested_at_ms = None
        return {"phase": "running"}

    def stop(self, plan: Dict[str, Any]) -> Dict[str, Any]:
        self.calls.append(("stop", plan.get("plan_id")))
        self.stop_requested_at_ms = self.clock.monotonic_ms()
        return {"phase": "stopping"}

    def state(self, plan: Dict[str, Any]) -> Dict[str, Any]:
        if self.fault_phase:
            return {"phase": self.fault_phase}
        if not self.running:
            return {"phase": "stopped"}
        if self.stop_requested_at_ms is not None:
            waited = self.clock.monotonic_ms() - self.stop_requested_at_ms
            if waited >= self.teardown_seconds * 1000:
                self.running = False
                return {"phase": "stopped"}
            return {"phase": "stopping"}
        return {"phase": "running"}

    def fresh_state(self) -> Dict[str, Any]:
        if self.fault_phase:
            return {"phase": self.fault_phase}
        return {"phase": "running" if self.running else "stopped"}

    def safe_stop(self) -> Dict[str, Any]:
        self.safe_stops += 1
        self.calls.append(("safe_stop", None))
        self.running = False
        self.stop_requested_at_ms = None
        return {"phase": "stopped"}

    def meter_status(self, plan: Dict[str, Any]) -> Dict[str, Any]:
        total = self.meter_start_m3 + self.flow_m3_per_sec * (self._elapsed_ms() / 1000.0)
        if self.meter_total_override is not None:
            total = self.meter_total_override
        if self.meter_unavailable:
            return {"healthy": False, "fresh": False, "decoder": "commissioned", "positive_total_m3": total}
        return {"healthy": True, "fresh": True, "decoder": "commissioned", "positive_total_m3": total}


class FakeBackend:
    """The reference service plus the control surface tests use to drive it."""

    def __init__(self, token: str = "test-token", base_epoch: Optional[str] = None,
                 meter_start_m3: float = 10.0, flow_m3_per_sec: float = 0.001,
                 validator_options: Optional[Dict[str, Any]] = None, database: Optional[str] = None):
        self.clock = VirtualClock(base_epoch)
        self._directory = None
        if database is None:
            self._directory = tempfile.mkdtemp(prefix="watering-fake-")
            database = os.path.join(self._directory, "plans.sqlite3")
        self.store = WateringPlanStore(database)
        self.sequencer = SimulatedSequencer(self.clock, meter_start_m3, flow_m3_per_sec)
        self._gate_reason: Optional[str] = None
        self._clock_synchronized = True
        self.scheduler = WateringPlanScheduler(
            self.store, self.sequencer,
            gates=self._gates, clock=self.clock.now,
            meter_status=self.sequencer.meter_status,
            monotonic_ms=self.clock.monotonic_ms, owner_id="fake")
        self.service = WateringPlanService(
            self.store, token,
            validator_options=validator_options or dict(DEFAULT_VALIDATOR_OPTIONS),
            scheduler=self.scheduler, device_state=self.device_state)
        self.ticks: List[Dict[str, Any]] = []

    # --- scheduler control ---------------------------------------------------
    def _gates(self) -> Dict[str, Any]:
        return {"clock_synchronized": self._clock_synchronized,
                "executor_ready": self._gate_reason is None,
                "reason": self._gate_reason}

    def device_state(self) -> Dict[str, Any]:
        return {"generated_at": self.clock.now(),
                "status": "available",
                "phase": "running" if self.sequencer.running else "idle",
                "devices": []}

    def set_gate(self, reason: str, clock_synchronized: bool = True) -> None:
        self._gate_reason = reason
        self._clock_synchronized = clock_synchronized

    def clear_gate(self) -> None:
        self._gate_reason = None
        self._clock_synchronized = True

    def tick(self) -> Dict[str, Any]:
        result = self.scheduler.tick()
        self.ticks.append(result)
        return result

    def advance(self, seconds: float, step: float = 1) -> List[Dict[str, Any]]:
        results = []
        remaining = float(seconds)
        while remaining > 0:
            delta = min(step, remaining)
            self.clock.advance(delta)
            results.append(self.tick())
            remaining -= delta
        return results

    def restart(self) -> Dict[str, Any]:
        # A process restart: the store keeps plan state, the scheduler must
        # re-reconcile on its next tick, and the sequencer still reports running.
        self.scheduler.started = False
        return self.tick()

    def run_until(self, plan_id: str, terminal: Optional[set] = None, max_seconds: int = 900) -> str:
        terminal = terminal or set(load_rules()["terminal_plan_states"])
        state = self.store.get_plan(plan_id)["state"]
        for _ in range(max_seconds):
            if state in terminal:
                return state
            self.advance(1)
            state = self.store.get_plan(plan_id)["state"]
        return state

    # --- fault injection -----------------------------------------------------
    def inject_meter_unavailable(self, unavailable: bool = True) -> None:
        self.sequencer.meter_unavailable = unavailable

    def inject_meter_total(self, total_m3: Optional[float]) -> None:
        self.sequencer.meter_total_override = total_m3

    def inject_sequencer_fault(self, phase: str = "fault") -> None:
        self.sequencer.fault_phase = phase

    def clear_faults(self) -> None:
        self.sequencer.meter_unavailable = False
        self.sequencer.meter_total_override = None
        self.sequencer.fault_phase = None

    def close(self) -> None:
        if self._directory:
            shutil.rmtree(self._directory, ignore_errors=True)
            self._directory = None


__all__ = ["VirtualClock", "SimulatedSequencer", "FakeBackend", "DEFAULT_VALIDATOR_OPTIONS"]
