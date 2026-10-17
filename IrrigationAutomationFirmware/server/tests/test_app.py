from fastapi.testclient import TestClient

from irrigation_server import Settings, create_app


class EmptyStore:
    def list_plans(self, *args):
        return {"items": [], "next_cursor": None}


def test_health_auth_and_request_id():
    app = create_app(Settings(":memory:", "secret"), store=EmptyStore())
    client = TestClient(app)
    assert client.get("/api/v1/health").status_code == 401
    response = client.get("/api/v1/health", headers={"Authorization": "Bearer secret", "X-Request-ID": "trace-1"})
    assert response.status_code == 200
    assert response.json()["api_version"] == "v1"
    assert response.json()["request_id"] == "trace-1"
    assert response.json()["data"]["clock_synchronized"] is True


def test_unknown_path_uses_error_envelope():
    client = TestClient(create_app(Settings(":memory:", "secret"), store=EmptyStore()))
    response = client.get("/api/v1/nope", headers={"Authorization": "Bearer secret"})
    assert response.status_code == 404
    assert response.json()["error"]["code"] == "not_found"
    assert response.json()["api_version"] == "v1"
