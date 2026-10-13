#!/usr/bin/env python3
"""Conformance suite for the watering-plan v1 protocol and lifecycle.

It reads the language-neutral rules (docs/integrations/watering-plan/
api-protocol.rules.json and conformance-scenarios.json), drives the reference
fake backend through them, and asserts the results match the tables. A real
backend port is validated by running this same suite against it.

    python3 tools/test_watering_plan_backend.py
"""

import copy
import json
import subprocess
import unittest
from pathlib import Path

from fake_watering_plan_backend import DEFAULT_VALIDATOR_OPTIONS, FakeBackend
from watering_plan_rules import RULES_PATH, load_rules
from watering_plan_scheduler import FAILED_STOP_REASONS

ROOT = Path(__file__).resolve().parents[1]
SCENARIOS_PATH = ROOT / "docs/integrations" / "watering-plan" / "conformance-scenarios.json"
CONTRACT = ROOT / "tools" / "watering_plan_contract.js"
TOKEN = "test-token"
AUTH = {"Authorization": "Bearer " + TOKEN}

# Codes the transport cannot surface for a well-formed request: they are raised
# by the store on a corrupt attempt, or are the catch-all. Everything else in
# error_status must be reached by a real request below.
INTERNAL_ONLY_CODES = {"invalid_plan_id", "attempt_not_found", "invalid_state", "invalid_tracking", "internal_error"}

RULES = load_rules()
SCENARIOS = json.loads(SCENARIOS_PATH.read_text(encoding="utf-8"))


def contract_error_codes():
    script = "process.stdout.write(JSON.stringify(require(%s).ERROR_CODES))" % json.dumps(str(CONTRACT))
    result = subprocess.run(["node", "-e", script], capture_output=True, text=True, check=True)
    return json.loads(result.stdout)


def plan_for(backend, plan_id, spec=None, **overrides):
    plan = copy.deepcopy(SCENARIOS["plan_defaults"])
    spec = dict(spec or {})
    start_offset = spec.pop("start_offset", 0)
    valid_offset = spec.pop("valid_offset", 36000)
    plan.update(spec)
    plan["plan_id"] = plan_id
    plan["created_at"] = backend.clock.now()
    plan["start_at"] = backend.clock.now_plus(start_offset)
    plan["valid_until"] = backend.clock.now_plus(valid_offset)
    plan.update(overrides)  # last, so a test can override any field including timestamps
    return plan


class RulesTableTests(unittest.TestCase):
    def test_code_sets_and_status_table_agree(self):
        self.assertEqual(set(RULES["contract_error_codes"]), set(contract_error_codes()),
                         "api-protocol.rules.json must equal the contract's ERROR_CODES")
        declared = set(RULES["contract_error_codes"]) | set(RULES["transport_error_codes"]) | set(RULES["store_error_codes"])
        self.assertEqual(declared, set(RULES["error_status"]),
                         "every declared code must carry a status and vice versa")

    def test_reason_vocabularies_cover_the_scheduler(self):
        self.assertTrue(FAILED_STOP_REASONS <= set(RULES["failure_reasons"]),
                        "every FAILED_STOP_REASONS entry must be a documented failure reason")
        for reason in ("duration_target_reached", "volume_target_reached", "cancel_stop_dispatched"):
            self.assertIn(reason, RULES["stop_reasons"])
        for reason in ("clock_unsynchronized", "executor_gate_failed", "pump_run_active"):
            self.assertIn(reason, RULES["delay_reasons"])

    def test_state_sets_are_consistent(self):
        self.assertTrue(set(RULES["active_plan_states"]) <= set(RULES["blocking_plan_states"]))
        self.assertTrue(set(RULES["active_plan_states"]) <= set(RULES["plan_states"]))
        self.assertTrue(set(RULES["terminal_plan_states"]) <= set(RULES["plan_states"]))
        self.assertTrue(set(RULES["claimable_plan_states"]) <= set(RULES["plan_states"]))


