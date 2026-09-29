# WaterLevel end node

`WaterLevel` is the imported reservoir-level project with a telemetry-only
LoRaWAN Class A transport added around its existing measurement and load-control
logic.

Sensor manual facts, implemented settings and open hardware checks are separated
in [RD-RWG-01 reference notes](../../docs/RD_RWG_01.md).

Each wake cycle:

1. reads the GPIO35 battery divider;
2. reads the RD-RWG-01 on UART2 GPIO16/GPIO17: REG0002 (primary variable unit)
   and REG0003 (decimal places) tell the firmware how to interpret REG0004
   (measurement output value);
3. converts the measurement to metres of water column and to a percentage of a
   configured range — `RANGE_METERS`, currently **5 m** and **unconfirmed**.
   The sensor manual states a 0-10 m level range, so if the installed unit is
   10 m the percentage reads half the true fill. Depth and pressure are measured
   and unaffected. Before production this has to be set for the installed
   sensor and for the pool it serves; see the
   [RD-RWG-01 notes](../../docs/RD_RWG_01.md);
4. applies the existing GPIO27 load rule — battery at least 11.5 V. The level
   gate (at least 15 percent) is commented out in `main.cpp`, so the output does
   not currently depend on the water level;
5. sends one FPort 40 uplink and completes RX1/RX2;
6. enters timer deep sleep while holding the selected GPIO27 output state.

Application downlinks are deliberately ignored. This first network integration
does not add remote load control or modify the existing threshold policy.

## Build

```text
pio run -e water_level
```

The build sleeps for **10 seconds** — the header fallback — so bench work does
not wait a quarter of an hour per uplink. The **deployment interval is 900
seconds**, prepared but not yet enabled: the flag sits commented out in the
`env:water_level` section of the repository `platformio.ini`.

### Switching to the deployment interval

Two steps, in this order:

1. Uncomment the `-D WATER_LEVEL_SLEEP_SECONDS=900` line in `platformio.ini`,
   or change the number to another interval (10 through 86400 seconds).
2. Regenerate **both** flows and rebuild:

   ```text
   python tools/build_water_level_dashboard.py
   python tools/build_irrigation_dashboard.py
   node tools/test_water_level_dashboard.js
   node tools/test_irrigation_dashboard.js
   pio run -e water_level
   ```

