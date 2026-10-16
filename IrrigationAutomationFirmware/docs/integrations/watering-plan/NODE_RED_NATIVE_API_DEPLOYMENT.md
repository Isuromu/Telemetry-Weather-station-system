# Node-RED native watering-plan API deployment

## Status

The generated Integrated Dashboard flow contains the six `/api/v1` HTTP routes
backed by the local `watering-plan-store` custom node. The routes persist, read
and cancel plans only. They **do not schedule or actuate watering**: no public
route reaches MQTT or the `CONTROL` sequencer.

The transport and lifecycle rules the routes implement are defined once, in
[`api-protocol.rules.json`](api-protocol.rules.json) (see
[`API_PROTOCOL_SPEC.md`](API_PROTOCOL_SPEC.md)); the flow's status map is
generated from it at build time. `tools/test_watering_plan_backend.py` exercises
those rules against the reference fake backend and is the conformance suite a
future backend must also pass.

The dashboard reads the same store, so a mobile-submitted plan appears on the
plan panel without a second copy and without any outbound HTTP client.

`GET /api/v1/device-state` is served from the sequencer's own live snapshot in
flow context (`flow.get('irrigation')`); when that snapshot is absent it returns
`status: "unavailable"`, and it marks any stale device `status: "stale"`. It does
**not** query InfluxDB yet. `GET /api/v1/health` returns `clock_synchronized`
from `IRRIGATION_CLOCK_SYNCHRONIZED`, the same value the plan validator gates on.
Until that variable is set to `true`, every plan write is rejected with the named
`clock_unsynchronized` code — plan acceptance is storage/API testing only, not
approval to water hardware.

## Install the custom node

On the Pi, from the Node-RED user directory, install this repository package so
the runtime can load its `sqlite3` dependency:

```sh
cd ~/.node-red
npm install /path/to/IrrigationAutomationFirmware/nodes/watering-plan-store
```

Restart Node-RED and confirm the palette/runtime log lists `watering-plan-store`.
**Do not deploy the routes until it does**: the six `http in` nodes are built-ins
and still register, but a request reaching an uninstalled node type hangs until
the client times out. `sqlite3` needs an arm64 prebuild or a node-gyp toolchain
(`python3 make g++`) on the Pi.

Node package defaults:

- database: `/var/lib/node-red/watering-plans.sqlite3` (node property)
- migrations: package-relative `docs/integrations/watering-plan/sqlite`, or the
  process environment variable `WATERING_PLAN_MIGRATIONS_DIR`

Ensure the Node-RED service account can create and read the database parent
directory. Back up the database with a SQLite-consistent backup; do not copy live
WAL files independently. Verify `schema_migrations` lists versions 1 and 2 after
the first deploy.

## Required environment

Node-RED environment variables (the editor Environment tab or `settings.js`) —
one secret and the plan validator's configuration. Prototype defaults in
parentheses:

- `IRRIGATION_PLAN_API_TOKEN` (none): mobile Bearer credential. If unset, every
  route returns 401. It is separate from the existing `IRRIGATION_INTERNAL_TOKEN`
  and must never be the same value.
- `IRRIGATION_CLOCK_SYNCHRONIZED` (`false`): set to `true` only with
  `chrony`/`timedatectl` evidence. It gates both health and plan acceptance.
- `IRRIGATION_GATEWAY_MAX_RUNTIME_SEC` (`14400`): the gateway ceiling; the
  accepted run limit is the lower of this and the plan's `hard_max_runtime_sec`.
- `IRRIGATION_FIELD_REGISTRY` (`field-001`→`valve1`, commissioned): JSON array of
  `{field_id, display_name, valve_id, flow_meter, enabled}`.
- `IRRIGATION_ALLOWED_VALVE_SETS` (`[["valve1"]]`): JSON array of valve-set
  allow-lists; a resolved target set must be one of them.
- `IRRIGATION_DURATION_UNITS` (`seconds,minutes,hours`) and
  `IRRIGATION_VOLUME_UNITS` (`m3`): comma-separated accepted stop units.
- `IRRIGATION_METER_STATUS_JSON` (unset): JSON
  `{ "valve1": { "healthy": true, "fresh": true, "decoder": "commissioned" } }`.
  Unset means every volume plan is rejected with `meter_unavailable`.

`WATERING_PLAN_MIGRATIONS_DIR` is a **process** environment variable read by the
node, not a Node-RED environment variable. Keep the two kinds distinct.

Never put any secret in flow JSON, the dashboard UI, logs, screenshots, or the
mobile repository configuration.

## Deploy and test

Follow `docs/NODERED_DASHBOARD_WORKFLOW.md`: regenerate the flow, run the tests,
import into the current unlocked irrigation flow, inspect the nodes, then deploy.

```sh
python3 tools/build_irrigation_dashboard.py
node tools/test_irrigation_dashboard.js
node tools/test_nodered_flows.js
node tools/test_flow_ids.js
npm test --prefix nodes/watering-plan-store
```

Then run the black-box API suite only against an authorized test Pi. The
default 3 s client timeout is too tight for the Pi; record what you observe in
[PI_LIVE_TEST_LOG.md](PI_LIVE_TEST_LOG.md):

```sh
export IRRIGATION_PLAN_API_URL='https://<test-pi>'
export IRRIGATION_PLAN_API_TOKEN='<test-token>'
python3 tools/test_mobile_watering_plan_api.py --live --timeout 15
```

Use an isolated LAN or an HTTPS reverse proxy. Expose `/api/v1/*` and the
dashboard only; never expose the Node-RED editor/admin endpoint or port-forward
the Pi.

## Known commissioning gaps

Native persistence and the mobile HTTP routes are a deployable test boundary, not
irrigation automation completion. Before enabling scheduler/actuation, implement
and commission: the InfluxDB device-state provider (device-state is currently
the sequencer snapshot), a recurring scheduler tick and executor gate, sequencer
result reconciliation, duration and meter tracking, active-stop acknowledgement,
restart recovery, and no-auto-resume behavior.
