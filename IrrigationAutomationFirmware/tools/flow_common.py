"""Shared helpers for the Node-RED dashboard flow builders.

Every dashboard flow in this repository is a build artifact: the builder in
`tools/build_<name>_dashboard.py` is the source of truth and the importable JSON
under `examples/<flow>/include/` must never be hand-edited. Run the builder
after changing it, then the matching `tools/test_<name>_dashboard.js`.

`write_flow` refuses to write a flow that breaks the identifier rule -- the
ChirpStack application ID and each DevEUI live in the Node-RED environment
(``IRRIGATION_APP_ID`` and ``<DEVICE>_DEV_EUI``), never in the export. The same
rule is checked across all flows by ``tools/test_flow_ids.js``.
"""
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# Workspace singletons every flow must reuse instead of shipping its own copy.
BROKER_ID = "ae0178f3742ff530"     # shared "chirpstack_mosquito" server
UI_BASE = "f53e93e9ba219e63"       # shared "My Dashboard" base
UI_THEME = "e49416861823a329"      # shared "MyTheme"

APP_ENV = "IRRIGATION_APP_ID"
DEVICE_ENV = {
    "MainValve": "MAIN_DEV_EUI",
    "PumpControl": "PUMP_DEV_EUI",
    "SoilNode": "SOIL_DEV_EUI",
    "WaterLevel": "WATER_DEV_EUI",
    "PressureControlNode": "VALVE1_DEV_EUI",
    "PressureControlNode2": "VALVE2_DEV_EUI",
}

_UUID = re.compile(r"['\"]([0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12})['\"]", re.I)
_DEV_EUI = re.compile(r"['\"]([0-9a-f]{16})['\"]", re.I)
_PLACEHOLDER = re.compile(r"\bSET_[A-Z0-9_]{3,}\b")
_SUBSCRIBE = re.compile(r"action\s*:\s*['\"]subscribe['\"]")
_BUILDS_TOPIC = re.compile(r"['\"`]application/")


def check_identifier_rule(nodes):
    """Raise SystemExit if a flow carries a ChirpStack identifier or placeholder.

    Mirrors tools/test_flow_ids.js so a bad flow fails at build time too.
    """
    problems = []
    for node in nodes:
        if node.get("type") == "mqtt-broker":
            continue
        for field in ("func", "topic", "payload"):
            text = str(node.get(field, ""))
            for match in _UUID.findall(text):
                problems.append("%s %s hardcodes application %s" % (node.get("id"), field, match))
            for match in _DEV_EUI.findall(text):
                problems.append("%s %s hardcodes DevEUI %s" % (node.get("id"), field, match))
            for match in _PLACEHOLDER.findall(text):
                problems.append("%s %s ships placeholder %s" % (node.get("id"), field, match))
        func = node.get("func", "")
        if node.get("type") == "function" and _BUILDS_TOPIC.search(func) and \
                APP_ENV not in func:
            problems.append("%s builds an application topic without %s" % (node.get("id"), APP_ENV))
    if problems:
        raise SystemExit("flow breaks the identifier rule:\n  " + "\n  ".join(problems))


def write_flow(nodes, relative_path, indent, trailing_newline=False):
    """Write a flow the way Node-RED's own export formats it, then report it."""
    check_identifier_rule(nodes)
    text = json.dumps(nodes, indent=indent, ensure_ascii=False)
    if trailing_newline:
        text += "\n"
    path = ROOT / relative_path
    # write_bytes rather than write_text(..., newline=""): the newline argument
    # only exists on Python 3.10+, and this box runs 3.8. Writing bytes keeps the
    # \n exactly as exported on every platform, which is what newline="" was for.
    path.write_bytes(text.encode("utf-8"))
    print("wrote %s (%d nodes)" % (path, len(nodes)))
    return path
