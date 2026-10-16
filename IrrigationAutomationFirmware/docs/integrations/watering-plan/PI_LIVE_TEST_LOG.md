# Pi live test log — watering-plan `/api/v1`

Running record of what was actually observed on the gateway Pi, as opposed to
what the repository tests prove. Append a dated section per session; keep
**observed**, **fixed**, **open** and **unverified** distinct, and never restate
a mock or prototype result as Pi evidence.

- Target: `192.168.2.217` (`ssh maxi@192.168.2.217`, local device), Node-RED
  5.0.4 as user `maxi`, Node 24.19.0.
- Routes: `http://192.168.2.217:1880/api/v1/*`; dashboard
  `http://192.168.2.217:1880/dashboard/irrigation`.
- Suite command (the default 3 s client timeout is too tight for the Pi):

  ```sh
  IRRIGATION_PLAN_API_URL='http://192.168.2.217:1880' \
  IRRIGATION_PLAN_API_TOKEN='<token>' \
  python3 tools/test_mobile_watering_plan_api.py --live --timeout 15
  ```

  Deployed wiring, and the parts that are Pi state rather than repository
  state, are recorded in the memory entry `pi-watering-plan-api-deployed`.

## 2026-10-16 — first deployment of the native routes

Deployed: the `watering-plan-store` package (copied to
`/home/maxi/node-red-nodes/watering-plan-store`, symlinked into `~/.node-red`,
`sqlite3` prebuilt inside the package dir), the 12 generated nodes appended to
the Pi's `flows.json` (`flows.json.bak-pre-api`), migrations copied next to the
package with `WATERING_PLAN_MIGRATIONS_DIR` pointing at them, database at
`/var/lib/node-red/watering-plans.sqlite3` owned by `maxi`, and the plan API and
validator variables in `~/.node-red/environment`.
`IRRIGATION_INTERNAL_TOKEN` is deliberately unset, so the CONTROL seam stays
closed and nothing can actuate.

**Observed — passes.** `PASS: 6 scenario tests; mode=live`, four runs across the
session, including three back to back after the store fix: 4 real cases
(`health_device_state_and_auth`, `create_duplicate_changed_and_stale_revisions`,
`collection_detail_cancel_and_missing`, `validation_and_path_mismatch`),
2 skipped by design. Raw curl, independent of the suite: create 201, repeat
create 200 idempotent, detail 200, list 200, cancel 200 with `state: cancelled`
on re-read, unknown plan 404 `plan_not_found`, missing bearer 401, unsupported
duration unit 422. Health and device-state answer in ~18 ms; plan writes ~30 to
55 ms; the whole suite runs in 3.2 s once server-side work is warm.
Plans survive a Node-RED restart (22 rows listed after restart).

**Fixed on the Pi — the store crashed the whole runtime.** With the root
filesystem full (0 bytes free), the store node logged `SQLITE_IOERR: disk I/O
error`, then `SQLITE_BUSY: database is locked`; both were unhandled and ended
the Node-RED process, which systemd restarted. Cause and fix are in commit
`fe3cbcf` (one connection behind an error listener, callback-backed statements,
serialized transactions); the fixed file was redeployed and three further suite
runs left `NRestarts` unchanged with no uncaught exceptions. Disk was recovered
with `apt-get clean` and the Chromium cache; the systemd journal was kept,
because device logs are evidence for the open pump/downlink investigations.

**Fixed on the Pi — the dashboard listed no plans.** The deployed dashboard
flow is an older design than the committed builder: `Request watering-plan list`
(`cf8eb30415263788`) fetches plans over **outbound HTTP to itself** through an
`http request` node, and its first guard returns early unless
`IRRIGATION_PLAN_API_URL` is set to an `http(s)://` URL. That variable was
absent, so the snapshot was never written and the panel read "No upcoming plans"
while the API held 29 rows. Adding
`IRRIGATION_PLAN_API_URL='http://192.168.2.217:1880'` to
`~/.node-red/environment` (backup `environment.bak-planurl`), restart, and the
rows appeared. The same env dependency applies to the Pi's cancel path
(`Request plan cancellation or stop`, `03c2f748596a7bcc`).

**Flow drift, decision outstanding.** `tools/build_irrigation_dashboard.py`
reads the SQLite store directly (`PLAN_LIST_REQUEST` → store,
`msg.planConsumer='dashboard'`, no URL and no token). The Pi still runs the
outbound-HTTP design. The builder output is the commit-checked flow and the
newer design, so it is the canonical one; a redeploy from the builder removes
the Pi's `IRRIGATION_PLAN_API_URL` dependency and this drift. Until that
redeploy happens, the Pi needs the variable and the repository does not.

