"""FastAPI application factory."""

from fastapi import FastAPI, Request
from fastapi.responses import JSONResponse

from .adapters.influx import InfluxUnavailable
from .config import Settings


def _error(status_code, code, message):
    return JSONResponse(
        status_code=status_code,
        content={"api_version": "v1", "error": {"code": code, "message": message, "details": []}},
    )


def create_app(settings: Settings, store, provider) -> FastAPI:
    """Build the server application around a plan store and Influx provider."""
    app = FastAPI(title="Irrigation Server")
    app.state.settings = settings
    app.state.store = store
    app.state.provider = provider

    def authorized(request: Request) -> bool:
        token = settings.plan_api_token
        return bool(token) and request.headers.get("authorization") == "Bearer " + token

    @app.get("/api/v1/health")
    def health():
        return {"api_version": "v1", "data": {"status": "ok"}}

    @app.get("/api/v1/device-state")
    def device_state(request: Request):
        if not authorized(request):
            return _error(401, "unauthorized", "Bearer credential is invalid")
        try:
            data = provider.device_state()
        except InfluxUnavailable:
            return _error(503, "influx_unavailable", "InfluxDB is unavailable")
        return {"api_version": "v1", "data": data}

    return app
