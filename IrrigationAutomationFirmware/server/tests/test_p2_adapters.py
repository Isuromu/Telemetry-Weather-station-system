from datetime import datetime, timedelta, timezone

import pytest
from fastapi.testclient import TestClient

from irrigation_server.adapters.influx import InfluxProvider, InfluxUnavailable
from irrigation_server.adapters.node_red import NodeRedAdapter, NodeRedError
from irrigation_server import Settings, create_app


class EmptyStore:
    def list_plans(self, *args):
        return {"items": [], "next_cursor": None}


class Record:
    def __init__(self, device, observed, fields):
        self.values = {"device_id": device, **fields}
        self.observed = observed

    def get_time(self):
        return self.observed


class Table:
    def __init__(self, *records):
        self.records = list(records)


class QueryApi:
    def __init__(self, tables=None, error=None):
        self.tables = tables or []
        self.error = error
        self.query_text = ""

    def query(self, query, org):
        self.query_text = query
        if self.error:
            raise self.error
        return self.tables


class InfluxClient:
    def __init__(self, query_api):
        self.api = query_api

    def query_api(self):
        return self.api


def test_device_state_reports_fresh_stale_and_unavailable():
    now = datetime(2026, 10, 18, tzinfo=timezone.utc)
    query = QueryApi([Table(
        Record("pump", now - timedelta(seconds=30), {"running": 0, "motor_current_a": 1.2}),
        Record("soil", now - timedelta(seconds=301), {"vwc_percent": 40.0}),
    )])
    provider = InfluxProvider("url", "org", "telemetry", "events", "token", 300, InfluxClient(query))
    result = provider.device_state(now)
    devices = {item["device"]: item for item in result["devices"]}
    assert result["status"] == "stale"
    assert devices["pump"]["status"] == "available"
    assert devices["pump"]["data"]["running"] == 0
    assert devices["soil"]["status"] == "stale"
    assert devices["valve1"]["status"] == "unavailable"
    assert "device_telemetry" in query.query_text
    assert "device_id" in query.query_text


def test_device_freshness_can_be_configured_per_logical_device():
    now = datetime(2026, 10, 18, tzinfo=timezone.utc)
    query = QueryApi([Table(
        Record("pump", now - timedelta(seconds=61), {"running": 0}),
        Record("soil", now - timedelta(seconds=61), {"vwc_percent": 40.0}),
    )])
    provider = InfluxProvider("url", "org", "telemetry", "events", "token", {"pump": 60}, InfluxClient(query))
    devices = {item["device"]: item for item in provider.device_state(now)["devices"]}
    assert devices["pump"]["status"] == "stale"
    assert devices["soil"]["status"] == "available", "unspecified device uses configured global fallback"


    provider = InfluxProvider("url", "org", "telemetry", "events", "token", client=InfluxClient(QueryApi(error=OSError())))
    with pytest.raises(InfluxUnavailable):
        provider.device_state()


def test_operation_event_query_filters_and_orders_by_operation_id():
    now = datetime(2026, 10, 18, tzinfo=timezone.utc)
    query = QueryApi([Table(Record("pump", now, {"event_type": "start_requested"}))])
    provider = InfluxProvider("url", "org", "telemetry", "events", "token", client=InfluxClient(query))
    events = provider.operation_events('op-"quoted"')
    assert len(events) == 1
    assert events[0]["event_type"] == "start_requested"
    assert events[0]["observed_at"] == "2026-10-18T00:00:00Z"
    assert r'op-\"quoted\"' in query.query_text


    class Provider:
        def device_state(self):
            return {"generated_at": "now", "status": "available", "devices": []}

    client = TestClient(create_app(Settings(":memory:", "secret"), EmptyStore(), Provider()))
    headers = {"Authorization": "Bearer secret"}
    assert client.get("/api/v1/device-state", headers=headers).json()["data"]["status"] == "available"

    class BrokenProvider:
        def device_state(self):
            raise InfluxUnavailable("offline")

    client = TestClient(create_app(Settings(":memory:", "secret"), EmptyStore(), BrokenProvider()))
    response = client.get("/api/v1/device-state", headers=headers)
    assert response.status_code == 503
    assert response.json()["error"]["code"] == "influx_unavailable"


class FakeResponse:
    status_code = 202

    def __init__(self, body):
        self.body = body

    def json(self):
        return self.body


class FakeHttp:
    def __init__(self, response):
        self.response = response
        self.calls = []

    def post(self, *args, **kwargs):
        self.calls.append((args, kwargs))
        return self.response


def test_node_red_adapter_sends_stable_operation_id_and_maps_duplicate():
    http = FakeHttp(FakeResponse({"data": {"accepted": True, "duplicate": True, "operation_id": "op-1"}}))
    adapter = NodeRedAdapter("http://node-red:1880", "internal", client=http)
    result = adapter.start("op-1", {"valve1": True}, "plan-1", 3)
    assert result["duplicate"] is True
    assert http.calls[0][1]["json"]["operation_id"] == "op-1"
    assert http.calls[0][1]["json"]["plan_id"] == "plan-1"
    assert http.calls[0][1]["json"]["plan_revision"] == 3
    assert http.calls[0][1]["headers"]["Authorization"] == "Bearer internal"
    assert http.calls[0][1]["timeout"] == 3.0


def test_node_red_adapter_validates_payload_and_response():
    adapter = NodeRedAdapter("http://node-red", "token", client=FakeHttp(FakeResponse({"data": {"operation_id": "other"}})))
    with pytest.raises(ValueError):
        adapter.start("op", {"valve1": "true"})
    with pytest.raises(NodeRedError, match="did not match"):
        adapter.stop("op")


def test_node_red_adapter_maps_http_error():
    response = FakeResponse({"error": {"code": "executor_busy", "message": "busy"}})
    response.status_code = 409
    adapter = NodeRedAdapter("http://node-red", "token", client=FakeHttp(response))
    with pytest.raises(NodeRedError) as error:
        adapter.stop("op")
    assert error.value.code == "executor_busy"
    assert error.value.status_code == 409
