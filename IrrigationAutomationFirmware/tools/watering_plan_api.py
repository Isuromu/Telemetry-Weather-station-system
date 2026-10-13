"""Temporary authenticated HTTP API and service wiring for watering plans."""

from __future__ import annotations

import json
import os
import secrets
import subprocess
import threading
import time
import uuid
from datetime import datetime, timezone
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any, Callable, Dict, Optional, Tuple
from urllib.parse import parse_qs, urlparse

try:
    from watering_plan_scheduler import WateringPlanScheduler
    from watering_plan_store import PlanNotFound, PlanStoreError, WateringPlanStore
except ImportError:  # pragma: no cover
    from .watering_plan_scheduler import WateringPlanScheduler
    from .watering_plan_store import PlanNotFound, PlanStoreError, WateringPlanStore


class ValidationError(PlanStoreError):
    def __init__(self, error: Dict[str, Any]):
        super().__init__(error["code"], error["message"], error.get("details", []))


class AuthError(ValidationError):
    pass


class WateringPlanService:
    def __init__(self, store: WateringPlanStore, token: str,
                 validator_options: Optional[Dict[str, Any]] = None,
                 node: str = "node", validator_cli: Optional[str] = None,
                 scheduler: Optional[WateringPlanScheduler] = None,
                 device_state: Optional[Callable[[], Dict[str, Any]]] = None):
        if not token:
            raise ValueError("Bearer token must be configured")
        self.store = store
        self.token = token
        self.validator_options = validator_options or {
            "clockSynchronized": True,
            "gatewayMaxRuntimeSec": 3600,
            "fields": [{"field_id": "field-001", "valve_id": "valve1", "enabled": True,
                        "flow_meter": "commissioned"}],
            "allowedValveSets": [["valve1"]],
            "durationUnits": ["seconds", "minutes", "hours"],
            "volumeUnits": ["m3", "liters"],
        }
        self.node = node
        self.validator_cli = validator_cli or str(Path(__file__).with_name("watering_plan_contract_cli.js"))
        self.scheduler = scheduler
        self.device_state = device_state or self._unavailable_device_state
        self._scheduler_thread: Optional[threading.Thread] = None

    @staticmethod
    def _unavailable_device_state() -> Dict[str, Any]:
        now = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
        return {"status": "unavailable", "reason": "device_state_provider_not_configured",
                "generated_at": now, "observed_at": None, "devices": []}

    def _validate(self, payload: Dict[str, Any], path_plan_id: Optional[str]) -> Dict[str, Any]:
        options = dict(self.validator_options)
        if path_plan_id is not None:
            options["pathPlanId"] = path_plan_id
        if self.validator_cli:
            command = [self.node, self.validator_cli]
            request = {"payload": payload, "options": options}
            result = subprocess.run(command, input=json.dumps(request), text=True,
                                    capture_output=True, check=False)
            if result.returncode != 0 and not result.stdout.strip():
                raise ValidationError({"code": "validator_unavailable", "message": result.stderr.strip() or "validator failed", "details": []})
            try:
                outcome = json.loads(result.stdout)
            except json.JSONDecodeError as error:
                raise ValidationError({"code": "validator_unavailable", "message": str(error), "details": []})
        else:
            raise ValidationError({"code": "validator_unavailable", "message": "Validator CLI is not configured", "details": []})
        if not outcome.get("ok"):
            raise ValidationError(outcome["error"])
        return outcome

    def start_scheduler(self, interval_seconds: float = 5.0) -> None:
        if self.scheduler is None or self._scheduler_thread is not None:
            return
        def loop() -> None:
            while True:
                self.scheduler.tick()
                time.sleep(interval_seconds)
        self._scheduler_thread = threading.Thread(target=loop, name="watering-plan-scheduler", daemon=True)
        self._scheduler_thread.start()

    def handle(self, method: str, path: str, headers: Dict[str, str], body: bytes = b"") -> Tuple[int, Dict[str, Any]]:
        request_id = uuid.uuid4().hex
        try:
            self._authorize(headers)
            parsed = urlparse(path)
            route = parsed.path
            if method == "GET" and route == "/api/v1/health":
                data = {"api_version": "v1", "time": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
                        "clock_synchronized": self.validator_options.get("clockSynchronized") is True}
                return self._success(request_id, data)
            if method == "GET" and route == "/api/v1/device-state":
                return self._success(request_id, self.device_state())
            if method == "GET" and route == "/api/v1/watering-plans":
                query = parse_qs(parsed.query)
                result = self.store.list_plans(
                    query.get("from", [None])[0], query.get("to", [None])[0],
                    int(query.get("limit", [50])[0]), query.get("cursor", [None])[0])
                result["items"] = [self._public_plan(plan) for plan in result["items"]]
                return self._success(request_id, result)
            plan_id, action = self._plan_route(route)
            if plan_id is None:
                return self._error(request_id, 404, "not_found", "API path was not found")
            if method == "GET" and action is None:
                return self._success(request_id, self._public_plan(
                    self.store.get_plan(plan_id, include_history=True)))
            if method == "PUT" and action is None:
                payload = self._json_body(body)
                validation = self._validate(payload, plan_id)
                result = self.store.accept_plan(validation)
                status = 201 if result.get("created") else 200
                return self._success(request_id, self._public_plan(result), status)
            if method == "POST" and action == "cancel":
                result = self.store.cancel_plan(plan_id)
                status = 202 if result["state"] == "stop_requested" else 200
                return self._success(request_id, self._public_plan(result), status)
            return self._error(request_id, 405, "method_not_allowed", "HTTP method is not supported")
        except AuthError as error:
            return self._error(request_id, 401, error.code, error.message, error.details)
        except ValidationError as error:
            status = 500 if error.code == "validator_unavailable" else 422
            return self._error(request_id, status, error.code, error.message, error.details)
        except PlanNotFound as error:
            return self._error(request_id, 404, error.code, error.message, error.details)
        except PlanStoreError as error:
            status = 400 if error.code == "invalid_cursor" else 409
            return self._error(request_id, status, error.code, error.message, error.details)
        except (ValueError, json.JSONDecodeError) as error:
            return self._error(request_id, 400, "invalid_json", str(error))
        except Exception:
            return self._error(request_id, 500, "internal_error", "Unexpected internal error")

    def _authorize(self, headers: Dict[str, str]) -> None:
        value = headers.get("Authorization", headers.get("authorization", ""))
        expected = "Bearer " + self.token
        if not value or not secrets.compare_digest(value, expected):
            raise AuthError({"code": "unauthorized", "message": "Bearer credential is invalid", "details": []})

    @staticmethod
    def _public_plan(stored: Dict[str, Any]) -> Dict[str, Any]:
        """Expose the shared mobile record shape instead of SQLite columns."""
        plan = stored.get("payload")
        if not isinstance(plan, dict):
            try:
                plan = json.loads(plan)
            except (TypeError, json.JSONDecodeError):
                plan = {}
        record = dict(plan)
        record.update({
            "state": stored.get("state"),
            "accepted_at": stored.get("accepted_at"),
            "updated_at": stored.get("updated_at"),
        })
        if stored.get("last_delay_reason"):
            record["delay_reason"] = stored["last_delay_reason"]
        attempts = stored.get("attempts") or []
        attempt = stored.get("attempt") or (attempts[-1] if attempts else None)
        if attempt:
            execution = {
                "operation_id": attempt.get("operation_id"),
                "actual_started_at": attempt.get("actual_started_at"),
                "actual_ended_at": attempt.get("actual_ended_at"),
                "actual_volume_m3": attempt.get("delivered_m3"),
                "stop_reason": attempt.get("final_reason"),
            }
            record["execution"] = {key: value for key, value in execution.items() if value is not None}
        if "events" in stored:
            record["events"] = stored["events"]
        for key in ("created", "replaced", "idempotent"):
            if key in stored:
                record[key] = stored[key]
        return record

    @staticmethod
    def _json_body(body: bytes) -> Dict[str, Any]:
        if len(body) > 1024 * 1024:
            raise ValueError("request body is too large")
        value = json.loads(body.decode("utf-8"))
        if not isinstance(value, dict):
            raise ValueError("request body must be a JSON object")
        return value

    @staticmethod
    def _plan_route(route: str) -> Tuple[Optional[str], Optional[str]]:
        prefix = "/api/v1/watering-plans/"
        if not route.startswith(prefix):
            return None, None
        parts = route[len(prefix):].strip("/").split("/")
        if len(parts) == 1 and parts[0]:
            return parts[0], None
        if len(parts) == 2 and parts[0] and parts[1] == "cancel":
            return parts[0], "cancel"
        return None, None

    @staticmethod
    def _success(request_id: str, data: Any, status: int = 200) -> Tuple[int, Dict[str, Any]]:
        return status, {"api_version": "v1", "request_id": request_id, "data": data}

    @staticmethod
    def _error(request_id: str, status: int, code: str, message: str, details: Optional[list] = None) -> Tuple[int, Dict[str, Any]]:
        return status, {"api_version": "v1", "request_id": request_id,
                        "error": {"code": code, "message": message, "details": details or []}}


class WateringPlanHandler(BaseHTTPRequestHandler):
    service: WateringPlanService

    def _serve(self) -> None:
        length = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(length) if length else b""
        status, payload = self.service.handle(self.command, self.path, dict(self.headers), body)
        encoded = json.dumps(payload, separators=(",", ":")).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(encoded)))
        self.end_headers()
        self.wfile.write(encoded)

    do_GET = _serve
    do_POST = _serve
    do_PUT = _serve

    def log_message(self, format: str, *args: Any) -> None:  # pragma: no cover
        return


def serve(service: WateringPlanService, host: str = "127.0.0.1", port: int = 8088) -> None:
    handler = type("BoundWateringPlanHandler", (WateringPlanHandler,), {"service": service})
    server = ThreadingHTTPServer((host, port), handler)
    server.serve_forever()


__all__ = ["WateringPlanService", "serve"]
