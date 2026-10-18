# P2 adapter wire contract (draft implementation)

`NodeRedAdapter` sends authenticated `POST /internal/irrigation/command` requests:

```json
{"action":"start","operation_id":"<stable attempt id>","plan_id":"<plan id>","plan_revision":1,"valves":{"valve1":true}}
{"action":"stop","operation_id":"<same attempt id>","plan_id":"<plan id>","plan_revision":1}
```

Successful responses must include `data.operation_id` matching request ID. A replay may return `data.duplicate=true`; caller treats it as same operation, not a new start. Adapter validates valve keys, enforces finite HTTP timeout, and maps HTTP error envelope to `NodeRedError`.

## Safety boundary

Adapter is not dispatched by server scheduler in P2. Node-RED now reserves each `(operation_id, action)` in the existing durable watering-plan SQLite store before forwarding a first request to `CONTROL`. Exact retries return the prior reservation without forwarding to `CONTROL`; different request content for same pair returns `409 operation_id_conflict`. One operation may have one start and one stop reservation under same stable `operation_id`.

A crash after reservation but before `CONTROL` forwarding fails closed: retry is deduplicated, never replayed. The reservation is not proof of actuation or completion; unresolved reservations require operator reconciliation. Keep `IRRIGATION_PLAN_EXECUTION_ENABLED=false` until scheduler integration, startup reconciliation, and supervised test acceptance. Do not infer completion from command HTTP acceptance; query `operation_event` records from InfluxDB.

## Influx state contract

`device_state()` returns all six schema device IDs. Each device has `status` (`available`, `stale`, `unavailable`), `observed_at` from Influx point timestamp, `age_sec`, and decoded `data`. Query errors raise `InfluxUnavailable`; route returns 503 rather than disguising a backend outage as stale/no data. Missing points are `unavailable`; nullable fields remain absent, never coerced to zero.

The generated irrigation flow now writes `device_telemetry` to `irrigation_telemetry` and `operation_event`, `command_ack`, and `sequencer_phase` to `irrigation_events`. Output nodes reference the workspace-owned config ID `9f8e7d6c5b4a3120` (`Irrigation InfluxDB write`), which carries the bucket-scoped write token; no Influx config node or credential is exported. `IRRIGATION_SITE_ID` defaults to `s01`; freshness uses `IRRIGATION_DEVICE_FRESHNESS_SECONDS` JSON overrides or global `IRRIGATION_INFLUX_FRESHNESS_SECONDS` (300 seconds).

`node-red-contrib-influxdb` 0.7.0's output node consumes `[fields, tags]` (or an array of those pairs), not a `{measurement,tags,fields,timestamp}` point object. The mappers emit that wire shape; a field named `time` sets the point timestamp, and empty tag values are omitted. A point object would be written as fields only, silently dropping tags and decoded values.

## Live verification (2026-10-18)

- Deployed the generated 48-node flow into the live Node-RED workspace at `192.168.1.116:1880`; the experimental `Main Valve Flow` tab was removed first at the operator's request. The workspace now has 155 nodes across six tabs, and the four Phase 6 output nodes write to `irrigation_telemetry` and `irrigation_events`.
- A temporary test tab wrote one synthetic `operation_event` and queried it back; the result contained the expected `operation_id`, `plan_id`, `plan_revision`, `event_type`, and `source` tags and the `message` field. The same test queried `irrigation_telemetry` and returned real `battery_voltage_v` values with `device_id`, `dev_eui`, `device_type`, and `site_id` tags. The temporary tab was removed and the workspace was redeployed.
- **Scoped write config installed:** created workspace config `9f8e7d6c5b4a3120` (`Irrigation InfluxDB write`) with the bucket-scoped write token, repointed all four Phase 6 outputs at it, and removed the legacy `6b15b438c5cf6d49` node. The scoped token writes `irrigation_events` (204) and is denied `main_valve` (403) and query access. A synthetic operation event written through the new config was read back with the server read token.
- **Store module updated:** installed the updated `watering-plan-store` module and `003_irrigation_operations.sql`; the live database is at schema versions 1, 2, 3 with `irrigation_operations` present. The live reservation test returned start `duplicate:false`, exact retry `duplicate:true`, changed content `409 operation_id_conflict`, and a stop reservation `duplicate:false`.
- **Server read token verified:** live Flux queries against `irrigation_telemetry` and `irrigation_events` returned 200; a write attempt returned 403.
- Synthetic records remain: operation IDs `p2-live-*` (`source: p2_live_test`), `p2-scoped-mvd9ox2z` (`source: p2_scoped_write_test`), the `p2_token_scope_test` write in `irrigation_events`, and reservations `p2-store-test-5jetox` (start/stop) in `irrigation_operations`. Pre-fix telemetry points have only a `measurement` field. Remove with the admin/read credential if desired.
- Backups: `/home/maxi/backups/phase6-20261018/` holds the pre-003 SQLite database and the prior module and migration files.
- The legacy all-access token id `1134f10e0b8f4000` is no longer referenced by Node-RED and can be deactivated after operator confirmation.
- **Replacement tokens verified (2026-10-18):** active scoped read/write tokens are installed. The write token writes `irrigation_telemetry` and `irrigation_events` (204) and is denied `main_valve` (403) and queries; the read token queries both buckets (200) and is denied writes (403). The four Phase 6 outputs are re-enabled, and live telemetry resumed. Values are not recorded.