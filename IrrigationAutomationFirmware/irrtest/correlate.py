"""Definitive downlink check: published, answered, or refused?

Reads a mosquitto_sub log written with `-F '%I %t %p'` (ISO arrival time,
topic, payload) and answers, per device: which command ids were published, did
the device ever echo them back, and was the echo an acceptance or a refusal?

The distinction matters because a device that refuses a command still echoes its
id, so "did the id come back" alone calls a refused open confirmed. For MainValve
the id is stored only when the command is accepted, so an id seen in
`reported_command_id` but never in `last_command_id` is a refusal or a failed
movement; the report's own phase and reason name it. A PCV reports a refusal
reason for a command it holds, and the pump stores an id only on acceptance.

Downlink payload shapes seen on this system:
  * valve_1 / valve_2  -> fPort 30, JSON `object` {"pcv": "open"|"close", "command_id": N}
  * main valve         -> fPort 30, base64 `data`, frame 01 01 <ang:2> <id:2>
  * pump               -> fPort 50, base64 `data`, frame 01 <op> <id:2> <arg:2>
                          op 03 = set frequency (arg = Hz*100), 04 = start, 01 = stop
The app-level downlink JSON has no `time`/`fCnt`/`deviceName` - only arrival
order and devEui - so ordering, not timestamps, drives the correlation.

usage: correlate.py irrtest/mqtt_run2.log [--since 15:49]
"""
import argparse
import base64
import json
import re

LINE = re.compile(r"^(\S+) (\S+) (.*)$")

PUMP_OP = {"03": "set_freq", "04": "start", "01": "stop"}

# Reasons a device reports when it did not carry the command out. MainValve names
# its phase and reason in the same uplink; a PCV reports status_reason; the pump
# reports command_result. Kept as one set so any of the three can be checked
# without a per-device branch.
REFUSAL_REASONS = {
    "invalid_command", "duplicate_command", "pressure_interlock",
    "pressure_sensor_error", "actuator_busy", "movement_timeout",
    "actuator_fault", "local_override", "modbus_error", "invalid_command_rejected",
    "pcv_actuation_failed", "lorawan_error",
}


def decode_down(fport, d):
    """Return (op, id, detail) or (None, None, why-not)."""
    obj = d.get("object")
    if isinstance(obj, dict):
        return obj.get("pcv"), obj.get("command_id"), ""
    b64 = d.get("data")
    if not b64:
        return None, None, "no data and no object"
    try:
        raw = base64.b64decode(b64)
    except Exception:
        return None, None, "bad base64"
    if len(raw) < 6:
        return None, None, "short frame %s" % raw.hex(" ")
    if fport == 50:
        return PUMP_OP.get("%02x" % raw[1], "op%02x" % raw[1]), int.from_bytes(raw[2:4], "big"), raw.hex(" ")
    if fport == 30:
        ang = int.from_bytes(raw[2:4], "big") / 10.0
        op = "open" if ang > 5 else "close"
        return op, int.from_bytes(raw[4:6], "big"), "%s %.1f deg" % (op, ang)
    return "op", int.from_bytes(raw[2:4], "big"), raw.hex(" ")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("log")
    ap.add_argument("--since", default=None, help="local HH:MM:SS cutoff")
    ap.add_argument("--json", action="store_true")
    a = ap.parse_args()

    names, downs, ups = {}, [], []
    for line in open(a.log, "rb"):
        s = line.decode("utf-8", "replace").rstrip("\n")
        m = LINE.match(s)
        if not m:
            continue
        t, topic, body = m.groups()
        if "/device/" not in topic or not body.startswith("{"):
            continue
        eui = topic.split("/device/")[1].split("/")[0]
        try:
            d = json.loads(body)
        except ValueError:
            continue
        di = d.get("deviceInfo") or {}
        if di.get("devEui"):
            names[di["devEui"]] = (di.get("deviceName") or di.get("deviceProfileName") or "?").strip()

        if topic.endswith("/command/down"):
            op, cid, detail = decode_down(d.get("fPort"), d)
            downs.append({"t": t[11:19], "eui": eui, "port": d.get("fPort"),
                          "op": op, "id": cid, "detail": detail})
        elif topic.endswith("/event/up"):
            o = d.get("object") or {}
            ups.append({"t": t[11:19], "eui": eui, "fcnt": d.get("fCnt"),
                        "reported": o.get("reported_command_id"), "last": o.get("last_command_id"),
                        "why": o.get("command_phase") or o.get("status_reason") or o.get("command_result")})

    def keep(r):
        return a.since is None or r["t"] >= a.since

    def verdict(dn, after):
        """(kind, uplink, why): accepted, refused or lost for this downlink."""
        for u in after:
            if dn["id"] == u["last"]:
                # The id is stored when the command is accepted, so this is an accept
                # unless the report itself still carries a refusal reason.
                if u["why"] in REFUSAL_REASONS:
                    return "refused", u, u["why"]
                return "accepted", u, u["why"]
        for u in after:
            if dn["id"] == u["reported"]:
                return "refused", u, u["why"]
        return "lost", None, None

    print("device map: " + ", ".join("%s=%s" % (k, v) for k, v in names.items()))
    print("\n%-9s %-9s %-6s %-10s %-6s %s" % ("arrived", "device", "port", "op", "id", "verdict"))
    lost = refused = total = 0
    for dn in downs:
        if not keep(dn):
            continue
        total += 1
        dev = names.get(dn["eui"], dn["eui"])
        if dn["id"] is None:
            print("%-9s %-9s %-6s %-10s %-6s UNPARSED (%s)" % (
                dn["t"], dev, dn["port"], dn["op"], "-", dn["detail"]))
            continue
        after = [u for u in ups if u["eui"] == dn["eui"] and u["t"] >= dn["t"]]
        kind, u, why = verdict(dn, after)
        if kind == "accepted":
            print("%-9s %-9s %-6s %-10s %-6s accepted (uplink %s fcnt=%s%s)" % (
                dn["t"], dev, dn["port"], dn["op"], dn["id"], u["t"], u["fcnt"],
                ", %s" % why if why else ""))
        elif kind == "refused":
            refused += 1
            print("%-9s %-9s %-6s %-10s %-6s *** ANSWERED AND REFUSED *** %s (uplink %s fcnt=%s)" % (
                dn["t"], dev, dn["port"], dn["op"], dn["id"], why or "-", u["t"], u["fcnt"]))
        else:
            lost += 1
            print("%-9s %-9s %-6s %-10s %-6s *** LOST *** (%d later uplinks, none mention it)" % (
                dn["t"], dev, dn["port"], dn["op"], dn["id"], len(after)))
    print("\n%d downlinks, %d refused, %d lost" % (total, refused, lost))
    if a.json:
        json.dump({"total": total, "refused": refused, "lost": lost, "down_links": downs},
                  open("irrtest/correlation.json", "w"), indent=2)


if __name__ == "__main__":
    main()
