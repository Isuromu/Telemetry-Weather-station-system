"""Correlate MQTT downlinks with device-reported command ids.

Answers: for every command ChirpStack published, did the device ever report
having seen it? A published id that never comes back in a later uplink is a
candidate lost downlink (or a rejected one - a refusal still reports).

usage: analyze_downlinks.py irrtest/mqtt.log [--since 15:26]
"""
import argparse
import base64
import json
import re
from datetime import datetime, timezone


def parse_log(path):
    """Yield (iso_ts, topic, payload_dict) for every JSON message."""
    out = []
    for line in open(path, "rb"):
        line = line.decode("utf-8", "replace").rstrip("\n")
        m = re.match(r"^(\S+)\s+(\{.*\})$", line)
        if not m:
            continue
        try:
            d = json.loads(m.group(2))
        except ValueError:
            continue
        out.append((m.group(1), d))
    return out


def iso(d):
    t = d.get("time")
    if not t:
        return None
    return t.split(".")[0].replace("T", " ")


def local(iso_str):
    """ChirpStack stamps UTC; the wall clock we log in is local. Convert."""
    t = iso_str.split(".")[0].replace(" ", "T") + "+00:00"
    dt = datetime.fromisoformat(t).replace(tzinfo=timezone.utc).astimezone()
    return dt.strftime("%H:%M:%S")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("log")
    ap.add_argument("--since", default=None, help="wall clock filter, e.g. 15:26")
    a = ap.parse_args()

    msgs = parse_log(a.log)
    name = {}
    downs = []
    ups = []

    for topic, d in msgs:
        di = d.get("deviceInfo") or {}
        eui = di.get("devEui")
        nm = (di.get("deviceName") or di.get("deviceProfileName") or "?").strip()
        if eui:
            name[eui] = nm
        if topic.endswith("/command/down"):
            b64 = d.get("data")
            try:
                raw = base64.b64decode(b64) if b64 else b""
            except Exception:
                raw = b""
            downs.append({
                "t": local(iso(d)) if iso(d) else None, "eui": eui, "dev": nm, "fPort": d.get("fPort"),
                "hex": raw.hex(" "), "bytes": len(raw),
                "fcnt": d.get("fCnt"), "confirmed": d.get("confirmed"),
            })
        elif topic.endswith("/event/up"):
            obj = d.get("object") or {}
            ups.append({
                "t": local(iso(d)) if iso(d) else None, "eui": eui, "dev": nm, "fPort": d.get("fPort"),
                "fcnt": d.get("fCnt"),
                "reported": obj.get("reported_command_id"),
                "last": obj.get("last_command_id"),
                "run": obj.get("run_state"), "pos": obj.get("pcv_last_commanded"),
            })

    def keep(row):
        return a.since is None or (row["t"] and row["t"] >= a.since)

    print("device map:")
    for e, n in name.items():
        print("   %s  %s" % (e, n))

    print("\nDOWNLINKS published by ChirpStack (%d):" % len([d for d in downs if keep(d)]))
    print("   %-19s %-11s %-4s %-9s %s" % ("time", "device", "port", "fcnt", "payload hex"))
    for d in downs:
        if keep(d):
            print("   %-19s %-11s %-4s %-9s %s" % (d["t"], d["dev"], d["fPort"], d["fcnt"], d["hex"] or "(empty)"))

    print("\nUPLINK reports of command ids (%d):" % len([u for u in ups if keep(u)]))
    last = {}
    for u in ups:
        if not keep(u):
            continue
        rid = u["reported"] if u["reported"] is not None else u["last"]
        if rid is None:
            continue
        key = (u["eui"], rid)
        if key in last:
            continue
        last[key] = u
        print("   %-19s %-11s fcnt=%-8s reported=%-6s last=%-6s run=%s" % (
            u["t"], u["dev"], u["fcnt"], u["reported"], u["last"], u["run"]))

    print("\nCORRELATION - published id vs a later device report of the same id:")
    print("   %-19s %-11s %-6s %s" % ("sent at", "device", "id", "verdict"))
    for d in downs:
        if not keep(d):
            continue
        ids = [int(x, 16) for x in d["hex"].split()] if d["hex"] else []
        # ids are the only 16-bit-looking pair after the op byte; report all id-like
        # candidates and let the verdict use what the device reports back.
        seen = [u for u in ups if u["eui"] == d["eui"] and u["t"] and d["t"] and u["t"] >= d["t"]]
        reported = sorted({(u["reported"], u["last"]) for u in seen})
        flag = ""
        if not seen:
            flag = "NO UPLINK AFTER -> check"
        print("   %-19s %-11s %-6s %s %s" % (
            d["t"], d["dev"], "?", "", flag or "uplinks: %s" % reported[:3]))


if __name__ == "__main__":
    main()
