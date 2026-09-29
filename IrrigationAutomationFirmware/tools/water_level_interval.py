"""The WaterLevel node's reporting interval, and the freshness limit it implies.

`examples/WaterLevel/src/main.cpp` sends exactly one uplink per wake cycle and
then deep-sleeps, so the age of the newest uplink is the entire liveness signal
and a freshness limit means nothing except relative to that interval. Both
dashboard builders read the limit from here, so the WaterLevel card and the
IntegratedDashboard's water tile cannot drift apart.

`tools/test_water_level_dashboard.js` deliberately re-resolves the interval on
its own rather than importing this module: a check that mirrors the
implementation it is checking proves nothing.
"""

import pathlib
import re

ROOT = pathlib.Path(__file__).resolve().parents[1]

# Clear one whole cycle before suspecting the node: an unconfirmed Class A
# uplink is lost in normal operation and must not flag a healthy node.
MULTIPLIER = 2
# Below this, ordinary delivery jitter makes the flag flap.
FLOOR_SECONDS = 60


def resolve_sleep_seconds():
    """The sleep interval the firmware actually compiles with, in seconds.

    Resolution follows the compiler: a -D in platformio.ini defines the macro,
    so WaterLevelConfig.h's #ifndef leaves it alone. A commented-out flag on
    either side is ignored, and failing to find the value is an error rather
    than a silent default -- a limit derived from nothing would look fine.
    """
    ini = (ROOT / "platformio.ini").read_text(encoding="utf-8")
    header = (ROOT / "examples" / "WaterLevel" / "src" /
              "WaterLevelConfig.h").read_text(encoding="utf-8")
    m = re.search(r"^\s*-D\s*WATER_LEVEL_SLEEP_SECONDS=(\d+)", ini, re.M)
    if not m:
        m = re.search(r"#define\s+WATER_LEVEL_SLEEP_SECONDS\s+(\d+)", header)
    if not m:
        raise SystemExit("cannot resolve WATER_LEVEL_SLEEP_SECONDS from "
                         "platformio.ini or WaterLevelConfig.h")
    return int(m.group(1))


def stale_after_seconds():
    """Freshness limit in seconds for a device reporting on that interval."""
    return max(MULTIPLIER * resolve_sleep_seconds(), FLOOR_SECONDS)
