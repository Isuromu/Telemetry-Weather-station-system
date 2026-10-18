"""InfluxDB telemetry and operation-event reader."""

from datetime import datetime, timedelta, timezone
import re


DEVICE_IDS = ("main_valve", "pump", "valve1", "valve2", "soil", "water_level")


class InfluxUnavailable(Exception):
    """InfluxDB could not answer a query."""


class InfluxProvider:
    def __init__(self, url, org, telemetry_bucket, events_bucket, token,
                 freshness_seconds=300, client=None):
        self.org = org
        self.telemetry_bucket = telemetry_bucket
        self.events_bucket = events_bucket
        self.freshness_seconds = freshness_seconds
        if isinstance(freshness_seconds, dict):
            if any(int(value) <= 0 for value in freshness_seconds.values()):
                raise ValueError("freshness limits must be positive")
        elif int(freshness_seconds) <= 0:
            raise ValueError("freshness_seconds must be positive")
        self._client = client
        self._url = url
        self._token = token

    def _query_api(self):
        if self._client is None:
            if not all((self._url, self.org, self.telemetry_bucket, self._token)):
                raise InfluxUnavailable("InfluxDB is not configured")
            try:
                from influxdb_client import InfluxDBClient
                self._client = InfluxDBClient(url=self._url, token=self._token, org=self.org)
            except Exception as exc:
                raise InfluxUnavailable("InfluxDB client unavailable") from exc
        return self._client.query_api()

    def close(self):
        if self._client is not None:
            self._client.close()
            self._client = None

    def device_state(self, now=None):
        now = now or datetime.now(timezone.utc)
        limits = list(self.freshness_seconds.values()) if isinstance(self.freshness_seconds, dict) else [self.freshness_seconds]
        start = now - timedelta(seconds=max([int(value) for value in limits] or [300]) * 100)
        query = f'''from(bucket: "{_flux_escape(self.telemetry_bucket)}")
  |> range(start: time(v: "{start.isoformat()}"))
  |> filter(fn: (r) => r._measurement == "device_telemetry")
  |> group(columns: ["device_id", "_time"])
  |> pivot(rowKey: ["_time"], columnKey: ["_field"], valueColumn: "_value")
  |> group(columns: ["device_id"])
  |> sort(columns: ["_time"], desc: true)
  |> limit(n: 1)'''
        try:
            tables = self._query_api().query(query=query, org=self.org)
        except Exception as exc:
            raise InfluxUnavailable("InfluxDB telemetry query failed") from exc

        latest = {}
        for table in tables:
            for record in table.records:
                values = record.values
                device_id = values.get("device_id")
                if device_id not in DEVICE_IDS:
                    continue
                observed = record.get_time()
                data = {key: value for key, value in values.items()
                        if key not in ("result", "table", "_start", "_stop", "_time", "_measurement", "_field", "_value", "device_id", "device_type", "site_id", "field_id", "dev_eui")}
                freshness = int(self.freshness_seconds.get(device_id, 300)) if isinstance(self.freshness_seconds, dict) else int(self.freshness_seconds)
                status = "stale" if observed is None or now - observed > timedelta(seconds=freshness) else "available"
                latest[device_id] = {
                    "device": device_id,
                    "status": status,
                    "observed_at": observed.astimezone(timezone.utc).isoformat().replace("+00:00", "Z") if observed else None,
                    "age_sec": max(0, int((now - observed).total_seconds())) if observed else None,
                    "data": data,
                }
        devices = []
        for device_id in DEVICE_IDS:
            devices.append(latest.get(device_id, {
                "device": device_id, "status": "unavailable", "observed_at": None,
                "age_sec": None, "data": {},
            }))
        statuses = {item["status"] for item in devices}
        overall = "unavailable" if statuses == {"unavailable"} else "stale" if "stale" in statuses or "unavailable" in statuses else "available"
        return {
            "generated_at": now.astimezone(timezone.utc).isoformat().replace("+00:00", "Z"),
            "status": overall,
            "devices": devices,
        }

    def operation_events(self, operation_id, start="-365d"):
        if not operation_id:
            raise ValueError("operation_id is required")
        if not re.fullmatch(r"-\d+[smhdw]", start):
            raise ValueError("start must be a bounded Flux duration")
        query = f'''from(bucket: "{_flux_escape(self.events_bucket)}")
  |> range(start: {start})
  |> filter(fn: (r) => r._measurement == "operation_event" and r.operation_id == "{_flux_escape(operation_id)}")
  |> pivot(rowKey: ["_time"], columnKey: ["_field"], valueColumn: "_value")
  |> sort(columns: ["_time"])'''
        try:
            tables = self._query_api().query(query=query, org=self.org)
        except Exception as exc:
            raise InfluxUnavailable("InfluxDB operation query failed") from exc
        events = []
        for table in tables:
            for record in table.records:
                event = dict(record.values)
                event["observed_at"] = record.get_time().astimezone(timezone.utc).isoformat().replace("+00:00", "Z")
                events.append(event)
        return events


def _flux_escape(value):
    return str(value).replace("\\", "\\\\").replace('"', '\\"')