class ProtocolTests(unittest.TestCase):
    def setUp(self):
        self.backend = FakeBackend(token=TOKEN)
        self.counter = 0

    def tearDown(self):
        self.backend.close()

    def call(self, method, path, body=None, headers=None):
        return self.backend.service.handle(method, path, AUTH if headers is None else headers,
                                           b"" if body is None else json.dumps(body).encode())

    def put(self, plan, path=None, options=None):
        self.backend.service.validator_options = options if options is not None else dict(DEFAULT_VALIDATOR_OPTIONS)
        target = path or ("/api/v1/watering-plans/" + plan["plan_id"])
        return self.backend.service.handle("PUT", target, AUTH, json.dumps(plan).encode())

    def next_plan(self, spec=None, **overrides):
        self.counter += 1
        return plan_for(self.backend, "conf-%d" % self.counter, spec, **overrides)

    # --- routes, success statuses and method errors --------------------------
    def test_route_success_and_method_errors(self):
        status, _ = self.call("GET", "/api/v1/health")
        self.assertEqual(status, 200)
        status, body = self.call("GET", "/api/v1/device-state")
        self.assertEqual(status, 200)
        self.assertIn("generated_at", body["data"])

        plan = self.next_plan()
        status, _ = self.put(plan)
        self.assertEqual(status, 201)
        self.assertEqual(self.call("GET", "/api/v1/watering-plans")[0], 200)
        self.assertEqual(self.call("GET", "/api/v1/watering-plans/" + plan["plan_id"])[0], 200)
        self.assertEqual(self.call("POST", "/api/v1/watering-plans/" + plan["plan_id"] + "/cancel")[0], 200)

        # A known path with an unsupported method is 405; an unknown path is 404.
        for method, path in (("DELETE", "/api/v1/health"), ("POST", "/api/v1/watering-plans"),
                             ("DELETE", "/api/v1/device-state")):
            status, body = self.call(method, path)
            self.assertEqual((status, body["error"]["code"]), (405, "method_not_allowed"), path)
        status, body = self.call("GET", "/api/v1/unknown")
        self.assertEqual((status, body["error"]["code"]), (404, "not_found"))

    def test_auth_never_leaks_the_token(self):
        status, body = self.call("GET", "/api/v1/health", headers={})
        self.assertEqual((status, body["error"]["code"]), (401, "unauthorized"))
        self.assertNotIn(TOKEN, json.dumps(body))

    def test_error_status_table_is_fully_covered(self):
        observed = {}

        def expect(code, result):
            observed[code] = result[0]
            self.assertEqual(result[0], RULES["error_status"][code], result)
            self.assertEqual(result[1]["error"]["code"], code)

        expect("unauthorized", self.call("GET", "/api/v1/health", headers={}))
        expect("not_found", self.call("GET", "/api/v1/nope"))
        expect("method_not_allowed", self.call("DELETE", "/api/v1/health"))
        expect("plan_not_found", self.call("GET", "/api/v1/watering-plans/absent"))
        expect("invalid_json", self.backend.service.handle("PUT", "/api/v1/watering-plans/x", AUTH, b"{not json"))
        expect("invalid_cursor", self.call("GET", "/api/v1/watering-plans?cursor=%40%40bad"))

        mismatched = self.next_plan()
        expect("plan_id_mismatch", self.put(mismatched, path="/api/v1/watering-plans/other"))

        cases = {
            "invalid_payload": (self.next_plan(bogus=1), None),
            "invalid_revision": (self.next_plan(revision=0), None),
            "invalid_status": (self.next_plan(status="running"), None),
            "invalid_source": (self.next_plan(source="robot"), None),
            "invalid_timezone": (self.next_plan(timezone="Mars/Phobos"), None),
            "invalid_time_window": (self.next_plan(spec={"valid_offset": 0}), None),
            "unsupported_run_mode": (self.next_plan(run_mode="serial"), None),
            "invalid_hard_max_runtime": (self.next_plan(hard_max_runtime_sec=0), None),
            "invalid_targets": (self.next_plan(targets={"mode": "weird"}), None),
            "duplicate_target": (self.next_plan(targets={"mode": "selected", "field_ids": ["field-001", "field-001"]}), None),
            "unknown_or_disabled_field": (self.next_plan(targets={"mode": "selected", "field_ids": ["ghost"]}), None),
            "invalid_stop_rule": (self.next_plan(stop={"type": "duration", "value": -1, "unit": "seconds"}), None),
            "unsupported_stop_type": (self.next_plan(stop={"type": "moisture", "value": 1, "unit": "percent"}), None),
            "unsupported_duration_unit": (self.next_plan(stop={"type": "duration", "value": 1, "unit": "fortnights"}), None),
        }
        for code, (plan, _) in cases.items():
            expect(code, self.put(plan))

        base = dict(DEFAULT_VALIDATOR_OPTIONS)
        unsync = dict(base, clockSynchronized=False)
        expect("clock_unsynchronized", self.put(self.next_plan(), options=unsync))
        no_meter = {k: v for k, v in base.items() if k != "meterStatusByValve"}
        expect("meter_unavailable", self.put(self.next_plan(stop={"type": "volume", "value": 1, "unit": "m3"}), options=no_meter))
        expect("unsupported_volume_unit",
               self.put(self.next_plan(stop={"type": "volume", "value": 1, "unit": "gallons"}), options=base))
        valve2 = dict(base, fields=[{"field_id": "field-001", "valve_id": "valve2", "enabled": True, "flow_meter": "commissioned"}],
                      allowedValveSets=[["valve2"]])
        expect("unsupported_volume_target",
               self.put(self.next_plan(stop={"type": "volume", "value": 1, "unit": "m3"}), options=valve2))
        capacity = dict(base, allowedValveSets=[])
        expect("parallel_capacity_not_commissioned", self.put(self.next_plan(), options=capacity))
        disabled = dict(base, fields=[{"field_id": "field-001", "valve_id": "valve1", "enabled": False, "flow_meter": "commissioned"}])
        expect("no_enabled_targets", self.put(self.next_plan(targets={"mode": "all"}), options=disabled))
        bad_registry = dict(base, fields=[{"field_id": "x"}])
        expect("gateway_configuration_invalid", self.put(self.next_plan(), options=bad_registry))
        broken = FakeBackend(token=TOKEN)
        try:
            broken.service.validator_cli = str(SCENARIOS_PATH.parent / "does-not-exist.js")
            result = broken.service.handle("PUT", "/api/v1/watering-plans/v", AUTH,
                                           json.dumps(plan_for(broken, "v")).encode())
            expect("validator_unavailable", result)
        finally:
            broken.close()

        # invalid_timestamp: a timestamp without an explicit offset.
        expect("invalid_timestamp", self.put(self.next_plan(created_at="2030-01-01T00:00:00")))
        expect("unsupported_schema_version", self.put(self.next_plan(schema_version=99)))

        # Revision conflicts: same revision changed, lower revision, active replace.
        rev = self.next_plan(spec={"start_offset": 3600})
        self.assertEqual(self.put(rev)[0], 201)
        expect("revision_conflict", self.put(dict(rev, summary="changed")))
        self.assertEqual(self.put(dict(rev, revision=2, summary="edit"))[0], 200)
        expect("stale_revision", self.put(rev))
        active = self.next_plan()  # due now; the only claimable plan when advanced
        self.put(active)
        self.backend.advance(1)
        expect("active_revision_conflict", self.put(dict(active, revision=2)))

        self.assertEqual(set(observed) | INTERNAL_ONLY_CODES, set(RULES["error_status"]),
                         "every error_status code must be either exercised or documented internal-only")

    def test_idempotency_revision_and_cancel_outcomes(self):
        # Future start_at so the scheduler, advanced below, claims only `active`.
        plan = self.next_plan(spec={"start_offset": 3600})
        self.assertEqual(self.put(plan)[0], 201)
        self.assertTrue(self.put(plan)[1]["data"]["idempotent"])          # same revision, same content
        changed = dict(plan, summary="changed")
        self.assertEqual((self.put(changed)[0]), 409)                     # same revision, different content
        self.assertEqual(self.put(dict(plan, revision=2, summary="edit"))[0], 200)  # higher revision, future
        self.assertEqual(self.put(plan)[0], 409)                          # lower revision
        active = self.next_plan()                                         # due now
        self.put(active)
        self.backend.advance(1)                                           # claim -> running
        self.assertEqual(self.backend.store.get_plan(active["plan_id"])["state"], "running")
        self.assertEqual(self.put(dict(active, revision=2))[0], 409)      # higher revision while active
        self.assertEqual(self.call("POST", "/api/v1/watering-plans/%s/cancel" % active["plan_id"])[0], 202)

    def test_pagination(self):
        ids = []
        for _ in range(3):
            plan = self.next_plan(spec={"start_offset": 3600})
            self.put(plan)
            ids.append(plan["plan_id"])
        status, body = self.call("GET", "/api/v1/watering-plans?limit=2")
        self.assertEqual(status, 200)
        self.assertEqual(len(body["data"]["items"]), 2)
        self.assertTrue(body["data"]["next_cursor"])
        page2 = self.call("GET", "/api/v1/watering-plans?limit=2&cursor=" + body["data"]["next_cursor"])[1]
        ordered = [item["plan_id"] for item in body["data"]["items"] + page2["data"]["items"]]
        self.assertEqual(ordered, ids, "ascending start_at then plan_id, no duplicates")

    def test_python_does_not_reimplement_validation(self):
        self.backend.service.validator_cli = ""
        status, body = self.put(self.next_plan())
        self.assertEqual((status, body["error"]["code"]), (500, "validator_unavailable"))

    def test_scenario_header_is_inert_on_reads(self):
        plan = self.next_plan()
        self.put(plan)
        before = self.backend.store.get_plan(plan["plan_id"])["state"]
        headers = dict(AUTH, **{"X-Mock-Scenario": "drop-after-store"})
        status, _ = self.call("GET", "/api/v1/watering-plans/" + plan["plan_id"], headers=headers)
        self.assertEqual(status, 200)
        self.assertEqual(self.backend.store.get_plan(plan["plan_id"])["state"], before)


