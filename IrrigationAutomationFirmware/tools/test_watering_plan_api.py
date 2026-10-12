#!/usr/bin/env python3
"""HTTP service tests without opening a network socket."""

import json
import os
import tempfile
import unittest

from test_watering_plan_store import payload
from watering_plan_api import WateringPlanService
from watering_plan_store import WateringPlanStore


class ApiTests(unittest.TestCase):
    def setUp(self):
        fd, self.path = tempfile.mkstemp(suffix=".sqlite3")
        os.close(fd)
        os.unlink(self.path)
        self.store = WateringPlanStore(self.path)
        self.service = WateringPlanService(self.store, "test-token")
        self.headers = {"Authorization": "Bearer test-token"}

    def tearDown(self):
        os.unlink(self.path)

    def call(self, method, path, body=None, headers=None):
        request_headers = self.headers if headers is None else headers
        return self.service.handle(method, path, request_headers,
                                   b"" if body is None else json.dumps(body).encode())

    def test_auth_envelope_and_idempotent_put(self):
        status, body = self.call("GET", "/api/v1/health", headers={})
        self.assertEqual(status, 401)
        self.assertEqual(body["error"]["code"], "unauthorized")
        status, body = self.call("PUT", "/api/v1/watering-plans/plan-api", payload("plan-api"))
        self.assertEqual(status, 201)
        self.assertEqual(body["api_version"], "v1")
        self.assertIn("request_id", body)
        status, body = self.call("PUT", "/api/v1/watering-plans/plan-api", payload("plan-api"))
        self.assertEqual(status, 200)
        self.assertTrue(body["data"]["idempotent"])

    def test_reads_cancel_and_validation_status(self):
        status, _ = self.call("PUT", "/api/v1/watering-plans/plan-api", payload("plan-api"))
        self.assertEqual(status, 201)
        status, body = self.call("GET", "/api/v1/watering-plans/plan-api")
        self.assertEqual(status, 200)
        self.assertEqual(body["data"]["plan_id"], "plan-api")
        status, body = self.call("GET", "/api/v1/watering-plans")
        self.assertEqual(status, 200)
        self.assertEqual(len(body["data"]["items"]), 1)
        status, body = self.call("PUT", "/api/v1/watering-plans/wrong", payload())
        self.assertEqual(status, 422)
        self.assertEqual(body["error"]["code"], "plan_id_mismatch")
        status, body = self.call("POST", "/api/v1/watering-plans/plan-api/cancel")
        self.assertEqual(status, 200)
        self.assertEqual(body["data"]["state"], "cancelled")

    def test_active_cancel_returns_accepted_stop(self):
        self.call("PUT", "/api/v1/watering-plans/plan-api", payload("plan-api"))
        self.assertIsNotNone(self.store.claim_next_due("2026-10-12T10:01:00Z", "test"))
        status, body = self.call("POST", "/api/v1/watering-plans/plan-api/cancel")
        self.assertEqual(status, 202)
        self.assertEqual(body["data"]["state"], "stop_requested")


if __name__ == "__main__":
    unittest.main()
