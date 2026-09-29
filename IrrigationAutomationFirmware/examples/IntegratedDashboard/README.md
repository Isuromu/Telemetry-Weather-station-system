# Single-page irrigation dashboard

Import `irrigation_dashboard_flow.json` into Node-RED with Dashboard 2 installed.
For every update, use the complete JSON.

## Updating an existing import

Import straight over the top. The flow ships no `tab` node and every node id is
the one the live canvas already uses, so the editor replaces each node in place
and adds nothing — no tab, no second page or group, and no duplicate `mqtt in`
(a second uplink node would send every downlink twice). Leave the import
dialog's defaults alone: ordinary nodes tick for replace, and the config nodes
(`ui-base`, `ui-theme`, `global-config`) start unticked and are skipped, keeping
the workspace copies. Never choose **Import copy**. The dialog's "Import to"
list decides which flow the nodes join — pick the existing
`irrigation_single_page` flow.

The first install on a fresh canvas is the same import: the flow brings its own
page, group, `ui-base` and `ui-theme`. Site-specific wiring lives in Node-RED
environment variables rather than in this JSON, so a re-import does not disturb
it; the settings *function* is regenerated, so edit limits in the builder (below).

To start clean instead, delete the old nodes in the editor and import — but
delete the page and its groups together with the nodes, and import before
deploying. A group whose page is gone makes the deploy fail with
`Cannot read properties of null (reading 'getBase')` (see below).

The flow contains one page, one compact HTML `ui-template`, and one sequencer
for WaterLevel, SoilNode, MainValve, valve_1, valve_2, and PumpControl. It uses
the workspace's shared Dashboard 2 base (`My Dashboard`) and theme, and adds one
page at `/irrigation` beside the other project pages, so the dashboard is at
`/dashboard/irrigation` rather than on a base of its own.

Set one shared ChirpStack application ID as `IRRIGATION_APP_ID`, then set
these six Node-RED DevEUI environment variables before enabling commands:

| Device | DevEUI variable |
| --- | --- |
| WaterLevel | `WATER_DEV_EUI` |
| SoilNode | `SOIL_DEV_EUI` |
| MainValve | `MAIN_DEV_EUI` |
| valve_1 | `VALVE1_DEV_EUI` |
| valve_2 | `VALVE2_DEV_EUI` |
| PumpControl | `PUMP_DEV_EUI` |

The `Subscribe to configured ChirpStack application` function uses that ID to
subscribe to `application/<IRRIGATION_APP_ID>/device/+/event/up`; the MQTT input
must be in Dynamic subscription mode. Existing deployments should update that
node by replacing the complete flow as described above.

The flow references the existing `chirpstack_mosquito` broker and ships no
broker node of its own, so importing cannot add a second server or overwrite the
host, port and credentials already set on the canvas. If that server does not
exist in your Node-RED instance, create it, or repoint the imported `mqtt in`
and `mqtt out` nodes at the one you use. The flow uses the corresponding
ChirpStack codec's decoded
`object` in each uplink. WaterLevel uses FPort 40, SoilNode FPort 10, MainValve
and field valves FPort 31, and PumpControl FPort 51. For PumpControl, the flow
also has a raw payload fallback for both the 17-byte protocol-v1 and 22-byte
protocol-v2 status formats. Commands use the existing FPort 30 main/field
valve formats and FPort 50 pump format.

After importing, deploy the flow and open
`/dashboard/irrigation` on the Node-RED host. Six device cards
should appear even before an uplink arrives. The header shows MQTT status;
the cards show `STALE` until valid uplinks are received. If every card remains
stale, check the MQTT connection, `IRRIGATION_APP_ID`, the six DevEUIs, and
the ChirpStack codecs. The shared application ID and DevEUIs are read through
Node-RED environment variables; entering them elsewhere in the dashboard
does not configure the flow.

If Node-RED reports `Cannot read properties of null (reading 'getBase')` from
`UIGroupNode`, the deployed `Irrigation` UI group points to a page config node
that is missing in that Node-RED instance. In Dashboard 2.0's Layout panel,
reselect or create the `Irrigation` page under the intended UI base, then set
the `Irrigation` group's Page to it and Deploy. Remove any orphaned duplicate
group from an older partial import. The generated JSON includes the complete
group → page → base chain; `tools/test_irrigation_dashboard.js` checks it.

