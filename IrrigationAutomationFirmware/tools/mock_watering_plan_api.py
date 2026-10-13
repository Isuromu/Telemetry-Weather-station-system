#!/usr/bin/env python3
"""Deterministic mock for mobile watering-plan API tests; not Node-RED."""
import argparse
import hashlib
import json
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlparse

TOKEN = "test-token"


class MockState:
    def __init__(self):
        self.plans = {}
        self.lock = threading.Lock()


class MockHandler(BaseHTTPRequestHandler):
    server_version = "WateringPlanMock/1.0"

    def log_message(self, _format, *_args):
        # Never log authorization headers or request bodies.
        return

    def _send(self, status, request_id, data=None, error=None):
        envelope = {"api_version": "v1", "request_id": request_id}
        envelope["data" if error is None else "error"] = data if error is None else error
        raw = json.dumps(envelope, separators=(",", ":")).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(raw)))
        self.end_headers()
        self.wfile.write(raw)

    def _error(self, status, request_id, code, message, details=None):
        self._send(status, request_id, error={"code": code, "message": message, "details": details or []})

    def _auth(self, request_id):
        if self.headers.get("Authorization") != "Bearer " + TOKEN:
            self._error(401, request_id, "unauthorized", "Bearer credential is invalid")
            return False
        return True

    def _request_id(self):
        return self.headers.get("X-Request-ID") or "mock-generated-request-id"

    def _body(self):
        try:
            length = int(self.headers.get("Content-Length", "0"))
            if length <= 0 or length > 1024 * 1024:
                return None
            return json.loads(self.rfile.read(length).decode("utf-8"))
        except (ValueError, UnicodeDecodeError):
            return None

    def do_GET(self):
        request_id = self._request_id()
        if not self._auth(request_id):
            return
        parsed = urlparse(self.path)
        if parsed.path == "/api/v1/health":
            return self._send(200, request_id, {"time": "2030-01-01T00:00:00Z", "clock_synchronized": True})
        if parsed.path == "/api/v1/device-state":
            return self._send(200, request_id, {"generated_at": "2030-01-01T00:00:00Z", "status": "available", "devices": []})
        if parsed.path == "/api/v1/watering-plans":
            query = parse_qs(parsed.query)
            try:
                limit = min(100, max(1, int(query.get("limit", [50])[0])))
            except ValueError:
                return self._error(400, request_id, "invalid_limit", "limit must be an integer")
            with self.server.state.lock:
                records = sorted(self.server.state.plans.values(), key=lambda p: (p["start_at"], p["plan_id"]))
            if query.get("from"):
                records = [p for p in records if p["start_at"] >= query["from"][0]]
            if query.get("to"):
                records = [p for p in records if p["start_at"] <= query["to"][0]]
            cursor = query.get("cursor", [None])[0]
            if cursor:
                records = [p for p in records if p["plan_id"] > cursor]
            page = records[:limit]
            next_cursor = page[-1]["plan_id"] if len(records) > limit and page else None
            return self._send(200, request_id, {"items": page, "next_cursor": next_cursor})
        prefix = "/api/v1/watering-plans/"
        if parsed.path.startswith(prefix) and "/cancel" not in parsed.path:
            plan_id = parsed.path[len(prefix):]
            with self.server.state.lock:
                record = self.server.state.plans.get(plan_id)
            if record is None:
                return self._error(404, request_id, "not_found", "Plan was not found")
            return self._send(200, request_id, record)
        return self._error(404, request_id, "not_found", "API path was not found")

    def do_PUT(self):
        request_id = self._request_id()
        if not self._auth(request_id):
            return
        path = urlparse(self.path).path
        prefix = "/api/v1/watering-plans/"
        if not path.startswith(prefix):
            return self._error(404, request_id, "not_found", "API path was not found")
        plan_id = path[len(prefix):]
        payload = self._body()
        if not isinstance(payload, dict):
            return self._error(400, request_id, "invalid_payload", "Body must be a JSON object")
        validation = self._validate(payload, plan_id)
        if validation:
            status, code, message = validation
            return self._error(status, request_id, code, message)
        if self.headers.get("X-Mock-Scenario") == "internal-error":
            return self._error(500, request_id, "internal_error", "Simulated internal failure")
        digest = hashlib.sha256(json.dumps(payload, sort_keys=True, separators=(",", ":")).encode()).hexdigest()
        with self.server.state.lock:
            existing = self.server.state.plans.get(plan_id)
            if existing:
                if payload["revision"] == existing["revision"]:
                    if digest == existing["_digest"]:
                        record = {k: v for k, v in existing.items() if k != "_digest"}
                        status = 200
                    else:
                        return self._error(409, request_id, "revision_conflict", "Same revision has different content")
                elif payload["revision"] < existing["revision"]:
                    return self._error(409, request_id, "stale_revision", "Revision is older than stored plan")
                elif existing["state"] not in ("scheduled", "delayed"):
                    return self._error(409, request_id, "active_plan_revision_conflict", "Only future plans can be replaced")
                else:
                    record = self._record(payload, digest)
                    status = 200
                    self.server.state.plans[plan_id] = record
            else:
                record = self._record(payload, digest)
                status = 201
                self.server.state.plans[plan_id] = record
        if self.headers.get("X-Mock-Scenario") == "drop-after-store":
            self.close_connection = True
            return
        return self._send(status, request_id, {k: v for k, v in record.items() if k != "_digest"})

    def do_POST(self):
        request_id = self._request_id()
        if not self._auth(request_id):
            return
        path = urlparse(self.path).path
        suffix = "/cancel"
        prefix = "/api/v1/watering-plans/"
        if not path.startswith(prefix) or not path.endswith(suffix):
            return self._error(404, request_id, "not_found", "API path was not found")
        plan_id = path[len(prefix):-len(suffix)]
        with self.server.state.lock:
            record = self.server.state.plans.get(plan_id)
            if record is None:
                return self._error(404, request_id, "not_found", "Plan was not found")
            if record["state"] in ("scheduled", "delayed"):
                record["state"] = "cancelled"
                record["status"] = "cancelled"
                status = 200
            elif record["state"] == "running":
                record["state"] = "stop_requested"
                status = 202
            else:
                status = 200
            result = {k: v for k, v in record.items() if k != "_digest"}
        return self._send(status, request_id, result)

    @staticmethod
    def _validate(plan, path_id):
        required = ("schema_version", "plan_id", "revision", "status", "source", "created_at", "timezone", "start_at", "valid_until", "targets", "run_mode", "stop", "hard_max_runtime_sec")
        if any(key not in plan for key in required):
            return 400, "invalid_payload", "Required plan field is missing"
        if plan["schema_version"] != 1:
            return 422, "unsupported_schema_version", "Only schema_version 1 is supported"
        if plan["plan_id"] != path_id:
            return 400, "plan_id_mismatch", "URL plan_id does not match payload plan_id"
        if not isinstance(plan["revision"], int) or plan["revision"] < 1:
            return 422, "invalid_revision", "revision must be a positive integer"
        if plan["status"] != "scheduled" or plan["source"] not in ("human", "ai"):
            return 422, "invalid_plan", "status or source is unsupported"
        if plan["run_mode"] != "parallel" or not isinstance(plan["targets"], dict) or plan["targets"].get("field_ids") != ["field-001"]:
            return 422, "unsupported_target", "Mock supports field-001 only"
        if not isinstance(plan["stop"], dict) or plan["stop"].get("type") != "duration" or plan["stop"].get("unit") not in ("seconds", "minutes", "hours") or not isinstance(plan["stop"].get("value"), (int, float)) or plan["stop"]["value"] <= 0:
            return 422, "unsupported_stop_rule", "Mock accepts positive duration plans only"
        if not isinstance(plan["hard_max_runtime_sec"], int) or plan["hard_max_runtime_sec"] < 1:
            return 422, "invalid_hard_max_runtime", "hard_max_runtime_sec must be positive"
        for name in ("created_at", "start_at", "valid_until"):
            value = plan.get(name)
            if not isinstance(value, str) or "T" not in value or not (value.endswith("Z") or "+" in value[10:] or "-" in value[10:]):
                return 422, "invalid_timestamp", name + " must include explicit offset"
        if plan["start_at"] >= plan["valid_until"]:
            return 422, "invalid_time_window", "start_at must precede valid_until"
        return None

    @staticmethod
    def _record(payload, digest):
        return dict(payload, state="scheduled", accepted_at=payload["created_at"], updated_at=payload["created_at"], _digest=digest)


def make_server(host="127.0.0.1", port=8088):
    server = ThreadingHTTPServer((host, port), MockHandler)
    server.state = MockState()
    return server


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8088)
    args = parser.parse_args()
    server = make_server(args.host, args.port)
    print("Mock API listening at http://%s:%d/api/v1 (token: test-token)" % (args.host, args.port))
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
