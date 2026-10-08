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

`fieldValveCommandTimeoutSec` (120 s) carries that same assumption, for the same
reason: an open command waits for the node's next wake, so the deadline has to
clear the interval the node is actually sleeping. At twice the 60-second
production interval it is as tight as this can safely be — one lost downlink used
to spend the whole budget — so an operator who raises a valve's interval by
downlink has to raise this to match. It is still the failure detector: silence
past it faults the sequence. What changed on 2026-10-08 is that half the budget is
now spent on one repeat of the same command rather than on waiting. An operator
watched a stopped run sit at "valve2: close sent" for the full 120 s and then
fault; the 2026-10-07 record already had the recovery — re-issuing the same close
was confirmed within five seconds. The repeat keeps the id on purpose: a PCV
accepts an identical repeat as a duplicate instead of pulsing the solenoid again,
and it echoes the id it holds either way, so a close that landed but was never
reported confirms on the repeat while a lost one is applied. Both waits spend
that repeat through the same per-valve latch the id refusal uses, so a valve is
never re-sent twice, and the fault that ends an unanswered wait now names what
the valve last said — `Field valve close unconfirmed: valve2 (echo 72 of 73, open,
41 s quiet)` — because otherwise the operator has a fault and no evidence. At the
2100 seconds this timeout used to carry, the wait was 35 minutes with the main
valve open, in both the open and the close case.

A field valve is confirmed by its own report **only once that report echoes our
command id**. This is not bookkeeping for its own sake. A PCV is Class A, so the
downlink arrives in the RX window *after* an uplink, and the first report following a
command still carries the previous state — a valve whose open is in flight keeps
reporting `closed`. Comparing a report's age against the send time does not separate
those two cases; the device's echoed `last_command_id` does. `valveReported` therefore
needs a fresh report of the requested state whose echoed id is not older than the id
the sequencer sent (`s.valveCmd`). Without that echo, a stop pressed inside an open's
confirmation window skips the valve in the close pass and leaves it open while the
panel prints "valves closed" — observed on hardware — and the open pass can likewise
skip a valve whose close is in flight and start the pump against a closed valve.

The other trap here is vocabulary: the codec names the uplink states `open`/`closed`
while the downlink action is `open`/`close`, so a state comparison against the action
word means the close half can never confirm and every stop run ends in the fault state
with the main valve still open. `tools/build_irrigation_dashboard.py` translates
between the two in `pcvState`, and `tools/test_irrigation_dashboard.js` feeds the
codec's real `closed` value so the mismatch cannot come back unnoticed. A valve is also
faulted at once, naming it, when its report carries `pcv_actuation_failed` or
`invalid_command_rejected`.

Start is accepted whenever the sequence is not already `running`. Pressing it while a
teardown is in progress **queues** the request rather than refusing it: the selection is
held in `pendingStart` and `s.selected` is left alone, so a close pass in flight finishes
on the set it was already walking and a refused start cannot reshape it. A queued start
fires once the field valves are confirmed closed, diverting to `open_main` before
`close_main` — so the main valve never closes and reopens — and it is re-checked against
`ready()` at that moment, because the safety conditions may have moved while the operator
waited. A start from `fault` queues the same way and re-runs the close pass, retrying the
valve that failed. A *safety* stop clears `pendingStart`, so a run stopped for low water
is never restarted automatically.

Stop pressed while the teardown is already running records the request in `stopReason`
and does nothing else — including leaving the notice alone. It used to overwrite it with
"Operator requested stop", which hid which valve the sequence was still waiting on
(`valve2: close sent; awaiting device result`) exactly when the operator needed to see it.

The zone checkboxes read their state from the server (`s.selection`, set by a
`kind:'select'` message whenever one changes, and recorded again on `start` — both only
while the sequence is editable, see below). They are deliberately not component-local,
because Dashboard 2.0 recreates the template when you leave and return to the page,
which would silently reset a local selection to whatever the component defaults to.
While a sequence is not idle the control panel also names the valves that run is
actually using (`s.selected`), so the display cannot disagree with what is watering.

