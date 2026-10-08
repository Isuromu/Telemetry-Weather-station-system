"""Fast state poller. Writes a JSONL timeline of sequencer + pump state.

Sampling at 0.5 s is what makes a lost downlink visible: the pump's
`pending` goes non-null and either resolves to `command_result` or is
replaced by the next id with no result at all.
"""
import json
import sys
import time

sys.path.insert(0, "/home/kamolovp/NX-devel/Electronics/Esp32/Projects/Telemetry-Weather-station/IrrigationAutomationFirmware/irrtest")
from cdp import seq_state, pump_state  # noqa: E402

out_path = sys.argv[1] if len(sys.argv) > 1 else "irrtest/timeline.jsonl"
dur = float(sys.argv[2]) if len(sys.argv) > 2 else 1200.0
rate = 0.5

t0 = time.time()
last = None
n = 0
with open(out_path, "w") as fh:
    while time.time() - t0 < dur:
        try:
            s = seq_state()
            p = pump_state()
        except Exception as e:
            fh.write(json.dumps({"t": round(time.time() - t0, 2), "error": str(e)[:120]}) + "\n")
            fh.flush()
            time.sleep(rate)
            continue
        row = {
            "t": round(time.time() - t0, 2),
            "wall": time.strftime("%H:%M:%S"),
            "phase": s.get("phase"), "notice": (s.get("notice") or "")[:100],
            "check": (s.get("check") or "")[:80], "fault": s.get("fault"),
            "seq_pending": s.get("pending"), "step": s.get("step"),
            "p_id": p.get("last_command_id"), "p_reported": p.get("reported_command_id"),
            "p_pending": p.get("pending"), "p_result": p.get("command_result"),
            "p_run": p.get("run_state"), "p_hz": p.get("actual_frequency_hz"),
            "p_cmd_hz": p.get("commanded_frequency_hz"),
            "p_comm": p.get("communication_ok"), "p_cfg": p.get("configuration_valid"),
            "p_vfdfault": p.get("vfd_fault_code"), "p_stale": p.get("stale"),
            "p_cur": p.get("motor_current_a"),
        }
        key = json.dumps({k: v for k, v in row.items() if k not in ("t", "wall")}, sort_keys=True)
        if key != last:
            fh.write(json.dumps(row) + "\n")
            fh.flush()
            last = key
        n += 1
        time.sleep(rate)
print("poller done: %d samples -> %s" % (n, out_path))
