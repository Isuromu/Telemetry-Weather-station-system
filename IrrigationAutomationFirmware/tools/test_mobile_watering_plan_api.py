#!/usr/bin/env python3
"""HTTP scenarios for mobile watering-plan requests; Python 3.8 stdlib only.

Default mode starts the deterministic mock. Use --live explicitly for a deployed
API; live mode only creates future plans and cancels them after assertions.
"""
import argparse
import datetime
import json
import threading
import unittest
import urllib.error
import urllib.parse
import urllib.request
import uuid

from mock_watering_plan_api import TOKEN, make_server


class ApiClient:
    def __init__(self, base_url, token, timeout=3):
        self.base = base_url.rstrip("/") + "/api/v1"
        self.token = token
        self.timeout = timeout

    def request(self, method, path, body=None, token=None, extra_headers=None):
        request_id = str(uuid.uuid4())
        headers = {"Accept": "application/json", "Content-Type": "application/json", "X-Request-ID": request_id}
        if token is None:
            token = self.token
        if token:
            headers["Authorization"] = "Bearer " + token
        if extra_headers:
            headers.update(extra_headers)
        data = None if body is None else json.dumps(body, separators=(",", ":")).encode("utf-8")
        request = urllib.request.Request(self.base + path, data=data, headers=headers, method=method)
        try:
            response = urllib.request.urlopen(request, timeout=self.timeout)
            status = response.status
            raw = response.read()
        except urllib.error.HTTPError as error:
            status = error.code
            raw = error.read()
        value = json.loads(raw.decode("utf-8"))
        return status, value, request_id


def new_plan(prefix="test"):
    now = datetime.datetime.now(datetime.timezone.utc).replace(microsecond=0)
    start = now + datetime.timedelta(days=2)
    valid = start + datetime.timedelta(hours=10)
    return {
        "schema_version": 1,
        "plan_id": prefix + "-" + str(uuid.uuid4()),
        "revision": 1,
        "status": "scheduled",
        "source": "human",
        "created_at": now.isoformat().replace("+00:00", "Z"),
        "timezone": "UTC",
        "start_at": start.isoformat().replace("+00:00", "Z"),
        "valid_until": valid.isoformat().replace("+00:00", "Z"),
        "targets": {"mode": "selected", "field_ids": ["field-001"]},
        "run_mode": "parallel",
        "stop": {"type": "duration", "value": 60, "unit": "seconds"},
        "hard_max_runtime_sec": 3600,
    }


class ScenarioTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.server = None
        cls.thread = None
        if not ARGS.live:
            cls.server = make_server("127.0.0.1", 0)
            cls.thread = threading.Thread(target=cls.server.serve_forever, daemon=True)
            cls.thread.start()
            base = "http://127.0.0.1:%d" % cls.server.server_address[1]
            token = TOKEN
        else:
            base = ARGS.base_url
            token = ARGS.token
        cls.client = ApiClient(base, token, ARGS.timeout)
        cls.created_ids = set()

    @classmethod
    def tearDownClass(cls):
        if ARGS.live:
            for plan_id in cls.created_ids:
                try:
                    cls.client.request("POST", "/watering-plans/" + urllib.parse.quote(plan_id, safe="") + "/cancel", {})
                except Exception:
                    print("WARNING: could not cancel test plan %s" % plan_id)
        if cls.server:
            cls.server.shutdown()
            cls.server.server_close()
            cls.thread.join(timeout=2)

    def assert_envelope(self, response, status, request_id=None):
        self.assertEqual(response[0], status, response[1])
        self.assertEqual(response[1].get("api_version"), "v1")
        self.assertTrue(response[1].get("request_id"))
        if request_id:
            self.assertEqual(response[1]["request_id"], request_id)

    def put(self, plan, **kwargs):
        result = self.client.request("PUT", "/watering-plans/" + urllib.parse.quote(plan["plan_id"], safe=""), plan, **kwargs)
        if ARGS.live and result[0] in (200, 201):
            self.created_ids.add(plan["plan_id"])
        return result

    def test_health_device_state_and_auth(self):
        status, value, rid = self.client.request("GET", "/health")
        self.assert_envelope((status, value), 200, rid)
        self.assertIn("clock_synchronized", value["data"])
        status, value, rid = self.client.request("GET", "/device-state")
        self.assert_envelope((status, value), 200, rid)
        self.assertIn("generated_at", value["data"])
        for bad_token in ("", "wrong-token"):
            status, value, rid = self.client.request("GET", "/health", token=bad_token)
            self.assert_envelope((status, value), 401, rid)
            self.assertEqual(value["error"]["code"], "unauthorized")
            self.assertNotIn("test-token", json.dumps(value))

    def test_create_duplicate_changed_and_stale_revisions(self):
        plan = new_plan()
        first = self.put(plan)
        self.assert_envelope(first, 201)
        self.assertEqual(first[1]["data"]["plan_id"], plan["plan_id"])
        duplicate = self.put(plan)
        self.assert_envelope(duplicate, 200)
        self.assertEqual(duplicate[1]["data"]["revision"], 1)
        changed = dict(plan, summary="changed without revision")
        conflict = self.put(changed)
        self.assert_envelope(conflict, 409)
        self.assertEqual(conflict[1]["error"]["code"], "revision_conflict")
        revision2 = dict(plan, revision=2, summary="approved edit")
        updated = self.put(revision2)
        self.assert_envelope(updated, 200)
        self.assertEqual(updated[1]["data"]["revision"], 2)
        stale = self.put(plan)
        self.assert_envelope(stale, 409)

    def test_timeout_after_store_retry_same_identity(self):
        if ARGS.live:
            self.skipTest("mock-only: live endpoint has no forced lost-response hook")
        plan = new_plan("retry")
        path = "/watering-plans/" + urllib.parse.quote(plan["plan_id"], safe="")
        first_id = str(uuid.uuid4())
        headers = {"X-Mock-Scenario": "drop-after-store", "X-Request-ID": first_id}
        try:
            self.client.request("PUT", path, plan, extra_headers=headers)
        except (urllib.error.URLError, ConnectionError, OSError):
            pass
        else:
            # Some HTTP stacks expose a clean close as an empty/invalid response.
            pass
        retry_status, retry_body, retry_id = self.client.request("PUT", path, plan)
        self.assert_envelope((retry_status, retry_body), 200, retry_id)
        self.assertNotEqual(first_id, retry_id)
        self.assertEqual(retry_body["data"]["plan_id"], plan["plan_id"])
        self.assertEqual(retry_body["data"]["revision"], plan["revision"])

    def test_validation_and_path_mismatch(self):
        plan = new_plan("invalid")
        plan["schema_version"] = 99
        result = self.put(plan)
        self.assert_envelope(result, 422)
        plan = new_plan("mismatch")
        wrong_path = "/watering-plans/not-the-payload-id"
        result = self.client.request("PUT", wrong_path, plan)
        self.assert_envelope(result, 400)
        self.assertEqual(result[1]["error"]["code"], "plan_id_mismatch")
        plan = new_plan("target")
        plan["targets"] = {"mode": "selected", "field_ids": ["unknown-field"]}
        self.assert_envelope(self.put(plan), 422)
        plan = new_plan("time")
        plan["start_at"] = "2030-01-01T00:00:00"
        self.assert_envelope(self.put(plan), 422)

    def test_collection_detail_cancel_and_missing(self):
        first, second = new_plan("list"), new_plan("list")
        self.assert_envelope(self.put(first), 201)
        self.assert_envelope(self.put(second), 201)
        path = "/watering-plans?limit=1"
        page1 = self.client.request("GET", path)
        self.assert_envelope(page1, 200)
        self.assertEqual(len(page1[1]["data"]["items"]), 1)
        plan_id = page1[1]["data"]["items"][0]["plan_id"]
        detail = self.client.request("GET", "/watering-plans/" + urllib.parse.quote(plan_id, safe=""))
        self.assert_envelope(detail, 200)
        self.assertEqual(detail[1]["data"]["plan_id"], plan_id)
        cursor = page1[1]["data"].get("next_cursor")
        if cursor:
            page2 = self.client.request("GET", "/watering-plans?limit=1&cursor=" + urllib.parse.quote(cursor, safe=""))
            self.assert_envelope(page2, 200)
        cancelled = self.client.request("POST", "/watering-plans/" + urllib.parse.quote(second["plan_id"], safe="") + "/cancel", {})
        self.assert_envelope(cancelled, 200)
        self.assertEqual(cancelled[1]["data"]["state"], "cancelled")
        self.assert_envelope(self.client.request("GET", "/watering-plans/not-found"), 404)
        self.assert_envelope(self.client.request("POST", "/watering-plans/not-found/cancel", {}), 404)

    def test_mock_error_envelope_and_active_cancel_semantics(self):
        if ARGS.live:
            self.skipTest("mock-only: does not actuate or simulate real scheduler")
        plan = new_plan("active")
        self.assert_envelope(self.put(plan), 201)
        with self.server.state.lock:
            self.server.state.plans[plan["plan_id"]]["state"] = "running"
        result = self.client.request("POST", "/watering-plans/" + plan["plan_id"] + "/cancel", {})
        self.assert_envelope(result, 202)
        self.assertEqual(result[1]["data"]["state"], "stop_requested")
        self.assertNotEqual(result[1]["data"]["state"], "stopped")
        failed = self.put(new_plan("error"), extra_headers={"X-Mock-Scenario": "internal-error"})
        self.assert_envelope(failed, 500)
        self.assertIn("error", failed[1])


def main():
    global ARGS
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--live", action="store_true", help="target deployed API; creates and cancels unique future plans")
    parser.add_argument("--base-url", help="deployed Node-RED API base URL, without /api/v1")
    parser.add_argument("--token", help="Bearer token; prefer IRRIGATION_PLAN_API_TOKEN")
    parser.add_argument("--timeout", type=float, default=3.0)
    ARGS = parser.parse_args()
    if ARGS.live:
        import os
        ARGS.base_url = ARGS.base_url or os.environ.get("IRRIGATION_PLAN_API_URL")
        ARGS.token = ARGS.token or os.environ.get("IRRIGATION_PLAN_API_TOKEN")
        if not ARGS.base_url or not ARGS.token:
            parser.error("--live requires --base-url and --token, or corresponding IRRIGATION_PLAN_API_* env vars")
        if not ARGS.base_url.lower().startswith("https://"):
            print("WARNING: bearer token will traverse HTTP in cleartext; use isolated trusted LAN only")
    else:
        ARGS.base_url, ARGS.token = None, None
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(ScenarioTests)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    if not result.wasSuccessful():
        raise SystemExit(1)
    print("PASS: %d scenario tests; mode=%s" % (result.testsRun, "live" if ARGS.live else "mock"))


if __name__ == "__main__":
    main()
