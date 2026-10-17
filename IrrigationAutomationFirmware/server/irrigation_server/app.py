import json
import secrets
import subprocess
import uuid
from datetime import datetime, timezone
from pathlib import Path
from urllib.parse import parse_qs

from fastapi import FastAPI, Request
from fastapi.responses import JSONResponse

from .config import Settings
from .store import PlanNotFound, PlanStoreError, WateringPlanStore

ROOT = Path(__file__).resolve().parents[2]


def create_app(settings: Settings | None = None, store=None) -> FastAPI:
    settings = settings or Settings.from_env()
    app = FastAPI(title="Irrigation Server", version="1.0")
    if store is None:
        if not settings.bearer_token:
            raise ValueError("IRRIGATION_API_TOKEN must be configured")
        store = WateringPlanStore(settings.database_path)
    app.state.store = store
    app.state.settings = settings

    @app.middleware("http")
    async def envelope(request: Request, call_next):
        request_id = request.headers.get("x-request-id", "").strip()[:128] or uuid.uuid4().hex
        try:
            auth = request.headers.get("authorization", "")
            if not settings.bearer_token or not secrets.compare_digest(auth, "Bearer " + settings.bearer_token):
                result = error(request_id, 401, "unauthorized", "Bearer credential is invalid")
            else:
                response = await call_next(request)
                response.headers["X-Request-ID"] = request_id
                if response.headers.get("content-type", "").startswith("application/json"):
                    body = b"".join([chunk async for chunk in response.body_iterator])
                    payload = json.loads(body)
                    payload["request_id"] = request_id
                    return JSONResponse(payload, status_code=response.status_code, headers={"X-Request-ID": request_id})
                return response
        except Exception:
            result = error(request_id, 500, "internal_error", "Unexpected internal error")
        result.headers["X-Request-ID"] = request_id
        return result

    @app.get("/api/v1/health")
    def health():
            return success({"api_version": "v1", "time": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"), "clock_synchronized": True if settings.validator_options is None else settings.validator_options.get("clockSynchronized") is True})

    @app.get("/api/v1/device-state")
    def device_state():
        return success({"status": "unavailable", "reason": "device_state_provider_not_configured", "generated_at": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"), "observed_at": None, "devices": []})

    @app.get("/api/v1/watering-plans")
    def list_plans(request: Request):
        q = request.query_params
        try:
            result = store.list_plans(q.get("from"), q.get("to"), int(q.get("limit", 50)), q.get("cursor"))
            result["items"] = [public_plan(x) for x in result["items"]]
            return success(result)
        except PlanStoreError as e:
            return error(uuid.uuid4().hex, 400, e.code, e.message, e.details)
        except ValueError as e:
            return error(uuid.uuid4().hex, 400, "invalid_query", str(e))

    @app.get("/api/v1/watering-plans/{plan_id}")
    def get_plan(plan_id: str):
        try:
            return success(public_plan(store.get_plan(plan_id, include_history=True)))
        except PlanNotFound as e:
            return error(uuid.uuid4().hex, 404, e.code, e.message, e.details)

    @app.put("/api/v1/watering-plans/{plan_id}")
    async def put_plan(plan_id: str, request: Request):
        try:
            raw = await request.body()
            if len(raw) > 1048576:
                raise ValueError("request body is too large")
            payload = json.loads(raw)
            if not isinstance(payload, dict):
                raise ValueError("request body must be a JSON object")
            options = dict(settings.validator_options) if settings.validator_options is not None else {"clockSynchronized": True, "gatewayMaxRuntimeSec": 3600, "fields": [{"field_id":"field-001","valve_id":"valve1","enabled":True,"flow_meter":"commissioned"}], "allowedValveSets":[["valve1"]], "durationUnits":["seconds","minutes","hours"], "volumeUnits":["m3","liters"]}
            options["pathPlanId"] = plan_id
            cli = settings.validator_cli or str(ROOT / "tools" / "watering_plan_contract_cli.js")
            proc = subprocess.run([settings.node, cli], input=json.dumps({"payload":payload,"options":options}), text=True, capture_output=True, check=False)
            if not proc.stdout.strip():
                return error(uuid.uuid4().hex, 500, "validator_unavailable", proc.stderr.strip() or "validator failed")
            outcome = json.loads(proc.stdout)
            if not outcome.get("ok"):
                e = outcome["error"]
                return error(uuid.uuid4().hex, 422, e["code"], e["message"], e.get("details", []))
            result = store.accept_plan(outcome)
            status = 201 if result.get("created") else 200
            return success(public_plan(result), status)
        except (json.JSONDecodeError, UnicodeDecodeError, ValueError) as e:
            return error(uuid.uuid4().hex, 400, "invalid_json", str(e))
        except PlanStoreError as e:
            return error(uuid.uuid4().hex, 409, e.code, e.message, e.details)
        except (OSError, subprocess.SubprocessError) as e:
            return error(uuid.uuid4().hex, 500, "validator_unavailable", str(e))

    @app.post("/api/v1/watering-plans/{plan_id}/cancel")
    def cancel_plan(plan_id: str):
        try:
            result = store.cancel_plan(plan_id)
            return success(public_plan(result), 202 if result["state"] == "stop_requested" else 200)
        except PlanNotFound as e:
            return error(uuid.uuid4().hex, 404, e.code, e.message, e.details)

    @app.exception_handler(404)
    async def not_found(request, exc):
        return error(uuid.uuid4().hex, 404, "not_found", "API path was not found")

    @app.exception_handler(405)
    async def method_not_allowed(request, exc):
        return error(uuid.uuid4().hex, 405, "method_not_allowed", "HTTP method is not supported")
    return app


def success(data, status=200):
    return JSONResponse({"api_version":"v1","request_id":"", "data":data}, status_code=status)


def error(request_id, status, code, message, details=None):
    return JSONResponse({"api_version":"v1","request_id":request_id,"error":{"code":code,"message":message,"details":details or []}}, status_code=status)


def public_plan(stored):
    plan = stored.get("payload") or {}
    if isinstance(plan, str):
        plan = json.loads(plan)
    record = dict(plan)
    record.update({"state":stored.get("state"),"accepted_at":stored.get("accepted_at"),"updated_at":stored.get("updated_at")})
    if stored.get("last_delay_reason"):
        record["delay_reason"] = stored["last_delay_reason"]
    attempt = stored.get("attempt") or ((stored.get("attempts") or [None])[-1])
    if attempt:
        execution = {"operation_id":attempt.get("operation_id"),"actual_started_at":attempt.get("actual_started_at"),"actual_ended_at":attempt.get("actual_ended_at"),"actual_volume_m3":attempt.get("delivered_m3"),"stop_reason":attempt.get("final_reason")}
        record["execution"] = {k:v for k,v in execution.items() if v is not None}
    if "events" in stored: record["events"] = stored["events"]
    for key in ("created","replaced","idempotent"):
        if key in stored: record[key] = stored[key]
    return record