**All adjustable limits and timeouts are in the `Irrigation settings — edit
limits here` function node** — but that node is generated, so edits made in the
editor are lost on the next import. Change `tools/build_irrigation_dashboard.py`
and regenerate instead. The initial values include the requested 20 cm start and
19 cm stop water levels, 70% soil start and 90% soil stop values, and 45–90°
main valve open range. Freshness limits are 20 minutes for SoilNode (its intended
600-second reporting interval doubled), 150 seconds for MainValve, for each field
valve 45 seconds while that valve is open and three minutes while it is closed,
and for PumpControl 45 seconds while its last report said it was running and
180 seconds while stopped, except on the stop sequence, which trusts a stopped
report only if it is under 75 seconds old. The tick inject node refreshes safety
checks every five seconds.

Each limit has to clear twice its device's reporting interval, or one lost uplink
marks a healthy node stale. MainValve is the worked example: it is Class C, and
its heartbeat used to be a fixed 300 seconds (`STATUS_INTERVAL_MS` in
`examples/MainValve/src/main.cpp`), so the 150 seconds this flow carries could
never clear two uplinks — it stopped every run about 2.5 minutes in and
intermittently refused to start. The limit is the safety check, so the heartbeat
was shortened rather than the limit loosened: it is now **60 seconds**, which
150 seconds clears twice with margin. That is 60 uplinks an hour instead of 12;
at MainValve's 18-byte payload that is roughly 0.1% of EU868's 1% duty cycle at
SF7–SF10, and over budget only if ADR settles at SF11 or SF12. Any future change
to either number has to keep the heartbeat at or below twice the limit.

The field valves are split by state, mirroring the PumpControl running/stopped
pair: `valveOpenMaxAgeSec` applies while the last reported command for that valve
was `open`, `valveClosedMaxAgeSec` while it was `close`. That is what lets the
open half be tight — three of the 15-second cadence a PCV keeps while its valve
is open (`VALVE_OPEN_REPORT_INTERVAL_SECONDS` in `PressureNodeConfig.h` and
`PressureNode2Config.h`) — without the closed half flagging a healthy idle valve.
Each valve runs its own firmware, so `tools/test_irrigation_dashboard.js`
resolves that cadence from *both* headers and fails if they disagree, because one
limit cannot fit two different cadences. The closed half assumes the production
60-second interval; an operator who sets a longer interval by downlink moves the
node off that assumption and this limit has to be raised to match.

`fieldValveCommandTimeoutSec` (300 s) carries that same assumption, for the same
reason: an open command waits for the node's next wake, so the deadline has to
clear the interval the node is actually sleeping. It is a failure detector rather
than a retry — neither `wait_field_open` nor `wait_field_close` re-sends — so it
is only how long a sequence that looks frozen runs before the operator is told
the downlink was lost. At the 2100 seconds it used to carry that was 35 minutes,
with the main valve open, in both the open and the close case.

**Flash both valves before importing this flow.** The 45-second open limit is
shorter than the 60-second cadence the valves kept before that firmware change,
so importing first exacts the failure it was written to prevent: while a valve is
open the running check stops every run about 45 seconds in, and the valves are at
that moment still reporting normally. The limit is only correct once both nodes
report on the new cadence.

While a valve is open the PCV reports every 15 seconds instead of on its
configured interval — an open valve is passing water, and in Class A a downlink
can only arrive in the RX window after an uplink, so that interval is also the
worst case before a close command reaches the valve. Both PCV firmwares share the
same schedule helper, so every variant reports faster while open; the two
low-power builds (`pio run -e pcv_low_power_class_a` and
`-e pcv_low_power_class_a_without_flowmeter`) additionally cut their deep-sleep
timer to match, which is what makes the change matter for the battery-powered
valves.