**The boxes are locked while a run is active** — every phase but `idle` and `fault`.
A run's valves are fixed when it starts, so ticking a box mid-run changed nothing
physically while the panel showed a set that was not the one watering; a queued start
pressed after that would then have carried the ticked set. Locked, the boxes display
`s.selected` instead of `s.selection`, so they always show the valves being commanded,
and `Start` during a teardown queues a repeat of that same set. `fault` is deliberately
not locked: it is terminal rather than active, and a start from `fault` closes both
field valves first and then opens the chosen set, so the selection has to stay editable
there — the panel heading says as much, switching to "Locked for the run in progress."
while it is locked.

The lock is enforced twice, on one rule (`selectAllowed` in the sequencer, `zonesLocked`
in the panel): `chooseZone` sends nothing while locked, and the sequencer records a
`select` — or the selection a `start` carries — only in `idle` or `fault`. So a stale
second tab that still shows idle cannot reshape the next run either. That also means a
mid-teardown `Start` queues the set the current run is using, since the boxes could not
have been moved; the queued start is still honoured, it just repeats.

The deliberate limit: a queued start opens nothing until the field valves are closed.
Deciding "which open valves are not selected" from `pcv_last_commanded` would be unsafe,
because that field is the state the valve last *commanded*, not its position — while an
open command is in flight the valve still reports `closed`, so such a valve would land in
neither the close set nor the new selection and be left open while the pump runs.

Every device here is also commanded by its own card. Those cards used to keep a
*separate* id counter per dashboard, in flow context — which is scoped to a tab — so
the counters drifted apart as soon as one dashboard was used more than another, and
a command sent from the quiet card was refused for carrying an old id. All five now
allocate from **one counter per device in global context**: `cmd_next_id_main`,
`cmd_next_id_pump`, `cmd_next_id_valve1`, `cmd_next_id_valve2`.

Which firmware actually refuses what matters here:

| Device | A smaller id | An equal id |
| --- | --- | --- |
| PumpControl | refused, `invalid` — its rule is a signed 16-bit difference | refused unless byte-identical (`duplicate`) |
| MainValve | **accepted** | refused unless the same angle |
| valve_1 / valve_2 | **accepted** | refused unless byte-identical |

So only the pump enforces order; the others break on an exact collision. Two
consequences drive the code. First, "ahead" is modular: an id counts as newer only
when it is 1..32767 ahead of the last one the device took, and the counters wrap
`65534 -> 0`, so `nextId` and the cards compare with `idAhead` rather than
`>`. Second, a counter can still be behind what the device holds if this dashboard
missed the acks — but a refusal *is* an uplink and carries the device's real
`last_command_id`, so the sequencer resends a refused `pump`/`main` command once
with the corrected id (`retryRefused`) instead of waiting out the deadline, and the
cards' next command catches up the same way. A field valve refused *only* for its id
is resent once too (`retryValveOnce`); a reported `pcv_actuation_failed` is hardware
and faults at once, naming the valve. MainValve accepts a smaller id and so refuses
only on an exact collision, so it is resent only when the refusal names its command
and the reason is `invalid_command` — see the next paragraph.

Note the counters live in global context, which is in memory unless Node-RED is
configured for persistent storage: a restart clears them, and the first command per
device reseeds from the id that device reports.

**A status only answers the command it names.** Every device here repeats the phase or
reason of its *last* command in later heartbeats until a newer event replaces it, so a
refusal from an earlier cycle is still in the payload when the next command goes out.
Read as an answer, one stale `rejected` failed a healthy MainValve open without the
valve ever being given its chance to move, and the close that followed then reported
"Main valve close unconfirmed; inspect system" with the valve still shut — both seen
live on 2026-10-07 and reproduced off-line on 2026-10-08. So the sequencer acts on a
status only when it arrived after the command was sent *and* it names that command:
`reported_command_id` for MainValve, the send time for a field valve (which echoes an
id only for a command it processed) and for the pump (which stores an id only when it
accepts one, so a refused command carries no id at all). An attributable refusal is
resent once — and only for a reused id; a pressure interlock or an actuator fault is
not retried, and the notice names the reason
(`Main valve close failed: pressure_interlock`).

