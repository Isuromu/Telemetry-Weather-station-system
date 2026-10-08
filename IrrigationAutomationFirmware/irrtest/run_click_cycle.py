"""Real-click irrigation cycles.

Drives the Node-RED dashboard through actual DOM clicks in headless Chrome
(the operator path: ui-template -> socket.io -> sequencer), never through an
inject endpoint. Records per-cycle timings and the full state timeline.

usage: run_click_cycle.py --water 20 --cycles 3 [--route /dashboard/irrigation]
"""
import argparse
import json
import sys
import time

sys.path.insert(0, "/home/kamolovp/NX-devel/Electronics/Esp32/Projects/Telemetry-Weather-station/IrrigationAutomationFirmware/irrtest")
from cdp import Chrome, HOST, seq_state, pump_state, ts  # noqa: E402

irrigation_action = {
    "start": "document.querySelector('button.ir-start').click(); 'ok'",
    "stop": "document.querySelector('button.ir-stop').click(); 'ok'",
}


def snap():
    s = seq_state()
    p = pump_state()
    return {
        "phase": s.get("phase"),
        "notice": (s.get("notice") or "")[:90],
        "check": (s.get("check") or "")[:60],
        "fault": s.get("fault"),
        "pending": s.get("pending"),
        "pump_id": p.get("last_command_id"),
        "pump_pending": p.get("pending"),
        "pump_result": p.get("command_result"),
        "pump_state": p.get("run_state"),
        "pump_hz": p.get("actual_frequency_hz"),
        "pump_comm": p.get("communication_ok"),
        "pump_cfg_ok": p.get("configuration_valid"),
    }


def wait_for(cond, timeout, label, log, poll=1.0):
    t0 = time.time()
    last = None
    while time.time() - t0 < timeout:
        s = snap()
        key = (s["phase"], s["notice"], s["pending"])
        if key != last:
            print("   %s %-18s notice=%r pending=%s" % (ts(), s["phase"], s["notice"], s["pending"]), flush=True)
            last = key
        if cond(s):
            return time.time() - t0, s
        time.sleep(poll)
    return None, snap()


def run_cycle(ch, i, water_s, level_before):
    print("\n%s ===== CYCLE %d ===== water %s s =====" % (ts(), i, water_s), flush=True)
    before = snap()
    print("   pre: phase=%s check=%r pump(id=%s run=%s comm=%s cfg=%s)" % (
        before["phase"], before["check"], before["pump_id"], before["pump_state"],
        before["pump_comm"], before["pump_cfg_ok"]))
    if before["pump_cfg_ok"] is False:
        print("   !! ABORT: pump configuration_valid=false -> start will be refused. "
              "Run 'vfd config check' on the pump console.", flush=True)
        return None

    ch.js("window.confirm=()=>true")
    t0 = time.time()
    print("   %s CLICK Start on %s" % (ts(), ch.js("location.pathname")))
    ch.js(irrigation_action["start"])

    dt, s = wait_for(lambda s: s["phase"] == "running", 200, "running", True)
    if dt is None:
        print("   !! never reached running. phase=%s notice=%r" % (s["phase"], s["notice"]), flush=True)
        return {"cycle": i, "result": "start-failed", "snapshot": s}

    print("   %s RUNNING after %.0f s" % (ts(), dt))
    start_s = dt
    time.sleep(water_s)

    t1 = time.time()
    print("   %s CLICK Stop" % ts())
    ch.js(irrigation_action["stop"])

    # watch the teardown; note when we enter and leave wait_pump_stop
    pump_stop_s = None
    entered = None
    deadline = time.time() + 400
    while time.time() < deadline:
        s = snap()
        if s["phase"] == "wait_pump_stop" and entered is None:
            entered = time.time()
            print("   %s   wait_pump_stop entered" % ts(), flush=True)
        if entered and s["phase"] != "wait_pump_stop" and pump_stop_s is None:
            pump_stop_s = time.time() - entered
            print("   %s   wait_pump_stop left after %.0f s" % (ts(), pump_stop_s), flush=True)
        if s["phase"] == "idle":
            break
        time.sleep(1)

    s = snap()
    teardown_s = time.time() - t1
    print("   %s IDLE  teardown=%.0f s  pump_stop=%s  notice=%r" % (
        ts(), teardown_s, ("%.0f s" % pump_stop_s) if pump_stop_s else "n/a", s["notice"]))

    level_after = level_of(ch)
    return {
        "cycle": i, "result": "idle", "start_s": round(start_s),
        "pump_stop_s": round(pump_stop_s) if pump_stop_s else None,
        "teardown_s": round(teardown_s), "fault": s["fault"], "notice": s["notice"],
        "pump_id": s["pump_id"], "level_cm_before": level_before, "level_cm_after": level_after,
    }


RES_JS = ("(()=>{const t=(document.body.innerText||'').replace(/\\s+/g,' ');"
          "const i=t.indexOf('Reservoir');if(i<0)return null;"
          "const m=t.slice(i).match(/([\\d.]+)\\s*cm/);return m?parseFloat(m[1]):null;})()")


def level_of(ch):
    return ch.js(RES_JS)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--water", type=int, default=20)
    ap.add_argument("--cycles", type=int, default=3)
    ap.add_argument("--route", default="/dashboard/irrigation")
    ap.add_argument("--out", default="/home/kamolovp/NX-devel/Electronics/Esp32/Projects/Telemetry-Weather-station/IrrigationAutomationFirmware/irrtest/cycles.json")
    a = ap.parse_args()

    ch = Chrome()
    ch.goto(HOST + a.route)
    for _ in range(25):
        if ch.js("document.querySelector('button.ir-start') ? 'yes' : null"):
            break
        time.sleep(1)
    print("%s page ready on %s" % (ts(), ch.js("location.pathname")))
    print("%s check=%r" % (ts(), (seq_state().get("check") or "")[:80]))

    results = []
    lvl = level_of(ch)
    print("%s reservoir before: %s cm" % (ts(), lvl))
    for i in range(1, a.cycles + 1):
        r = run_cycle(ch, i, a.water, lvl)
        if r is None or r.get("result") != "idle":
            print("\n%s STOPPING: cycle %d did not complete cleanly." % (ts(), i))
            results.append(r)
            break
        results.append(r)
        lvl = r["level_cm_after"] or lvl
        # let the bus settle between cycles
        time.sleep(5)

    print("\n%s ================= TALLY =================" % ts())
    print("%-6s %-9s %-11s %-11s %-9s %s" % ("cycle", "start_s", "pump_stop_s", "teardown_s", "fault", "level cm"))
    for r in results:
        if not r:
            continue
        print("%-6s %-9s %-11s %-11s %-9s %s -> %s" % (
            r.get("cycle"), r.get("start_s"), r.get("pump_stop_s"), r.get("teardown_s"),
            r.get("fault"), r.get("level_cm_before"), r.get("level_cm_after")))
    json.dump(results, open(a.out, "w"), indent=2)
    print("wrote " + a.out)
    ch.close()


if __name__ == "__main__":
    main()