class LifecycleScenarioTests(unittest.TestCase):
    def run_scenario(self, scenario):
        backend = FakeBackend(token=TOKEN)
        try:
            plan = plan_for(backend, "scn-" + scenario["name"], scenario.get("plan"))
            status, body = backend.service.handle(
                "PUT", "/api/v1/watering-plans/" + plan["plan_id"], AUTH, json.dumps(plan).encode())
            self.assertEqual(status, 201, body)
            for action in scenario["actions"]:
                if "advance" in action:
                    backend.advance(action["advance"])
                elif "gate" in action:
                    backend.set_gate(action["gate"])
                elif action.get("clear_gate"):
                    backend.clear_gate()
                elif action.get("restart"):
                    backend.restart()
                elif "inject" in action:
                    for key, value in action["inject"].items():
                        if key == "meter_unavailable":
                            backend.inject_meter_unavailable(value)
                        elif key == "meter_total":
                            backend.inject_meter_total(value)
                        elif key == "sequencer_fault":
                            backend.inject_sequencer_fault(value)
                elif "regress_monotonic" in action:
                    backend.clock.regress_monotonic(action["regress_monotonic"])
                elif action.get("run_until_terminal"):
                    backend.run_until(plan["plan_id"])
                elif action.get("cancel"):
                    status, body = backend.service.handle(
                        "POST", "/api/v1/watering-plans/%s/cancel" % plan["plan_id"], AUTH, b"{}")
                    self.assertEqual((status, body["data"]["state"]), (202, "stop_requested"))
                elif "assert_state" in action:
                    state = backend.store.get_plan(plan["plan_id"])["state"]
                    self.assertEqual(state, action["assert_state"], scenario["name"])
                elif "assert_delay_reason" in action:
                    reason = backend.store.get_plan(plan["plan_id"])["last_delay_reason"]
                    self.assertEqual(reason, action["assert_delay_reason"], scenario["name"])
                else:
                    self.fail("unknown scenario action %r" % (action,))
            record = backend.store.get_plan(plan["plan_id"], include_history=True)
            attempt = (record.get("attempts") or [None])[-1]
            return backend, record, attempt
        except Exception:
            backend.close()
            raise

    def test_scenarios(self):
        for scenario in SCENARIOS["scenarios"]:
            with self.subTest(scenario=scenario["name"]):
                backend, record, attempt = self.run_scenario(scenario)
                try:
                    expect = scenario["expect"]
                    self.assertEqual(record["state"], expect["final_state"])
                    if "execution_stop_reason" in expect:
                        self.assertEqual((attempt or {}).get("final_reason"), expect["execution_stop_reason"])
                    if "actual_runtime_ms" in expect:
                        self.assertEqual((attempt or {}).get("actual_runtime_ms"), expect["actual_runtime_ms"])
                    if "delivered_m3" in expect:
                        self.assertAlmostEqual((attempt or {}).get("delivered_m3"), expect["delivered_m3"], places=6)
                    if "safe_stops_min" in expect:
                        self.assertGreaterEqual(backend.sequencer.safe_stops, expect["safe_stops_min"])
                    self.assertIn(record["state"], RULES["plan_states"])
                finally:
                    backend.close()


if __name__ == "__main__":
    unittest.main(verbosity=2)
