# Mobile-to-Node-RED watering-plan API test plan

## Coverage boundary

This plan follows v1 contract in [AI Watering Plan roadmap](AI_WATERING_PLAN_ROADMAP.md) and [mobile integration guide](../mobile/REACT_NATIVE_WATERING_PLAN_INTEGRATION.md). Test runner sends the same HTTP requests the planned React Native client will send; no app required.

**Important:** repository now generates Node-RED `/api/v1` watering-plan HTTP routes backed by local `watering-plan-store` SQLite custom node. They are not live-Pi verified until package installation, flow deployment, and explicit `--live` test run complete. `tools/watering_plan_api.py` is the Python reference protocol implementation. The rules themselves are portable and language-neutral: [api-protocol.rules.json](api-protocol.rules.json) owns transport and lifecycle, [API_PROTOCOL_SPEC.md](API_PROTOCOL_SPEC.md) describes them, and [conformance-scenarios.json](conformance-scenarios.json) is the shared vector set. `tools/mock_watering_plan_api.py` is a thin HTTP adapter over the reference fake backend. Runner tests prove client/runner behavior only; generated-flow tests prove topology only. Do not describe either as deployed Node-RED or hardware proof. See [native deployment](NODE_RED_NATIVE_API_DEPLOYMENT.md).

Confidence labels below:

- **Roadmap contract** — documented expected behavior, not proof of deployed implementation.
- **Python prototype** — implemented and covered in the checked-in Python service/store; still not Node-RED proof.
- **Reference fake** — deterministic fake backend plus the portable lifecycle vectors in `conformance-scenarios.json`; still not production behavior.
- **Node-RED generated** — route/store implementation exists in generated flow but has not been deployed and observed on Pi.
- **Node-RED live verified** — use only after authorized `--live` evidence.

## Shared HTTP contract

All requests use `Accept: application/json`, `Content-Type: application/json`, and `Authorization: Bearer <token>`. Each request gets a fresh `X-Request-ID`; it diagnoses one attempt and is not an idempotency key. Never log token or return it in a response. All responses use JSON envelope:

```json
{"api_version":"v1","request_id":"...","data":{}}
```

or

```json
{"api_version":"v1","request_id":"...","error":{"code":"...","message":"...","details":[]}}
```

Plan writes are idempotent by exact `plan_id` + `revision` + content. On timeout, retry identical payload with same ID/revision; use new request ID per HTTP attempt. Never infer watering started from accepted write. Active cancel response `stop_requested` means stop requested, not physically confirmed.

### Request matrix

| Operation | Request and success assertions | Negative/retry assertions | Confidence |
|---|---|---|---|
| Health | `GET /api/v1/health`; 200 envelope; `data.time`, `data.clock_synchronized`; no secrets. | Missing/invalid bearer → 401 envelope. Bad/unreachable route → clear transport/API failure. | Roadmap; Python prototype; Node-RED generated; live unverified |
| Device snapshot | `GET /api/v1/device-state`; 200 envelope; `generated_at`, per-record observation times/status; stale/unavailable explicit. | Auth 401. Empty/unavailable backend is not “available”; stale data never treated as fresh. | Roadmap; Python prototype endpoint exists but default provider is unavailable; Node-RED generated; live unverified |
| List plans | `GET /api/v1/watering-plans` optionally `from`, `to`, `limit`, opaque `cursor`; 200 `{items,next_cursor}`; ascending `start_at`, then `plan_id`; page continuation has no duplicates. | Invalid limit/query behavior must use error envelope; auth 401. | Roadmap; Python prototype supports pagination; exact invalid-query semantics Node-RED generated; live unverified |
| Get plan | `GET /api/v1/watering-plans/{plan_id}`; 200 record includes accepted plan, state, timestamps, optional execution/history. | Unknown ID → 404 envelope; encoded ID/path handling and auth 401. | Roadmap; Python prototype; Node-RED generated; live unverified |
| Create plan | `PUT /api/v1/watering-plans/{plan_id}` with full JSON plan below; 201 and record with matching identity/revision/state. | Malformed JSON/required fields → 400; schema/time/capability violation → 422; path/body ID mismatch → 400; auth 401. | Roadmap; Python prototype; Node-RED generated; live unverified |
| Duplicate create/retry | Repeat exact PUT body, ID, revision; 200 existing record; no duplicate schedule/event/actuation. Simulate lost response after store, then retry exact body; same accepted plan returned. | Same ID+revision with changed content → 409; lower revision → 409. | Roadmap; Python prototype store; mock timeout simulates lost response; Node-RED generated; live unverified |
| Update plan | PUT same ID with `revision + 1`, changed approved content; only future scheduled/delayed record replaceable; 200 updated record. | Update active record → 409; same revision changed body → 409; stale revision → 409. | Roadmap; Python prototype store; Node-RED generated; live unverified |
| Cancel future | `POST /api/v1/watering-plans/{plan_id}/cancel` with `{}`; 200 and cancelled state. | Unknown plan 404; repeat cancellation idempotent result expected but exact status/state not fully nailed down by roadmap. | Roadmap; Python prototype; repeat semantics Node-RED generated; live unverified |
| Cancel active | Same POST; 202 and `state: stop_requested`; later GET must distinguish requested from confirmed stop and expose final result only after acknowledgement. | Never assert physical stop from 202; radio loss/restart require device commissioning and cannot be proven by mock. | Roadmap; Python prototype; Node-RED generated; live unverified |

