#!/usr/bin/env python3
"""Scheduler policy tests with a fake sequencer."""

import os
import tempfile
import unittest

from test_watering_plan_store import payload, validate
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

    def tearDown(self):
        os.unlink(self.path)

    def test_unsynchronized_clock_delays_without_dispatch(self):
        scheduler = WateringPlanScheduler(self.store, self.seq,
                                          gates=lambda: {"clock_synchronized": False},
                                          clock=lambda: self.now)
        result = scheduler.tick()
        self.assertEqual(result["action"], "delayed")
        self.assertEqual(self.store.get_plan("one")["state"], "delayed")
        self.assertEqual(self.seq.calls, [])

    def test_claim_dispatches_existing_sequencer(self):
        scheduler = WateringPlanScheduler(self.store, self.seq, clock=lambda: self.now)
        result = scheduler.tick()
        self.assertEqual(result["action"], "started")
        self.assertEqual(self.seq.calls[0][0], "start")
        self.assertEqual(self.store.get_plan("one")["state"], "starting")
        self.seq.phase = "running"
        self.assertEqual(scheduler.tick()["action"], "running")
        self.assertEqual(self.store.get_plan("one")["state"], "running")

    def test_active_cancel_uses_ordered_stop_then_completion(self):
        scheduler = WateringPlanScheduler(self.store, self.seq, clock=lambda: self.now)
        scheduler.tick()
        self.store.cancel_plan("one", self.now)
        self.assertEqual(scheduler.tick()["action"], "stop_dispatched")
        self.assertEqual(self.seq.calls[-1][0], "stop")
        self.assertEqual(scheduler.tick()["action"], "completed")
        self.assertEqual(self.store.get_plan("one")["state"], "completed")


if __name__ == "__main__":
    unittest.main()
