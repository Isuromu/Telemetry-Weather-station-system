# Working on the Node-RED dashboards

Operational notes for changing a dashboard without breaking the live system.
Read `NODERED_FLOW_CONVENTIONS.md` first for the identity rules; this file is the
mechanics.

## The pipeline

A dashboard flow is generated, never hand-edited:

```
examples/<Device>/include/*_flow.json   <- generated, do not edit by hand
       ^
tools/build_<device>_dashboard.py       <- the source of truth (markup + decode + logic)
tools/test_<device>_dashboard.js        <- unit tests for the generated flow
tools/test_nodered_flows.js             <- conventions test, all flows
```

Change the builder, regenerate, run the tests, then deploy. Editing the JSON
directly means the next rebuild silently discards the edit.

```bash
python3 tools/build_<device>_dashboard.py     # regenerates the flow json
node tools/test_<device>_dashboard.js
node tools/test_nodered_flows.js
```

Run **both** suites. `test_nodered_flows.js` catches conventions the per-device
tests do not: identity literals, a shipped `tab` or `mqtt-broker`, a repeating
subscribe inject, a stored-context guard.

## Deploying through the editor

`irrtest/deploy_flow.py` performs the operator's own sequence over CDP. Doing it
by hand, the order matters:

1. **Select the tab and unlock it.** A locked workspace makes Node-RED refuse
   edits and force the import onto a new flow. Check with
   `RED.workspaces.isLocked(id)`; `RED.workspaces.unlock(id)`. The Water Level
   and SoilNode tabs were locked, which is why every import spawned a tab.
2. **Delete the nodes the import is about to replace.** `RED.view.select({nodes})`
   plus `core:delete-selection` is unreliable — remove by id instead:
   `RED.nodes.remove(id)`, and `RED.nodes.removeGroup(groupObject)` for groups
   (it needs the object, not the id; `RED.nodes.remove` silently ignores groups).
   Keep `ui-base` and `ui-theme`: they have no `z` and every tab uses them.
3. **Import** with the dialog. `RED.actions.invoke('core:show-import-dialog')`,
   focus `#red-ui-clipboard-dialog-import-text`, `Input.insertText` the JSON —
   then **dispatch a native `keyup`**, because Node-RED enables the Import button
   on `keyup`/`paste` only and `insertText` raises neither.
4. **Check "Import to: current flow".** It is selected automatically *only when
   the active workspace is unlocked*. If it is not selected, stop — importing
   will create a new tab. Placement follows the **active workspace**; the nodes'
   `z` is ignored.
5. **Resolve conflicts** via "View nodes…" then "Import selected". Rows in the
   Nodes group import with their original ids. Rows carrying a `replace` toggle
   are the shared config nodes (`ui-base`, `ui-theme`, `mqtt-broker`, `influxdb`,
   `ui-page`) — leave those off so the existing singletons are kept.
6. **Verify the editor model before deploying**: every expected id present on the
   tab, no duplicate ids, no new workspace, and the function content actually
   changed. Only then click Deploy (mode: Modified Flows).
7. **Verify the server after**: read `/flows` (a *read*) and confirm the tab's
   `env` still holds all its variables, then confirm telemetry advances —
   `last_seen` moving is the proof the subscription came back.

## Editor/CDP gotchas that cost real time

- CDP `Input.dispatchMouseEvent` does **not** reach jQuery UI button handlers.
  Use `$('#id').trigger('click')`, wrapped so the expression returns a primitive
  — returning a jQuery object fails with "Object reference chain is too long".
  The one exception is the import-target option and the notification buttons,
  which do need a real mouse click.
- `offsetParent !== null` is useless for visibility: jQuery UI dialogs sit under
  `position: fixed`, for which `offsetParent` is always null. Screenshot instead
  (`Page.captureScreenshot`).
- After `Page.reload`, wait until `RED.nodes.eachWorkspace` reports the expected
  count. Checking for the deploy button alone fires far too early, and evaluating
  before `RED` exists throws `RED is not defined`.
- The renderer can wedge — `Runtime.evaluate` times out and
  `Page.handleJavaScriptDialog` reports no dialog. Restart Chrome with the same
  `--user-data-dir` to keep the editor session.
- Someone may be editing in parallel. Watch for *"The flows on the server have
  been updated"* and reload before deploying, or a stale model overwrites their
  change.

## Keeping the context small

A dashboard session can burn context on log volume rather than reasoning. What
worked:

- **Never read a raw MQTT log or a flow JSON into context.** A single
  `mosquitto_sub` capture is ~400–750 KB of base64 and protobuf. Write a small
  script that answers the question and print only the verdict — `irrtest/correlate.py`
  turns a log into a one-line-per-command confirmed/lost table.
- **Filter with `grep -a`.** The logs contain NUL bytes, so plain `grep` treats
  them as binary and prints nothing — which reads as "no matches" when the data
  is right there.
- **Timestamp at capture time**, not later: `mosquitto_sub -F '%I %t %p'`. The
  app-level downlink JSON carries no `time`, `fCnt` or `deviceName`, so arrival
  order is the only clock you get.
- **Poll state on a schedule** (`irrtest/poll_state.py`, 0.5 s) instead of
  re-deriving phases from logs. It prints only on change, so a 25-minute run is a
  few dozen lines.
- **Batch independent tool calls** in one message; prefer `find`-resolved paths
  in shell commands, since path lookups on the repo intermittently fail in this
  environment.
- **Keep generated artifacts out of git** (see below) so `git status` stays
  readable and the diff shows code, not data.
- **Delegate broad searches** to a subagent when the answer is a location rather
  than a reading — it returns file:line instead of whole files.

## What must never be committed

MQTT captures, their derived timelines and the deployed-flow backups carry the
real ChirpStack application UUID and every DevEUI. `NODERED_FLOW_CONVENTIONS.md`
requires identity to come from environment variables and never appear in git, so
these live on disk and are ignored:

```
irrtest/mqtt*.log
irrtest/t.log
irrtest/deployed_flows_backup_*.json
irrtest/shot_*.png
irrtest/__pycache__/
```

Check for leaks before committing anything under `irrtest/`:

```bash
grep -alE '[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}' irrtest/*
```

Node-RED node ids are 16 hex characters too, so an EUI-shaped hit is not
necessarily a DevEUI — compare against the real EUIs before panicking.

## Testing behaviour, not just the build

The two unit suites never touch a real device. To validate end to end, use the
real-click harness — it drives the dashboard page in headless Chrome, so it
exercises `ui-template` → socket.io → sequencer rather than an inject endpoint:

```bash
python3 irrtest/run_click_cycle.py --water 20 --cycles 3
python3 irrtest/correlate.py irrtest/mqtt_run4.log   # every command: confirmed or lost
```

A watering cycle is only "clean" when the tally shows `teardown` ending `idle`,
`fault` null, and the notice `Watering stopped; valves closed` — and the
correlation shows no lost downlink. `pump_stop` is the number to watch: 13–17 s
on a healthy VFD link, and a jump past the 150 s timeout means the RS485 bus
dropped, not that the sequence is wrong.
