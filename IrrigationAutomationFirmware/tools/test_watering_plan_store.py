#!/usr/bin/env python3
"""Focused SQLite store tests; run with python3 tools/test_watering_plan_store.py."""

import json
import os
import subprocess
import tempfile
import threading
import unittest
from pathlib import Path

from watering_plan_store import PlanStoreError, WateringPlanStore


ROOT = Path(__file__).resolve().parents[1]
CLI = ROOT / "tools" / "watering_plan_contract_cli.js"
OPTIONS = {
    "clockSynchronized": True,
    "gatewayMaxRuntimeSec": 3600,
    "fields": [{"field_id": "field-001", "valve_id": "valve1", "enabled": True,
                "flow_meter": "commissioned"}],
    "allowedValveSets": [["valve1"]],
    "durationUnits": ["seconds"],
    "volumeUnits": ["m3"],
}


def payload(plan_id="plan-1", revision=1, start="2026-10-12T10:00:00Z", status="scheduled", until="2026-10-12T11:00:00Z"):
    return {
        "schema_version": 1, "plan_id": plan_id, "revision": revision, "status": status,
        "source": "human", "created_at": "2026-10-12T09:00:00Z", "timezone": "UTC",
        "start_at": start, "valid_until": until,
        "targets": {"mode": "selected", "field_ids": ["field-001"]},
        "run_mode": "parallel", "stop": {"type": "duration", "value": 30, "unit": "seconds"},
        "hard_max_runtime_sec": 300,
    }


def validate(value):
    result = subprocess.run(["node", str(CLI)], input=json.dumps({"payload": value, "options": OPTIONS}),
                            text=True, capture_output=True, check=True)
    outcome = json.loads(result.stdout)
    assert outcome["ok"], outcome
    return outcome


class StoreTests(unittest.TestCase):
    def setUp(self):
        fd, self.path = tempfile.mkstemp(suffix=".sqlite3")
        os.close(fd)
        os.unlink(self.path)
        self.store = WateringPlanStore(self.path)

    def tearDown(self):
        os.unlink(self.path)

    def test_duplicate_revision_rules_and_replacement(self):
        first = self.store.accept_plan(validate(payload()))
        self.assertTrue(first["created"])
        self.assertTrue(self.store.accept_plan(validate(payload()))["idempotent"])
        changed = payload()
        changed["stop"]["value"] = 31
        with self.assertRaisesRegex(PlanStoreError, "Same revision"):
            self.store.accept_plan(validate(changed))
        replacement = payload(revision=2)
        result = self.store.accept_plan(validate(replacement))
        self.assertTrue(result["replaced"])
        self.assertEqual(result["revision"], 2)
        with self.assertRaisesRegex(PlanStoreError, "older"):
            self.store.accept_plan(validate(payload(revision=1)))
        active = self.store.claim_next_due("2026-10-12T10:01:00Z", "one")
        self.assertIsNotNone(active)
        with self.assertRaisesRegex(PlanStoreError, "only scheduled"):
            self.store.accept_plan(validate(payload(revision=3)))

    def test_claim_is_ordered_and_competing_claims_are_exclusive(self):
        self.store.accept_plan(validate(payload("later", start="2026-10-12T10:02:00Z")))
        self.store.accept_plan(validate(payload("early", start="2026-10-12T10:01:00Z")))
        results = []
        barrier = threading.Barrier(3)
        def claim(owner):
            barrier.wait()
            results.append(self.store.claim_next_due("2026-10-12T10:03:00Z", owner))
        threads = [threading.Thread(target=claim, args=(str(i),)) for i in (1, 2)]
        for thread in threads:
            thread.start()
        barrier.wait()
        for thread in threads:
            thread.join()
        self.assertEqual(sum(result is not None for result in results), 1)
        self.assertEqual(results[0]["plan_id"] if results[0] else results[1]["plan_id"], "early")
        self.assertIsNone(self.store.claim_next_due("2026-10-12T10:03:00Z", "three"))

    def test_cancel_delay_expiry_and_restart_reconciliation(self):
        self.store.accept_plan(validate(payload("future", start="2026-10-12T12:00:00Z", until="2026-10-12T13:00:00Z")))
        self.assertEqual(self.store.cancel_plan("future")["state"], "cancelled")
        self.store.accept_plan(validate(payload("due")))
        self.assertEqual(self.store.mark_delayed("due", "pump_busy")["state"], "delayed")
        self.assertEqual(self.store.expire_due("2026-10-12T12:00:00Z"), 1)
        self.store.accept_plan(validate(payload("active", start="2026-10-12T13:00:00Z", until="2026-10-12T14:00:00Z")))
        claimed = self.store.claim_next_due("2026-10-12T13:01:00Z", "owner")
        self.assertIsNotNone(claimed)
        self.assertEqual(self.store.reconcile_active_attempts("2026-10-12T13:02:00Z"), 1)
        self.assertEqual(self.store.get_plan("active")["state"], "recovery_required")

    def test_collection_cursor(self):
        for plan_id, start in (("a", "2026-10-12T10:00:00Z"), ("b", "2026-10-12T10:01:00Z"), ("c", "2026-10-12T10:02:00Z")):
            self.store.accept_plan(validate(payload(plan_id, start=start)))
        first = self.store.list_plans(limit=2)
        self.assertEqual([p["plan_id"] for p in first["items"]], ["a", "b"])
        second = self.store.list_plans(limit=2, cursor=first["next_cursor"])
        self.assertEqual([p["plan_id"] for p in second["items"]], ["c"])


if __name__ == "__main__":
    unittest.main()
