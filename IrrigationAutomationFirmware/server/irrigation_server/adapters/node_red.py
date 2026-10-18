"""Bounded HTTP client for the Node-RED irrigation adapter."""

from urllib.parse import urljoin


class NodeRedError(Exception):
    def __init__(self, code, message, status_code=None):
        super().__init__(message)
        self.code = code
        self.status_code = status_code


class NodeRedAdapter:
    def __init__(self, base_url, token, timeout_seconds=3.0, client=None):
        if timeout_seconds <= 0 or timeout_seconds > 30:
            raise ValueError("timeout_seconds must be in (0, 30]")
        self.configured = bool(base_url and token)
        self.base_url = base_url.rstrip("/") + "/" if base_url else ""
        self.token = token
        self.timeout_seconds = timeout_seconds
        self.client = client
        if self.configured and self.client is None:
            import httpx
            self.client = httpx.Client(timeout=timeout_seconds)

    def close(self):
        close = getattr(self.client, "close", None)
        if close:
            close()

    def start(self, operation_id, valves, plan_id=None, plan_revision=None):
        if not operation_id or not isinstance(operation_id, str) or len(operation_id) > 128:
            raise ValueError("operation_id must be a non-empty string of at most 128 characters")
        if not isinstance(valves, dict) or not any(valves.values()):
            raise ValueError("at least one valve must be selected")
        if set(valves) - {"valve1", "valve2"} or any(type(value) is not bool for value in valves.values()):
            raise ValueError("valves must contain boolean valve1/valve2 values")
        payload = {"action": "start", "operation_id": operation_id, "valves": valves}
        self._add_plan_metadata(payload, plan_id, plan_revision)
        return self._request(payload)

    def stop(self, operation_id, plan_id=None, plan_revision=None):
        if not operation_id or not isinstance(operation_id, str) or len(operation_id) > 128:
            raise ValueError("operation_id must be a non-empty string of at most 128 characters")
        payload = {"action": "stop", "operation_id": operation_id}
        self._add_plan_metadata(payload, plan_id, plan_revision)
        return self._request(payload)

    @staticmethod
    def _add_plan_metadata(payload, plan_id, plan_revision):
        if plan_id is not None:
            if not isinstance(plan_id, str) or not plan_id or len(plan_id) > 128:
                raise ValueError("plan_id must be a non-empty string")
            payload["plan_id"] = plan_id
        if plan_revision is not None:
            if type(plan_revision) is not int or plan_revision < 1:
                raise ValueError("plan_revision must be a positive integer")
            payload["plan_revision"] = plan_revision

    def _request(self, payload):
        if not self.configured:
            raise NodeRedError("adapter_unavailable", "Node-RED adapter is not configured")
        try:
            response = self.client.post(
                urljoin(self.base_url, "internal/irrigation/command"),
                json=payload,
                headers={"Authorization": "Bearer " + self.token},
                timeout=self.timeout_seconds,
            )
        except Exception as exc:
            raise NodeRedError("adapter_timeout", "Node-RED adapter request failed") from exc
        if response.status_code >= 500:
            raise NodeRedError("adapter_unavailable", "Node-RED adapter unavailable", response.status_code)
        try:
            body = response.json()
        except Exception as exc:
            raise NodeRedError("adapter_invalid_response", "Node-RED adapter returned invalid JSON", response.status_code) from exc
        if response.status_code >= 400:
            error = body.get("error", {})
            raise NodeRedError(error.get("code", "adapter_rejected"), error.get("message", "Command rejected"), response.status_code)
        data = body.get("data") if isinstance(body, dict) else None
        if not isinstance(data, dict) or data.get("operation_id") != payload["operation_id"]:
            raise NodeRedError("adapter_invalid_response", "Node-RED adapter response did not match operation")
        return data
