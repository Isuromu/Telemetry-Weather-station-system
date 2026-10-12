#!/usr/bin/env python3
"""Scheduler policy tests with a fake sequencer."""

import os
import tempfile
import unittest

from test_watering_plan_store import OPTIONS, payload, validate
from watering_plan_scheduler import WateringPlanScheduler
from watering_plan_store import WateringPlanStore


class FakeSequencer:
    def __init__(self):
        self.calls = []
        self.phase = "open_main"

    def start(self, plan):
        self.calls.append(("start", plan["resolved_valves"]))
        return {"phase": self.phase}

    def stop(self, plan):
        self.calls.append(("stop", plan["plan_id"]))
        self.phase = "stopped"
        return {"phase": "stopping"}

    def state(self, plan):
        return {"phase": self.phase}

    def fresh_state(self):
        return {"phase": self.phase}

    def safe_stop(self):
        self.calls.append(("safe_stop",))


class SchedulerTests(unittest.TestCase):
    def setUp(self):
        fd, self.path = tempfile.mkstemp(suffix=".sqlite3")
        os.close(fd)
        os.unlink(self.path)
        self.store = WateringPlanStore(self.path)
        self.now = "2026-10-12T10:01:00Z"
        self.seq = FakeSequencer()
        self.store.accept_plan(validate(payload("one")))
        self.monotonic = 1000
        self.meter = {"healthy": True, "fresh": True, "decoder": "commissioned",
                      "positive_total_m3": 10.0}

    def scheduler(self):
        return WateringPlanScheduler(
            self.store, self.seq, clock=lambda: self.now,
            monotonic_ms=lambda: self.monotonic,
            meter_status=lambda _plan: self.meter,
        )

    def tearDown(self):
        os.unlink(self.path)

    def test_unsynchronized_clock_delays_without_dispatch(self):
        scheduler = WateringPlanScheduler(self.store, self.seq,
                                          gates=lambda: {"clock_synchronized": False},
                                          clock=lambda: self.now,
                                          monotonic_ms=lambda: self.monotonic)
        result = scheduler.tick()
        self.assertEqual(result["action"], "delayed")
        self.assertEqual(self.store.get_plan("one")["state"], "delayed")
        self.assertEqual(self.seq.calls, [])

    def test_claim_dispatches_existing_sequencer(self):
        scheduler = self.scheduler()
        result = scheduler.tick()
        self.assertEqual(result["action"], "started")
        self.assertEqual(self.seq.calls[0][0], "start")
        self.assertEqual(self.store.get_plan("one")["state"], "starting")
        self.seq.phase = "running"
        self.assertEqual(scheduler.tick()["action"], "running")
        self.assertEqual(self.store.get_plan("one")["state"], "running")

    def test_active_cancel_uses_ordered_stop_then_completion(self):
        scheduler = self.scheduler()
        scheduler.tick()
        self.store.cancel_plan("one", self.now)
        self.assertEqual(scheduler.tick()["action"], "stop_dispatched")
        self.assertEqual(self.seq.calls[-1][0], "stop")
        self.assertEqual(scheduler.tick()["action"], "completed")
        self.assertEqual(self.store.get_plan("one")["state"], "completed")

    def test_duration_target_uses_persisted_monotonic_start(self):
        scheduler = self.scheduler()
        scheduler.tick()
        self.seq.phase = "running"
        self.assertEqual(scheduler.tick()["action"], "running")
        active = self.store.active_plan()
        self.assertEqual(active["attempt"]["monotonic_start_ms"], 1000)
        self.monotonic = 31_000
        self.assertEqual(scheduler.tick()["action"], "duration_target_reached_stop_dispatched")
        self.assertEqual(self.store.get_plan("one")["state"], "stopping")
        self.assertEqual(scheduler.tick()["action"], "completed")
        attempt = self.store.get_plan("one", include_history=True)["attempts"][0]
        self.assertEqual(attempt["actual_runtime_ms"], 30_000)
        self.assertEqual(attempt["final_reason"], "duration_target_reached")

    def test_volume_target_uses_commissioned_valve1_positive_total(self):
        self.store.cancel_plan("one")
        volume = payload("volume")
        volume["stop"] = {"type": "volume", "value": 0.04, "unit": "m3"}
        options = dict(OPTIONS)
        options["meterStatusByValve"] = {"valve1": {"healthy": True, "fresh": True, "decoder": "commissioned"}}
        self.store.accept_plan(validate(volume, options))
        scheduler = self.scheduler()
        scheduler.tick()
        self.seq.phase = "running"
        scheduler.tick()
        self.meter["positive_total_m3"] = 10.04
        self.assertEqual(scheduler.tick()["action"], "volume_target_reached_stop_dispatched")
        self.assertEqual(scheduler.tick()["action"], "completed")
        attempt = self.store.get_plan("volume", include_history=True)["attempts"][0]
        self.assertEqual(attempt["meter_baseline_m3"], 10.0)
        self.assertAlmostEqual(attempt["delivered_m3"], 0.04)

    def test_lost_meter_fails_only_after_ordered_stop(self):
        self.store.cancel_plan("one")
        volume = payload("meter-loss")
        volume["stop"] = {"type": "volume", "value": 1, "unit": "m3"}
        options = dict(OPTIONS)
        options["meterStatusByValve"] = {"valve1": {"healthy": True, "fresh": True, "decoder": "commissioned"}}
        self.store.accept_plan(validate(volume, options))
        scheduler = self.scheduler()
        scheduler.tick()
        self.seq.phase = "running"
        scheduler.tick()
        self.meter = {"healthy": False}
        self.assertEqual(scheduler.tick()["action"], "meter_unavailable_stop_dispatched")
        self.assertEqual(scheduler.tick()["action"], "failed")
        attempt = self.store.get_plan("meter-loss", include_history=True)["attempts"][0]
        self.assertEqual(attempt["final_reason"], "meter_unavailable")

    def test_restart_marks_active_run_for_recovery_and_only_calls_safe_stop(self):
        claimed = self.store.claim_next_due(self.now, "before-restart")
        self.assertIsNotNone(claimed)
        self.seq.phase = "running"
        scheduler = self.scheduler()
        self.assertEqual(scheduler.tick()["action"], "idle")
        self.assertIn(("safe_stop",), self.seq.calls)
        self.assertNotIn(("start", ["valve1"]), self.seq.calls)
        self.assertEqual(self.store.get_plan("one")["state"], "recovery_required")


if __name__ == "__main__":
    unittest.main()