Step 2 is not optional, and neither builder may be skipped — both dashboard flows
derive their freshness limit from this same interval. The WaterLevel card exposes
its number; the IntegratedDashboard *acts* on its own, because the sequencer reads
it through `fresh('water', …)` to decide whether the node is alive enough to
water on. A flow generated at 10 s and firmware built at 900 s would call a
healthy node stale every cycle, so the tile would read stale between uplinks and
the sequencer would refuse to start a run. Skip the rebuild and the firmware keeps
the short interval. Both checks fail loudly if the generated limit and the sleep
interval ever disagree, which is the point of running them. See
[Staleness](#staleness).

## OTAA credentials

Copy `config/WaterLevelLoRaSecrets.example.h` to
`config/WaterLevelLoRaSecrets.h`, set `CONFIGURED` to `true`, and enter the
WaterLevel node's own JoinEUI, DevEUI, and AppKey. The destination file is
Git-ignored. A build without credentials remains valid: it measures, applies
the load rule, reports the missing credentials over Serial, and sleeps without
starting OTAA.

The radio pinout and EU868 configuration match `PressureControlNode`:

- NSS GPIO5, DIO1 GPIO26, RESET GPIO14, BUSY GPIO25;
- SPI SCK GPIO18, MISO GPIO19, MOSI GPIO23;
- TX enable GPIO32 and RX enable GPIO33.

Use `include/water_level_class_a_codec.js` as the ChirpStack device profile
codec.

## Uplink payload

FPort 40 carries a 10-byte big-endian payload:

| Offset | Size | Meaning |
| --- | ---: | --- |
| 0 | 1 | Protocol version (`1`) |
| 1 | 1 | Flags: bit 0 pressure valid, bit 1 load on |
| 2 | 2 | Battery voltage in millivolts |
| 4 | 2 | Signed pressure in millibar |
| 6 | 2 | Depth in millimetres |
| 8 | 2 | Water level in 0.1 percent |

## Node-RED dashboard

Import `include/water_level_dashboard_flow.json` through Node-RED's **Import**
menu. The file deliberately carries no flow tab of its own, so use the
dialog's **Import to** list to pick the flow the nodes should join — importing
never creates a second `Water Level` tab. Install Dashboard 2.0
(`@flowfuse/node-red-dashboard`) if its `ui-*` nodes are unavailable. The flow
adds a `Water Level` page at `/dashboard/water-level` with two panels:

- **Water Level** — water level, depth, pressure, battery, load output and
  signal, followed by the uplink counter, FCnt, data rate, gateway and the
  latest network event;
- **Water Level history** — line charts of water depth in centimetres and
  pressure in bar, each keeping a 24-hour window.

Small **+** / **−** buttons to the right of each chart change that chart's
visible time range independently (30 minutes to 24 hours, initially 6 hours).
They only change the view: they do not alter the node's reporting interval or
discard the chart's stored history, and the two charts zoom separately.

The flow follows the `MainValve` (valve_1) structure — one ChirpStack event
input, one filter/decode function, one card — plus the two history charts. It is
telemetry only: there is no downlink node, because the firmware deliberately
ignores application downlinks. Nothing on the page can command the node.

### Updating an existing import

Re-import after every regeneration — the decode rules, the card markup and the
freshness limit all live in the JSON, and hand-editing the imported flow leaves
the file and the workspace out of step. Either route works:

- **Delete and re-import** (tidiest). Select the previous import — the
  `Water Level` page, both groups and the flow's nodes — and delete it, leaving
  the shared `My Dashboard` base and `MyTheme` alone. Delete the page, its
  groups and its widgets together, then import before deploying: a page removed
  on its own leaves its widgets unresolvable and the deploy fails. This route
  recreates the page from the file, so it lands at `/dashboard/water-level`
  rather than keeping an auto-generated path.
- **Import over the top.** Paste and accept the dialog's defaults. Nodes whose
  ids already exist are ticked for **replace**, so the functions and card come
  through with the new code; the page and groups arrive unticked and are
  skipped, so whatever your canvas already has — a hand-set path, page order,
  theme — survives. Never pick **Import copy** for those; that is what mints a
  second page.

Confirm it took by reading the card footer: it names the freshness limit the
flow is actually using (*Freshness limit: 60 s* at the bench interval, 1800 s
after the deployment switch). A limit that does not match the firmware's sleep
interval is the one failure this re-import exists to prevent.

### Configuration

Set the same Node-RED environment variables the integrated dashboard uses, so
one `settings.js` block configures both flows:

| Variable | Value |
| --- | --- |
| `IRRIGATION_APP_ID` | the shared ChirpStack application ID |
| `WATER_DEV_EUI` | this node's DevEUI, from `config/WaterLevelLoRaSecrets.h` |

The MQTT input subscribes dynamically to
`application/<IRRIGATION_APP_ID>/device/<WATER_DEV_EUI>/event/+`. Both the
subscribe and decode functions read the variables at run time; nothing is
hardcoded in the flow. Until both are valid the card shows **Not configured**,
naming the missing variable in red, and the subscribe node's status stays red.
The `Refresh subscription` inject re-reads them every 30 seconds, so correcting
the environment and redeploying is enough.

### What the import adds, and what it borrows

The flow is built so that importing it needs no hand cleanup afterwards.
Everything shared is referenced by id instead of being re-declared, so the
editor reuses what is already there rather than adding a second copy:

| Config node | Behaviour on import |
| --- | --- |
| `My Dashboard` base, `MyTheme` | referenced by id, reused |
| `chirpstack_mosquito` broker | referenced by id, **no broker node is shipped** |
| `Water Level` page and its two groups | shipped, under the ids your workspace already uses |

The page and groups are the only config nodes the flow genuinely adds, and they
carry the *existing* workspace ids rather than fresh ones. That is deliberate:
Node-RED preserves incoming ids on import and raises an import conflict when an
id is already taken, and a clashing config node starts **unticked** in that
dialog — so the page and groups you already have win, and nothing is duplicated.
An id matching nothing is imported as-is instead, which is what leaves a second
`Water Level` page and a second pair of groups to delete by hand.

If the import dialog appears, leave the ticked-but-conflicting entries alone and
import the rest; the config nodes are listed under their own heading at the
bottom.

The broker choice matters most: the flow points at the same
`chirpstack_mosquito` broker `MainValve` uses, and deliberately ships no
`mqtt-broker` node of its own. Shipping one would arrive as a second server to
delete by hand, and re-declaring the shared id could overwrite the host, port,
authentication and TLS settings already configured for your ChirpStack
installation. Configure the broker once, in the flow that already owns it.

The page reuses the shared base, so it appears beside the Valve, Pump and
SoilNode pages in the same navigation menu.

If a `Water Level` page or group is already on the canvas under a *different*
id, delete it before importing — for example the leftovers of an earlier import
made before the ids were pinned. Otherwise the flow's page and that leftover
both stay, and so do the duplicates.

### Regenerating

The importable JSON is generated. After changing the decode rules or the card
markup, run:

```text
python tools/build_water_level_dashboard.py
node tools/test_water_level_dashboard.js
```

The generator rewrites the JSON; the check re-runs the decoder against the
recorded FPort 40 uplink and verifies the wiring, the chart feeds, the
environment-variable handling, and the import invariants — no flow tab, no
broker config node, and the shared broker id still matching `MainValve`.

### Staleness

The node only reports when it wakes, so a failed node and a sleeping node look
identical on the page. A watchdog makes the difference visible, following
`PumpControl`: an inject node fires once two seconds after deploy and then every
15 seconds into a **Check stale status** function, which measures how long ago
the last uplink arrived and compares it against a freshness limit derived from
the firmware's sleep interval.

That rule (twice the sleep interval, floor 60 s) lives in
`tools/water_level_interval.py`, and the IntegratedDashboard builder imports it
too so its water tile cannot drift from this card. Both validators re-resolve the
interval independently and fail if the number in the JSON disagrees.

Past the limit:

- the status pill turns amber and reads **No recent uplink**;
- an amber banner names the age and the limit — *No uplink for 34 min (limit
  1800 s)* — alongside the time of the last report;
- the readings stay on screen but dim to half opacity, because the last known
  reservoir level is still useful context;
- **Signal** reads `Unknown` instead of showing the RSSI and SNR of a link that
  may no longer exist;
- before the first uplink ever arrives the pill reads **Waiting for uplink**.
  The card starts in the stale state, so it never presents old data as current.

Age is measured from when the uplink reached Node-RED, not from the gateway
timestamp in the payload, so clock skew between the gateway and the Node-RED
host cannot make a live node look old. Any `up` uplink clears the flag. A join,
log or status event does not: those prove the radio is alive but are not the
reading this page waits for.

The limit is **twice the firmware's sleep interval**, never less than 60
seconds, and the builder reads that interval out of `platformio.ini` or
`WaterLevelConfig.h` at generation time rather than hardcoding a number:

| Sleep interval | Source | Freshness limit | A stopped node is flagged within | Shipped flow now |
| --- | --- | --- | --- | --- |
| 10 s | `WaterLevelConfig.h` (bench fallback) | 60 s | 75 s | yes |
| 900 s | `platformio.ini`, commented out (deployment) | 1800 s | ~30 min | after the switch |

The IntegratedDashboard ships the same number in its `waterMaxAgeSec` — 60 s
now, 1800 s after the switch — so the sequencer gets the same picture of the
node the card shows.

The limit has to move with the interval, because the node sends exactly one
uplink per wake cycle and then sleeps. A fixed limit is wrong at both ends: the
1200 seconds it used to be would leave a dead node reading fresh through 120
missed reports at the bench's 10-second setting, while at the deployment's 900
seconds it covered barely one cycle, so a single lost uplink — normal for an
unconfirmed Class A uplink — would paint a healthy node amber. Two intervals
absorbs one lost report exactly, and a node that stops is flagged at its second
missed cycle.

`tools/test_water_level_dashboard.js` re-resolves the interval the same way and
fails if the limit does not clear a whole cycle, if it waits longer than three,
or if the 15-second watchdog is too coarse to notice in time — so the limit, the
firmware interval and the watchdog cannot drift apart silently.

This is device staleness only. Unlike `MainValve`'s **Show missing final
result**, it times out nothing the page sent, because this flow sends nothing.

### Behaviour notes

Only a valid reading is charted. When the payload's pressure-valid flag is
clear, depth, water level and pressure become `null`, the card says so, and the
charts receive no point at all — a gap in the chart means the sensor reported
invalid, not a zero reading. Battery and the network fields still update.

If the ChirpStack device profile has no codec installed, or the codec returns an
error, the decoder falls back to the documented ten-byte layout. The card's
**Decoded by** field then reads `raw payload` instead of `codec`, which is the
signal that the device profile needs its codec installed. If the fallback also
fails, the card shows the reason in red rather than a stale reading.

`IntegratedDashboard` already carries a small WaterLevel card for the shared
irrigation sequencer. This standalone flow is the dedicated node page; the two
can read the same uplinks at the same time without conflict, but only the
integrated flow issues commands to the other devices.

The imported standalone `platformio.ini` is retained as historical source
material. Builds integrated with this repository use the root `water_level`
environment.