PumpControl needs three limits rather than two, because its stopped state is asked
two different questions. `pumpOnlineMaxAgeSec: 180` is the liveness one — *is the
pump talking to us at all?* — and it is what the Start gate and the card read. At
the 60-second stopped heartbeat (`STATUS_INTERVAL_STOPPED_MS` in
`examples/PumpControl/src/main.cpp`) that is three uplinks, so one lost heartbeat
no longer refuses Start with *Pump offline, running or faulted*, or flickers the
card stale. `pumpStoppedMaxAgeSec: 75` is the tighter one and belongs to a single
question: *may the stop sequence believe this "stopped" report and close the
valves?* Being wrong there means closing the field valves and then the main valve
onto a pump that is actually running, so it stays tight at 1.25 intervals.
`pumpRunningMaxAgeSec: 45` has both jobs while the pump runs — three of its
15-second running cadence, and running is the state worth knowing about. The two
stopped limits bracket each other deliberately: `pumpOnlineMaxAgeSec` sits above
`pumpStopTimeoutSec`, so a stop confirmation that has aged out is judged against
the same clock as its own deadline instead of stalling between the two.

One consequence is worth knowing before it is seen. A stopped report 76–180 s old
is now fresh enough to start from but too old to close the valves on, so a stop
pressed at that moment sends a stop command and waits for the acknowledgement
rather than going straight to closing. That is the intended trade: the extra round
trip is cheap, and it is the case where the pump's own radio has gone quiet.

`waterMaxAgeSec` is not a free choice, and it is not display-only: the sequencer
reads it through `fresh('water', …)`, so too tight a limit refuses to start a run
and stops a running one. The WaterLevel node sends one uplink per wake cycle and
then deep-sleeps, so a limit means nothing except relative to that interval, and
the interval differs between bench and field — the builder derives the value from
`WATER_LEVEL_SLEEP_SECONDS` (twice the interval, floor 60 s).
`tools/water_level_interval.py` holds the rule and both this flow and the
WaterLevel dashboard follow it, so the two cannot drift apart.

**So switching the WaterLevel build to its 900-second deployment interval
requires regenerating this flow too.** At the bench's 10 s it ships 60 s; at
900 s it becomes 1800 s. Skip that step and the tile reads stale between uplinks
and the sequencer refuses to water. The two-step procedure is in
`examples/WaterLevel/README.md`.

The start sequence checks water, soil, device freshness, main valve online and
fault status, and pump/VFD status; opens and verifies the main valve; opens
selected field valves and waits for their reported command IDs; arms pump
frequency; rechecks conditions; and starts the pump. Stop sends pump stop,
waits for a stopped uplink, closes selected field valves in reverse order, and
closes MainValve last. If pump stop cannot be confirmed, it sends emergency
stop and leaves the valves open if that too cannot be confirmed.

Pump Start and Stop are two-stage operations. An `in_progress` report means
the VFD accepted the request, but the sequencer continues waiting for the
matching command ID's final `accepted` report and measured running/stopped
condition. The 150-second dashboard timeout remains longer than PumpControl's
120-second final-condition timeout. The Pump card hides stale radio and VFD
health values, shows Class C/LoRaWAN and protocol-v2 join diagnostics, and has
a rate-limited refresh button. Refresh sends the non-actuating FPort 50
payload `01 05`; it does not consume a command ID or operate the VFD. Stop
remains available even when Pump telemetry is offline.

Field valve telemetry currently reports the last commanded position, not a
verified physical position. The dashboard labels this explicitly. The flow
uses a matching post-command uplink as the available completion signal, but
this cannot prove that a latching field valve physically moved. Confirm the
field-valve position-feedback design before relying on unattended operation.

The complete flow is generated by `tools/build_irrigation_dashboard.py`. Its
visual layout lives in `examples/IntegratedDashboard/docs/dashboard_template.html`.
Run the generator after
changing either source to keep the importable JSON current, then
`node tools/test_irrigation_dashboard.js`, which checks the sequence logic and
the import invariants (no tab, no broker node, shared ids, derived water age, and
the cadences it resolves from the WaterLevel, PCV and PumpControl sources)
and fails if either drifts. Replace the whole deployed flow using the steps
above.
