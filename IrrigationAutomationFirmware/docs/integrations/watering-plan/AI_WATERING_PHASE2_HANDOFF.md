# AI Watering Phase 2 handoff

Date: 2026-10-12

## Why this file exists

This is a concise continuation note for a new session. It records verified repository state, approved decisions, and Phase 2 implementation status.

## Repository state inspected

- Working tree was clean on branch `agent-work`.
- `agent-work` was three commits ahead of `origin/agent-work`:
  - `95ed435` — `AI Watering plan: add validated SQLite plan foundation`
  - `9b358fa` — `Add mobile watering plan contract`
  - `7fa295f` — `fix(irrigation): honest refusals, fault banner`
- The inherited instructions require `bd`, but this Git repository currently has no `.beads` database or tracked Beads JSONL file. `bd ready` therefore fails with `no beads database found`. Do not initialize one silently; first confirm where this repository's tracker is intended to live.

## What is already implemented

Commit `95ed435` added foundation. Current working tree adds temporary Python control-plane and generated sequencer seam:

- version-1 request schema: `plan-request.schema.json`;
- semantic validation and canonical SHA-256 payload hashing: `tools/watering_plan_contract.js`;
- initial SQLite schema for plans, attempts, and append-only events: `sqlite/001_initial.sql`;
- repeat-safe migration runner: `tools/migrate_watering_plan_db.py`;
- transactional store: `tools/watering_plan_store.py`;
- validator bridge, authenticated API, scheduler, and HTTP sequencer adapter: `tools/watering_plan_contract_cli.js`, `tools/watering_plan_api.py`, `tools/watering_plan_scheduler.py`;
- generated internal token-gated command/state endpoints in `tools/build_irrigation_dashboard.py`;
- focused store/scheduler/API tests and dashboard seam coverage;
- updated approved policies in `AI_WATERING_PLAN_ROADMAP.md`.

Runtime deployment still depends on Raspberry Pi configuration: Node-RED version, internal token provisioning, clock synchronization, and InfluxDB device-state provider.

## Approved decisions that must not be reopened without new evidence

Source: `AI_WATERING_PLAN_ROADMAP.md`.

- Mobile submits already-approved human or AI plan occurrences. Node-RED does not approve or author plans.
- Node-RED/SQLite is the temporary Raspberry Pi implementation; a future local server must retain the same `/api/v1` contract.
- Scheduling must call the existing Integrated Dashboard sequencer. It must not create a second direct pump/valve command path.
- There is one pump and at most one active pump run. Due plans wait or expire; they are never silently changed or started after `valid_until`.
- Version 1 supports multi-valve **duration** plans and single-field **volume** plans only for the commissioned metered mapping `field-001` -> `valve1`.
- Moisture-based completion is explicitly out of scope.
- `targets.mode = all` is resolved to a fixed enabled-valve snapshot when the plan is accepted.
- Eligible-plan order is `start_at`, then gateway acceptance time, then `plan_id`.
- A scheduler claim must be one conditional SQLite write so duplicate ticks/processes cannot own the same plan.
- Revision validation, plan insert/update, and event insertion must be one transaction. Events are mirrored to InfluxDB only after commit.
- Clock must be synchronized before a due plan starts.
- Restart never resumes watering automatically. Recovery may only enter the existing safe stop path after fresh telemetry; uncertainty becomes `recovery_required`.
- Active-plan cancellation means `stop_requested` and invokes the ordered stop path; it does not immediately claim the hardware is stopped.
- `['valve1', 'valve2']` is allowed only as a controlled stress-test set, not as production hydraulic commissioning.
- Prototype access is local LAN with a bearer token provisioned by short-lived QR pairing. Do not expose Node-RED publicly.

## Exact Phase 2 scope

The roadmap names Phase 2 **scheduler adapter**:

1. Add one scheduler owner with a regular tick and atomic due-plan claim.
2. Resolve `all` to a valve snapshot at acceptance time.
3. Validate target capabilities before scheduling.
4. Dispatch accepted due plans through the existing Integrated Dashboard sequencer.
5. Implement delayed, expired, conflict, and active-cancellation handling.

Phase 1 prerequisite and Phase 2 adapter are implemented in the working tree:

1. tested SQLite plan-store adapter with idempotent create/replace/cancel/read and atomic claim operations;
2. temporary `/api/v1` plan endpoints using the existing contract validator;
3. scheduler tick and HTTP sequencer adapter on top of that store.

This preserves roadmap dependency order. It does not claim live Raspberry Pi deployment until runtime module/configuration is confirmed.

## Critical files for continuation

- `docs/integrations/watering-plan/AI_WATERING_PLAN_ROADMAP.md`
- `docs/integrations/watering-plan/plan-request.schema.json`
- `docs/integrations/watering-plan/sqlite/001_initial.sql`
- `docs/integrations/watering-plan/sqlite/README.md`
- `tools/watering_plan_contract.js`
- `tools/test_watering_plan_contract.js`
- `tools/migrate_watering_plan_db.py`
- `tools/build_irrigation_dashboard.py`
- `tools/test_irrigation_dashboard.js`
- `tools/test_watering_plan_store.py`
- `tools/test_watering_plan_scheduler.py`
- `tools/test_watering_plan_api.py`
- `docs/WATERING_PROCESS_CASES.md`

## Constraints for implementation

- The Integrated Dashboard flow is generated. Never edit its importable JSON directly; edit `tools/build_irrigation_dashboard.py`, regenerate, and run its test.
- Keep source and comments in English.
- Do not invent production pump-capacity sets, extra field mappings, meter capability, or soil-stop semantics.
- Confirm the Raspberry Pi Node-RED version and SQLite node/API before claiming deployment support. The Python migration tool does not install that module.
- Preserve named API errors, revision/idempotency behavior, and state names from the roadmap.

## Minimum verification for the next slice

Run at least:

```sh
node tools/test_watering_plan_contract.js
python3 tools/migrate_watering_plan_db.py <temporary-db-path>
node tools/test_irrigation_dashboard.js
node tools/test_flow_ids.js
```

Add focused tests for:

- duplicate identical delivery;
- same-revision content conflict and stale revision rejection;
- higher-revision replacement only in `scheduled`/`delayed`;
- future cancellation versus active `stop_requested`;
- competing atomic claims;
- delay, expiry, deterministic ordering, and restart reconciliation;
- proof that schedule dispatch enters the existing sequencer rather than publishing direct device commands.

## Current limitations

- Phase 3 duration/volume result reconciliation is not implemented; scheduler enforces hard runtime safety ceiling and observes sequencer state.
- Node-RED SQLite module/version and live Pi deployment remain unverified.
- `/device-state` returns explicit unavailable data until Influx provider is configured.
