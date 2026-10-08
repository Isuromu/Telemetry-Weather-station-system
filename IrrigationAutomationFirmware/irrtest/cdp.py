"""Minimal Chrome DevTools Protocol client for drive-by real-click testing.

Talks to an already-running headless Chrome on the debug port and to the
Node-RED admin API on the dashboard host.
"""
import base64
import json
import time
import urllib.request

import websocket

DEBUG = "http://127.0.0.1:9222"
HOST = "http://192.168.2.217:1880"
AUTH = base64.b64encode(b"admin:admin").decode()


# ---------- Node-RED admin API ----------

def api_get(path):
    req = urllib.request.Request(HOST + path, headers={"Authorization": "Basic " + AUTH})
    return json.loads(urllib.request.urlopen(req, timeout=15).read().decode())


def flow_mem(flow_id):
    return api_get("/context/flow/" + flow_id).get("memory", {})


def _unwrap(v):
    if isinstance(v, dict) and "msg" in v:
        v = v["msg"]
    if isinstance(v, str):
        try:
            return json.loads(v)
        except ValueError:
            return v
    return v


SEQ_FLOW = "irrigation_single_page"
PUMP_FLOW = "e036313ddf417d8b"


def seq_state():
    """Sequencer state as the irrigation dashboard sees it."""
    return _unwrap(flow_mem(SEQ_FLOW).get("irrigation", {}))


def pump_state():
    return _unwrap(flow_mem(PUMP_FLOW).get("pump_state", {}))


# ---------- CDP ----------

class Chrome:
    def __init__(self, timeout=30):
        self.timeout = timeout
        self._id = 0
        self.ws = None
        self.reconnect()

    def reconnect(self):
        """Re-resolve the page target and open a fresh websocket.

        A page reload, or a slow response, can kill the socket; everything else
        in this client is stateless enough to just reconnect and carry on.
        """
        try:
            self.ws.close()
        except Exception:
            pass
        targets = [t for t in json.load(urllib.request.urlopen(DEBUG + "/json")) if t["type"] == "page"]
        if not targets:
            raise RuntimeError("no page target on " + DEBUG)
        self.ws = websocket.create_connection(targets[0]["webSocketDebuggerUrl"], timeout=self.timeout)
        self.cmd("Page.enable")
        self.cmd("Runtime.enable")

    def cmd(self, method, **params):
        for attempt in (1, 2):
            try:
                self._id += 1
                self.ws.send(json.dumps({"id": self._id, "method": method, "params": params}))
                while True:
                    msg = json.loads(self.ws.recv())
                    if msg.get("id") == self._id:
                        if "error" in msg:
                            raise RuntimeError("%s: %s" % (method, msg["error"]))
                        return msg
            except websocket.WebSocketTimeoutException:
                if attempt == 2:
                    raise
                self.reconnect()

    def js(self, expr):
        r = self.cmd("Runtime.evaluate", expression=expr, returnByValue=True, awaitPromise=True)
        res = (r.get("result") or {}).get("result", {})
        if res.get("subtype") == "error":
            raise RuntimeError("JS error: " + str(res.get("description"))[:300])
        return res.get("value")

    def goto(self, url):
        self.cmd("Page.navigate", url=url)

    def close(self):
        try:
            self.ws.close()
        except Exception:
            pass


def ts():
    return time.strftime("%H:%M:%S")