**Dashboard refresh is polled, not pushed.** `Refresh watering plans`
(`be7da2f304253677`) is an inject with `repeat="15"`, so a plan written through
the API appears on the panel within 15 s, never per call. Only the writing live
cases can therefore move the panel. Panel grouping: Upcoming = `scheduled`,
`delayed`; Active = `starting`, `running`, `stopping`, `stop_requested`,
`recovery_required`; Recent = last five of `completed`, `failed`, `expired`,
`cancelled`.

**Open — data left on the Pi.** The store holds 29 rows, all test junk from
these live runs (`test-*`, `list-*`, `curlvar-*`, `proof-*`, `manual-*`):
7 `scheduled`, 22 `cancelled`. The scheduled seven are live future intents; they
cannot actuate while the sequencer seam is closed, but they arm the moment
`IRRIGATION_INTERNAL_TOKEN` is set. Purge before commissioning. The live runner
creates plans and never removes them; a purge helper, or cleanup inside the
runner, would keep the table honest.

**Open — disk.** `/` was at 0 bytes free during the first run and sits near 97%
now (InfluxDB is 2.4G of it). Check `df -h /` before running the suite: a full
disk is what crashed Node-RED, and the store now reports such failures instead
of dying, but plan writes still need room.

**Unverified — malformed JSON.** `PUT /api/v1/watering-plans/<id>` with a body of
`{bad` and `Content-Type: application/json` returns 400 with Node-RED's
`body-parser` HTML, which leaks an absolute path, instead of the v1 error
envelope. Confirmed identically on the Pi and on the local sandbox, so it is
Node-RED behaviour, not a deployment fault. It needs `httpNodeMiddleware` in
`settings.js`; no node in the flow can intercept it.

**Still absent by design.** The routes store, read and cancel plans only. There
is no scheduler tick, no sequencer call, no meter or duration tracking and no
stop acknowledgement; device-state is the sequencer snapshot and reports
`stale`/`unavailable` when that snapshot is absent. Nothing here is evidence
that watering works.

## 2026-10-16 — a cancelled plan vanished from the dashboard

Operator cancelled `list-f5d92da0-f5dc-460e-9bc3-e563dbc49cf7` from the panel and
the row disappeared rather than moving to Recent.

**Not data loss.** `GET /api/v1/watering-plans/list-f5d92da0-…` answered
`state: cancelled` with events `plan_accepted → plan_cancelled`, cancelled
`2026-10-16T10:07:47Z`; the row was still in the store and in the list response.

**Cause — display only.** The dashboard fetches
`/api/v1/watering-plans?limit=50` and the store returns rows ordered `start_at,
plan_id` ascending. The template then built Recent as
`filter(finished states).slice(-5).reverse()`, which ranks the five with the
**latest due time**, not the five that finished last. Cancelling removed the plan
from Upcoming, and its due time (`2026-10-18T09:23:13Z`) was older than 17 other
finished rows, so the five-row window never contained it: invisible in every
column while the API still served it. Measured live at the time: 30 rows, 24
finished, 19 of them in no column at all.

The same code is in the canonical repository template, not a Pi drift:
`examples/IntegratedDashboard/docs/dashboard_template.html`.

**Fixed** in commit `adaaceb`: finished plans are sorted by `updated_at` (the
moment they finished; `start_at` only as fallback for older rows), the whole
finished set is kept so the column can count it, five are shown, and
"N older finished plans not shown" replaces the silent drop. The dashboard test
asserts the regression directly — a plan cancelled now whose due time is the
oldest must rank first — and fails against the old `slice(-5)` logic.

**Deployed to the Pi** by replacing only the `ui-template` node's `format` in
`~/.node-red/flows.json` (backup `flows.json.bak-pre-recent`), then a service
restart; the rest of the Pi's older dashboard design was left alone. Verified
against the live API with the deployed template's own computeds: the cancelled
plan is now first in Recent, and the column reports 19 hidden.

**Still open.** The panel fetch is `?limit=50` with no pagination, so once the
store passes 50 rows the oldest plans stop being fetched and no column can show
them; the count line cannot detect that. Fixing it needs either a larger limit,
a follow-up page request, or a state filter on the query.

## Between sessions

- Run the suite three times, not once: the crash above only appeared under
  repeated writes, and a single green run would have hidden it.
- Check `systemctl show nodered -p NRestarts` and the journal for
  `Uncaught Exception` before and after a run; the suite can pass and the
  runtime still be restarting underneath.
- A store-node change must be redeployed to
  `/home/maxi/node-red-nodes/watering-plan-store` and needs a service restart;
  the Pi loads the file from that directory, not from git.
