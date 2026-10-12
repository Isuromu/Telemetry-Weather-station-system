# Watering-plan SQLite store

`001_initial.sql` is the initial migration for the temporary Raspberry Pi plan
store. `002_attempt_tracking.sql` adds Phase 3 result tracking without
rewriting existing plans. Together they define plan definitions, individual
execution attempts, and an append-only event history. Payload and valve
snapshots are stored as JSON text so the store does not depend on SQLite JSON
extensions.

Initialize or migrate a database with:

```sh
python3 tools/migrate_watering_plan_db.py /var/lib/node-red/watering-plans.sqlite3
```

The migration command is safe to repeat and refuses a newer schema or an
unversioned database that already contains watering-plan tables. The database
adapter must keep foreign-key enforcement enabled on every connection. The
scheduler must claim a due plan with one conditional
database write so two ticks or Node-RED instances cannot own the same plan.
Plan revision checks, insert/update, and the corresponding event insert must be
one transaction. Application code must canonicalize the accepted request before
writing `canonical_payload` and its SHA-256 digest; retries compare both the
revision and canonical payload.

[`../plan-request.schema.json`](../plan-request.schema.json) defines the
version-1 request shape. API validation must also parse timestamps as instants,
require explicit offsets, verify `start_at < valid_until`, and validate
capabilities against the accepted valve snapshot; JSON Schema shape checks do
not replace those semantic checks.

`watering_plan_events` rejects updates and deletes at the database level. The
adapter should insert an event in the same transaction as every plan or attempt
state change. Mirror committed events to InfluxDB after the SQLite transaction;
InfluxDB is an event/telemetry sink, not the authoritative plan store.

The Python prototype runtime now exposes the store, temporary API, scheduler, and generated dashboard sequencer seam. Node-RED SQLite module and Raspberry Pi Node-RED version still need confirmation before claiming Node-RED-native deployment support.

For Phase 3, an attempt stores the dashboard-confirmed monotonic start point,
the pre-dispatch commissioned `valve1` positive-total meter baseline, latest
delivered cubic metres, and final elapsed runtime. A monotonic reading is valid
only for the running process; after a restart the scheduler records
`recovery_required` and may use the existing dashboard stop path after fresh
state observation. It never resumes watering from a stored target.