### Plan request body

```json
{
  "schema_version": 1,
  "plan_id": "UUID-per-occurrence",
  "revision": 1,
  "status": "scheduled",
  "source": "human",
  "created_at": "2026-10-13T10:00:00Z",
  "timezone": "Asia/Tashkent",
  "start_at": "2026-10-15T06:00:00+05:00",
  "valid_until": "2026-10-15T16:00:00+05:00",
  "targets": {"mode": "selected", "field_ids": ["field-001"]},
  "run_mode": "parallel",
  "stop": {"type": "duration", "value": 60, "unit": "seconds"},
  "hard_max_runtime_sec": 3600
}
```

`plan_group_id` and `summary` optional. Do not submit recurrence expression; app expands dates into occurrences. Volume requests require commissioned metered valve and current healthy/fresh meter evidence; runner uses safe duration fixture only. Multi-valve, physical sequencer, restart recovery, actual meter delta, and stop acknowledgement need commissioned hardware and are out of mock coverage.

### Error contract assertions

For every error, assert non-2xx status, `api_version == "v1"`, nonempty `request_id`, and `error.code/message/details`; do not parse error response as a plan. Roadmap status map: 400 malformed, 401 auth, 404 unknown, 409 revision/active replacement conflict, 422 valid JSON violating scheduling/capability rule, 500 unexpected internal failure. Server must not echo credentials. Exact field validation, status code for each validation edge, rate limiting, and request-ID generation remain Node-RED implementation details to verify against deployment.

## Runnable reference fake backend

Requires Python 3.8+, no third-party packages beyond the repository tools. From
repository root:

```sh
python tools/test_mobile_watering_plan_api.py     # HTTP client scenarios
PYTHONPATH=tools python tools/test_watering_plan_backend.py   # protocol + lifecycle conformance
```

The first starts and stops an in-process server, runs client-shaped requests,
and exits nonzero on assertion failure (expected ending: `PASS: ... mode=mock`).
The second drives the portable lifecycle vectors (`conformance-scenarios.json`)
against the same backend and is the suite a real backend must also pass.

Standalone fake backend for a phone on the same Wi-Fi (advances the clock so
plans start and finish):

```sh
python tools/mock_watering_plan_api.py --host 0.0.0.0 --real-time
curl -i -H 'Authorization: Bearer test-token' -H 'Accept: application/json' http://127.0.0.1:8088/api/v1/health
```

Scenarios cover auth, envelopes, health/device snapshot, create, same-content
retry, changed duplicate conflict, stale/higher revision, schema/path/target/time
validation, list/detail/pagination, future cancel, and active stop request; the
conformance suite adds the full run lifecycle (duration, expiry, volume + meter,
active cancel, delayed + recovery, and the fault paths). The timeout case
deliberately stores then closes the connection; the runner retries the exact same
body/ID/revision with a fresh request ID. `X-Mock-Scenario` is test-only and must
never be added to app requests. The fake backend simulates the sequencer and
meter; it does not query InfluxDB or actuate valves.

## Live deployed endpoint mode

Only use against an explicitly authorized test gateway, not production. It creates unique plans scheduled two days in future, checks reads/revision behavior, and cancels created future plans. Do not run during commissioning or against a live active system without operator approval. Supply token via environment to avoid shell-history exposure:

```sh
export IRRIGATION_PLAN_API_URL='https://<test-gateway>'
export IRRIGATION_PLAN_API_TOKEN='<test-token>'
python tools/test_mobile_watering_plan_api.py --live
```

Equivalent flags exist, but environment variables preferred. Script never prints token. HTTP bearer warning appears for `http://`; only use on isolated trusted LAN. A connection refusal, 404 route, or incompatible envelope is a live failure—not a mock pass. Current repository has no route to target, so live run remains unavailable until Node-RED API is deployed and URL/token provided. Live mode intentionally skips mock-only forced timeout/internal-error and running-state injection because those require test hooks or unsafe control of scheduler state.

## Regression checks and result interpretation

```sh
PYTHONPATH=tools python -m unittest test_watering_plan_api test_watering_plan_store test_watering_plan_scheduler -v
node tools/test_watering_plan_contract.js
PYTHONPATH=tools python tools/test_watering_plan_backend.py
```

Report results in three separate lines: mock scenario result; Python prototype regression result; live Node-RED result (`not run`, `unavailable`, or observed pass/fail). Passing first two does not establish Node-RED route behavior. Before release, execute live suite plus manual cases for active stop acknowledgement, Pi restart, radio/meter loss, and no auto-resume on commissioned hardware.