`actuator_busy` is a third case, and the only one that is waited rather than retried
or faulted. MainValve runs one movement at a time and refuses anything that arrives
while one is active, and only the *refusal* names our command — the movement's own
`finished` event names the command that started it. So the wait is held in sequencer
state (`s.mainBusyWait`), not read back from the phase, and the phase that decides is
re-entered once the valve's report says its movement ended. That covers the live case
of Stop pressed while the main valve was opening: the close is refused, the valve is
left to reach 90°, and the close is then re-issued and confirmed — the run ends idle
instead of faulting with the valve open. The wait is bounded by
`mainCommandTimeoutSec` (300 s, which clears the firmware's 180 s movement timeout),
so a valve stuck part-way still faults, naming `actuator_busy`.

The fourth case is the one that wasted the most time live, because nothing at all
comes back. MainValve's refusal is a **one-shot event**: the heartbeats after it carry
the *last* event's phase and id, so a lost uplink loses the refusal for good and the
wait cannot resolve — on 2026-10-08 a close refused busy during an opening movement had
its refusal uplink lost, and the run sat 216 s into a 300 s deadline with the valve idle
at 90° and everything else healthy. What saves it is that MainValve's angle is measured
physical position and rides every heartbeat, so the wait is judged on *state* as well:
fresh, not moving, and not at the angle that was asked for means the command never
landed. After half the deadline the sequencer repeats it once, with a fresh id — an id
the valve never stored cannot collide, and one it did store is ignored as a duplicate
instead of re-running the movement. The notice reads `Main valve close unconfirmed;
repeating once`, and the deadline still ends the wait if the repeat goes unanswered too.
Field valves get the same single repeat with the same half-deadline rule, but keep the
id, because a PCV's latching pulse must not fire twice for a close that already landed.

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

PumpControl reports every 60 seconds while stopped and every 15 seconds while
running. `pumpOnlineMaxAgeSec: 180` is the Start-gate and card liveness limit;
`pumpStoppedMaxAgeSec: 75` is the tighter limit used before the stop sequence may
trust a stopped report and close valves; `pumpRunningMaxAgeSec: 45` covers the
running state. The 150-second command timeouts still exceed PumpControl's
120-second final-condition timeout.

A stopped report 76–180 seconds old is fresh enough to start from but too old to
close valves on. A stop at that point sends a stop command and waits for a fresh
acknowledgement rather than moving directly to valve closure.

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

## What the tests cover, and what they cannot

The sequencer is a state machine whose failures are all races, so the tests are
matrices rather than examples — each one names the invariant it holds, and adding
a phase or a command without a case fails the suite.

| Matrix | What it drives | Invariant |
| --- | --- | --- |
| Stop from every phase | all 17 phases, with the pending/valve-id state each is actually entered with | ends `idle`, valves closed, pump stopped, no fault; a guard fails the suite if a `s.phase===` comparison in the generated function has no case |
| Bounded exit | every `wait_*` phase with devices that never answer | leaves the wait by its own deadline and names it; the pump stop escalates to `estop` first, then faults |
| Running safety inputs | each required device broken on its own, everything else healthy | stops with the reason of the check that fired, plus the four-hour ceiling |
| Start from every phase | idle, running, fault, every teardown, plus refused and empty selections | idle starts, running is refused by name, a fault or teardown queues without reshaping the run |
| Zone lock | the panel's computed/methods and the sequencer's `select` handler | boxes disabled and showing the running set while active; a `select` cannot reach the next start |
| Refusals and silence | stale vs correlated refusals, repeats, busy, deadlines | only a report that names the command is acted on; one repeat at half the deadline; field-valve faults name the valve's last report |
| Emitted frames | one case per command | port, op code, angle, argument, id and envelope (`qos 0`, unretained, unconfirmed, device topic) |

`tools/test_irrigation_dashboard.js` runs all of it in-process against the generated
function, so a wrong command or a hang fails locally. What it cannot do is make a real
device refuse, stall, or drop an uplink: those paths are exercised by the in-process
matrices, and confirmed live by driving the dashboard page over CDP
(`irrtest/run_click_cycle.py` for whole cycles, and a boundary sweep that starts a run
and acts the instant a valve reports open, a pump start is accepted, or `running` begins).
Anything that needs a *forced* downlink loss or a real hardware refusal is still
field-verification, not test coverage.
