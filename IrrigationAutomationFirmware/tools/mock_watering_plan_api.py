#!/usr/bin/env python3
"""HTTP adapter over the reference fake watering-plan backend (TEMPORARY).

The protocol and the lifecycle are not implemented here. They come from
``fake_watering_plan_backend`` (which runs the real store, scheduler and
service against a virtual clock) and, behind that, from
``docs/integrations/watering-plan/api-protocol.rules.json``. This module only
exposes that backend over the /api/v1 HTTP contract for the mobile app, and
keeps two test-only ``X-Mock-Scenario`` hooks that the app must never send.

Run it for a phone on the same Wi-Fi:

    python3 tools/mock_watering_plan_api.py --host 0.0.0.0 --real-time
"""

import argparse
import json
import threading
from http.server import ThreadingHTTPServer

from fake_watering_plan_backend import FakeBackend
from watering_plan_api import WateringPlanHandler

TOKEN = "test-token"
MOCK_SCENARIOS = ("drop-after-store", "internal-error")


class MockHandler(WateringPlanHandler):
    server_version = "WateringPlanFake/1.0"

    def _serve(self):
        scenario = self.headers.get("X-Mock-Scenario")
        length = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(length) if length else b""
        if scenario == "internal-error" and self.command == "PUT":
            return self._write(500, self._envelope_error("internal_error", "Simulated internal failure"))
        status, payload = self.service.handle(self.command, self.path, dict(self.headers), body)
        if scenario == "drop-after-store" and self.command == "PUT" and status in (200, 201):
            # Simulate a lost response after the plan was stored: the client sees
            # the connection close and must retry with the same identity.
            self.close_connection = True
            return
        return self._write(status, payload)

    def _envelope_error(self, code, message):
        request_id = self.headers.get("X-Request-ID") or "mock-generated-request-id"
        return {"api_version": "v1", "request_id": request_id,
                "error": {"code": code, "message": message, "details": []}}

    def _write(self, status, payload):
        raw = json.dumps(payload, separators=(",", ":")).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(raw)))
        self.end_headers()
        self.wfile.write(raw)

    def log_message(self, _format, *_args):
        return


# The base class binds these names to its own _serve at class-creation time, so
# the override above only takes effect if the aliases are rebound here.
MockHandler.do_GET = MockHandler._serve
MockHandler.do_POST = MockHandler._serve
MockHandler.do_PUT = MockHandler._serve


def make_server(host="127.0.0.1", port=8088, backend=None, real_time=False):
    backend = backend or FakeBackend(token=TOKEN)
    handler = type("BoundMockHandler", (MockHandler,), {"service": backend.service})
    server = ThreadingHTTPServer((host, port), handler)
    server.backend = backend
    server.real_time_stop = None
    if real_time:
        stop = threading.Event()

        def loop():
            while not stop.wait(1.0):
                backend.advance(1)

        threading.Thread(target=loop, name="fake-clock", daemon=True).start()
        server.real_time_stop = stop
    return server


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8088)
    parser.add_argument("--real-time", action="store_true",
                        help="advance the virtual clock once per second so plans start and finish")
    args = parser.parse_args()
    server = make_server(args.host, args.port, real_time=args.real_time)
    print("Reference fake backend at http://%s:%d/api/v1 (token: %s)" % (args.host, args.port, TOKEN))
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        if server.real_time_stop:
            server.real_time_stop.set()
        server.server_close()
        server.backend.close()


if __name__ == "__main__":
    main()
