# Watering-plan API protocol (v1) — portable specification

This is the normative description of the temporary watering-plan API and its
plan lifecycle. A real backend is written from it and validated against it with
`tools/test_watering_plan_backend.py`; nothing here is specific to Node-RED or
to Python.

## Where each rule lives

Highest to lowest; each fact is defined in exactly one place.

1. `plan-request.schema.json` — the request **field shape** only (types, required
   keys, enums, `additionalProperties`). It never defines a status or a
   lifecycle rule.
2. `tools/watering_plan_contract.js` — payload **acceptance**: which error code a
   payload produces, and the canonical payload digest. Its `ERROR_CODES` export
   is the authoritative code set; it must equal
   `contract_error_codes` in the rules file.
3. `api-protocol.rules.json` — everything else: routes, `error_code -> status`,
   state and transition tables, reason vocabularies, idempotency/revision and
   cancel outcomes, pagination, the record projection.
4. This document — prose over the three above. It introduces no new constant and
   every table below cites its `api-protocol.rules.json` key.

## Transport

Every response is JSON with `Content-Type: application/json` and one envelope:

```json
{"api_version": "v1", "request_id": "...", "data": {}}
{"api_version": "v1", "request_id": "...", "error": {"code": "...", "message": "...", "details": []}}
```

- `request_id` is echoed from the `X-Request-ID` request header when present,
  otherwise generated. It identifies one HTTP attempt and is **not** an
  idempotency key. It is never the bearer token.
- Requests carry `Accept: application/json`, `Content-Type: application/json`
  and `Authorization: Bearer <token>`. Missing or invalid credentials yield the
  `default_errors.unauthorized` entry (401 `unauthorized`); a response never
  contains the token.

### Routes — `routes`

| Method | Path | Success |
|---|---|---|
| `GET` | `/api/v1/health` | 200 |
| `GET` | `/api/v1/device-state` | 200 |
| `GET` | `/api/v1/watering-plans` | 200 |
| `GET` | `/api/v1/watering-plans/{plan_id}` | 200 |
| `PUT` | `/api/v1/watering-plans/{plan_id}` | 201 new, 200 repeat/replace |
| `POST` | `/api/v1/watering-plans/{plan_id}/cancel` | 202 active stop requested, 200 future cancel |

An unknown path is 404 `not_found`; a known path with an unsupported method is
405 `method_not_allowed` (`default_errors`).

### Error code → status — `error_status`

`invalid_payload`, `invalid_plan_id`, `plan_id_mismatch`, `invalid_cursor`,
`invalid_json` → **400**; `unauthorized` → 401; `not_found`, `plan_not_found` →
404; `method_not_allowed` → 405; `revision_conflict`, `stale_revision`,
`active_revision_conflict`, `attempt_not_found`, `invalid_state`,
`invalid_tracking` → **409**; every other contract semantic code (including
`invalid_timestamp`, `invalid_time_window`, `clock_unsynchronized`,
`meter_unavailable`) → **422**; `validator_unavailable`, `internal_error` →
**500**. `error_code_aliases` maps historical names
(`active_plan_revision_conflict` → `active_revision_conflict`).

## Plan writes

`plan_id` is the idempotency key. Outcomes are in `write_outcomes.accept`:

| Incoming | Result |
|---|---|
| New `plan_id` | 201, `state` = `scheduled` (or `cancelled` when `status` is `cancelled`) |
| Same revision, same canonical payload | 200, `idempotent` |
| Same revision, different content | 409 `revision_conflict` |
| Lower revision | 409 `stale_revision` |
| Higher revision while `scheduled`/`delayed` | 200 `replaced` |
| Higher revision while active | 409 `active_revision_conflict` |

`POST .../cancel` follows `write_outcomes.cancel`: a claimable plan becomes
`cancelled` (200); an active plan becomes `stop_requested` (202); a terminal
plan is unchanged (200). A 202 means **stop requested**, never a confirmed
physical stop.

## Lifecycle

States are in `plan_states` (attempt states in `attempt_states`); `transitions`
is the authoritative table. In summary:

```
scheduled -> delayed -> starting -> running -> stopping -> completed
     |          |          |           |           |
     +-> cancelled         +-> failed  +-> failed  +-> failed
     +-> expired                          +-> recovery_required
```

- `claimable_plan_states` (`scheduled`, `delayed`) may be replaced or cancelled.
- `active_plan_states` (`starting`, `running`, `stopping`, `stop_requested`)
  block a new run; `blocking_plan_states` adds `recovery_required`.
- A plan that is due but cannot start becomes `delayed` with a reason from
  `delay_reasons`; a gate failure uses `clock_unsynchronized` or
  `executor_gate_failed`, a busy pump `pump_run_active`.
- A run stops for a reason from `stop_reasons` (target reached or a cancel
  dispatch) or a fault from `failure_reasons`.
- A plan whose `valid_until` passes before it starts becomes `expired`.
- On restart, an unresolved active attempt becomes `recovery_required`; watering
  is never resumed automatically, and only the safe stop path may be attempted
  after fresh observation.

Every state change appends an event; `event_types` lists the vocabulary.

## Reads

`GET /api/v1/watering-plans` accepts optional `from`, `to`, `limit`, `cursor`;
ordering, default/max limit and the opaque cursor are in `pagination`. Each item
is the record shape from `record_projection` — the request payload flattened,
`state`/`accepted_at`/`updated_at` merged in, `delay_reason` from the plan's
`last_delay_reason`, and an optional `execution` object with
`execution_fields`. `GET .../{plan_id}` additionally carries `events` and
`attempts`.

## Porting to a new backend

1. Port `tools/watering_plan_contract.js` (or call it) so payload acceptance and
   `ERROR_CODES` are identical.
2. Implement the routes and envelopes above, mapping error codes through
   `error_status`.
3. Implement the lifecycle in `transitions`, driving the reference state
   machine.
4. Implement the record projection per `record_projection`.
5. Run `tools/test_watering_plan_backend.py` against it: the conformance suite
   reads `api-protocol.rules.json` and `conformance-scenarios.json`, so a
   conforming backend passes the same vectors.

`time_mode: "virtual"` in the scenarios only changes **when** ticks happen; it
never changes what a rule does.
