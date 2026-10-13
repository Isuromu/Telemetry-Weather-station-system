# React Native watering-plan integration

## Purpose and scope

This guide lets the mobile team implement watering-plan screens before the
production server exists. The app talks to either the temporary Node-RED API or
the later server through the same `/api/v1` contract. Only the connection
profile changes.

Use this document with:

- [Mobile design reference](MEVACHILIK_FIGMA_REFERENCE.md)
- [Watering-plan roadmap](../watering-plan/AI_WATERING_PLAN_ROADMAP.md)
- [Farm spatial-data model](FARM_SPATIAL_DATA_MODEL.md)

The app owns human plan authoring, AI proposal generation/presentation, user
approval, recurrence expansion, and the calendar presentation. The backend
receives only approved, dated executable occurrences.

## Connection profile

Keep this in one module, for example `src/config/irrigationApi.ts`. Do not put
backend-mode conditionals into screens or plan business logic.

```ts
export type IrrigationConnectionProfile = {
  baseUrl: string;
  apiVersion: "v1";
  getAccessToken: () => Promise<string | null>;
};
```

| Profile | `baseUrl` | Credential | Use |
|---|---|---|---|
| `localFake` | `http://<dev-machine-lan-ip>:8088` | `test-token` | Reference fake backend on a laptop during app work. Temporary; never a production profile. |
| `temporaryLan` | Node-RED API on the Pi LAN address | QR-provisioned prototype token | Current onsite testing. |
| `production` | Server URL | Server-issued access token | After server launch. |

All profiles send `Authorization: Bearer <token>` and call the same paths.

`localFake` runs `python3 tools/mock_watering_plan_api.py --host 0.0.0.0
--real-time`, which serves the same `/api/v1` contract over the portable rules
in `../watering-plan/api-protocol.rules.json` and simulates plan runs, so the app
can exercise a full lifecycle without Node-RED or the Pi. Remove the profile
before any real deployment.
Store the profile credential in iOS Keychain / Android Keystore through the
application's secure-storage library. Do not store it in AsyncStorage, logs,
analytics, screenshots, or repository configuration.

## API client rules

The repository includes a typed, transport-only client at
[`irrigationApi.ts`](irrigationApi.ts). Configure it once with the selected
connection profile and an access-token getter backed by the app's secure
storage. The module accepts an injected `fetch` and request-ID factory for
platform setup and mock-server integration. It does not store credentials,
retry requests, or create a new plan identity; the app owns those decisions.

All requests use JSON and these headers:

```http
Authorization: Bearer <token>
Content-Type: application/json
Accept: application/json
X-Request-ID: <new UUID for this HTTP attempt>
```

The backend response envelope is always one of:

```ts
type ApiSuccess<T> = {
  api_version: "v1";
  request_id: string;
  data: T;
};

type ApiFailure = {
  api_version: "v1";
  request_id: string;
  error: {
    code: string;
    message: string;
    details: unknown[];
  };
};
```

`X-Request-ID` helps diagnose one HTTP attempt. It is not idempotency. Plan
idempotency comes from `plan_id` and `revision` inside the plan payload.

## Plan model

The app creates a new UUID for each dated occurrence. It never reuses a plan ID
for a different date. A user edit increments `revision`; a retry keeps the same
payload, `plan_id`, and `revision`.

```ts
type StopRule =
  | { type: "duration"; value: number; unit: "seconds" | "minutes" | "hours" }
  | { type: "volume"; value: number; unit: "m3" };

type WateringPlanRequest = {
  schema_version: 1;
  plan_id: string;
  revision: number;
  status: "scheduled";
  source: "human" | "ai";
  plan_group_id?: string;
  summary?: string;
  created_at: string;
  timezone: "Asia/Tashkent" | string;
  start_at: string;
  valid_until: string;
  targets: {
    mode: "selected";
    field_ids: string[];
  };
  run_mode: "parallel";
  stop: StopRule;
  hard_max_runtime_sec: number;
};
```

The UI selects a logical field. Before submission, the app sends the logical
`field_ids`; the API resolves them through the gateway registry. The first
prototype maps `field-001` to `valve1`. Do not derive valve IDs from a displayed
field name. A future field-catalog API can replace local field configuration
without changing the plan request shape.

Use ISO 8601 timestamps with an explicit offset, for example
`2026-10-10T06:00:00+05:00`, and include `timezone: "Asia/Tashkent"`. The
prototype creates `valid_until` as `start_at + 10 hours`.

Version 1 limits:

- A duration plan may select one or more commissioned valves.
- A volume plan selects exactly one commissioned metered valve. Initially this
  is `valve1` / `field-001` only.
- Moisture stop rules are visible in the design but must not be sent in version
  1.
- `run_mode` is always `parallel`.
- The requested `hard_max_runtime_sec` is mandatory; the backend may apply a
  stricter commissioned cap.

## Plan lifecycle calls

### Health and connection check

Call `GET /api/v1/health` when the app pairs, foregrounds, or needs to explain
why a plan cannot be submitted. Show a clear blocked state if the Pi clock is
unsynchronized; scheduled plans depend on it.

### Device-state refresh: polling only

The app receives no pushed device-state updates in version 1. It calls:

```http
GET /api/v1/device-state
```

The temporary Node-RED implementation and later server implementation both
build this response from local InfluxDB. They do not subscribe the mobile app
to Mosquitto or give it direct InfluxDB access.

```ts
type DeviceStateSnapshot = {
  generated_at: string;
  pump?: {
    state: string;
    observed_at: string;
    is_stale: boolean;
  };
  fields: Array<{
    field_id: string;
    valve_id: string;
    valve_state?: string;
    flow_m3_h?: number;
    delivered_volume_m3?: number;
    soil_moisture_percent?: number;
    observed_at: string;
    is_stale: boolean;
  }>;
};
```

Poll only while a screen is visible:

| Visible screen | Refresh interval |
|---|---|
| Active irrigation / live dashboard | 5 seconds |
| Idle dashboard | 15 seconds |
| Calendar or past-plan list | Do not poll device state; refresh plans on foreground or every 30 seconds. |
| Background app | Do not poll. Refresh when the app returns to foreground. |

Always show or retain a last-updated time. Treat `is_stale: true`, a missing
record, or an old `observed_at` value as unknown state; do not render it as a
confirmed stopped pump, closed valve, or zero flow. The server/Node-RED decides
the staleness threshold from device telemetry expectations and exposes that
result in the snapshot.

### Create or edit a future occurrence

```http
PUT /api/v1/watering-plans/0a7a2a88-1d24-4a0f-9716-6ea9a1ce9c73
```

The path ID must equal `body.plan_id`.

- `201`: new accepted occurrence.
- `200`: identical retry or accepted higher revision of a future occurrence.
- `409`: changed duplicate revision, stale revision, or an attempted edit of
  an active occurrence. Refresh the plan before presenting an edit choice.
- `422`: named capability, time, clock, or schema-policy rejection. Show the
  returned error code/message and leave the unsent draft editable.

The app may mark a plan as “saved to controller” only after `200` or `201`.
Network timeout does not prove failure: read the plan by `plan_id`, then retry
the exact same request only when it is absent.

### Cancel

```http
POST /api/v1/watering-plans/{plan_id}/cancel
```

- A future plan becomes `cancelled`.
- An active plan returns `202` and becomes `stop_requested`.
- `stop_requested` is not a physical-stop confirmation. Poll until a final
  state is returned.

### Read plan state

```http
GET /api/v1/watering-plans
GET /api/v1/watering-plans/{plan_id}
```

Use the collection endpoint for the calendar/upcoming list and the detail
endpoint for a running or completed-plan screen. Do not infer success from a
previous create response.

The collection accepts optional `from` and `to` ISO 8601 timestamps, `limit`
(default 50, maximum 100), and an opaque `cursor`. Results are ordered by
`start_at` ascending and then `plan_id`; the response returns `next_cursor` or
`null`. Keep this behavior identical in the Node-RED and server implementations.

The read `data` object is a plan record. It contains the submitted plan fields
plus these backend-owned fields:

```ts
type WateringPlanRecord = WateringPlanRequest & {
  state:
    | "scheduled"
    | "delayed"
    | "starting"
    | "running"
    | "stop_requested"
    | "stopping"
    | "completed"
    | "cancelled"
    | "expired"
    | "failed";
  accepted_at: string;
  updated_at: string;
  delay_reason?: string;
  execution?: {
    operation_id?: string;
    actual_started_at?: string;
    actual_ended_at?: string;
    actual_volume_m3?: number;
    stop_reason?: string;
    failure_code?: string;
    telemetry_observed_at?: string;
  };
};
```

For `GET /api/v1/watering-plans`, `data` is instead:

```ts
type WateringPlanCollection = {
  items: WateringPlanRecord[];
  next_cursor: string | null;
};
```

The expected state names are:

| State | App meaning |
|---|---|
| `scheduled` | Stored and waiting for `start_at`. |
| `delayed` | Due, but a named gate prevents start; display the reason. |
| `starting` | Ordered valve/pump sequence was requested. |
| `running` | The sequencer reports an active run. |
| `stop_requested` / `stopping` | Stop is in progress. |
| `completed` | Final result available. |
| `cancelled` | Future occurrence was cancelled. |
| `expired` | It could not start before `valid_until`. |
| `failed` | A named command, telemetry, or teardown failure occurred. |

## AI plan and calendar behavior

The AI proposal may exist only in the mobile app. On the review screen, the
app shows its rationale, weather/sensor freshness, proposed dates, and the
explicit confirm action. Do not send the draft proposal to Node-RED or the
server.

On confirmation:

1. Generate one `plan_group_id` for the confirmed recommendation.
2. Expand each proposed date into one `WateringPlanRequest` with its own UUID
   `plan_id`.
3. Set `source: "ai"` and include a short optional `summary`.
4. Submit each occurrence. Mark the group as saved only after every required
   occurrence receives `200` or `201`; otherwise show which dates need retry.

Human recurrence works the same way: expand it into dated occurrences before
submitting. The Pi never receives a recurrence expression.

## Refresh and offline behavior

- Poll a running, starting, or stopping plan every 5 seconds while its screen
  is visible.
- Poll an upcoming/calendar list on foreground, explicit refresh, and every
  30 seconds while visible. These intervals are UI defaults, not hardware
  control timing.
- Cache last successful reads for display with a visible “last updated” time.
- Queue a plan only as a local unsent draft when offline. Do not call it saved
  or scheduled until the API accepts it.
- Retry a submission after a transport failure with exactly the same plan body.
- Never issue a second immediate-start command because a response was slow.

## Mapping to the supplied design

| Design screen | Required app behavior |
|---|---|
| Schedule form | Validate date, target, duration/volume rule, and 10-hour prototype delay window before submission. |
| AI plan review | Keep proposal local until user confirms. |
| AI plan saved | Group successfully accepted dated occurrences by `plan_group_id`. |
| Daily plan | Render returned plan state plus actual volume/duration when available. |
| Start confirmation | Use an explicit confirmation before creating an immediate occurrence. |
| Connection lost | Preserve local drafts and show the last successful refresh time. |

## What is intentionally not in this first client contract

- Direct ChirpStack, MQTT, InfluxDB, or Node-RED-editor access from the app.
- Mobile-issued valve/pump commands outside an approved plan or the existing
  manual-control API when it is separately specified.
- Moisture-stop submission.
- Server storage of AI prompts or detailed proposal content.
- Map-tile, imagery, field-boundary editing, and GPS-track APIs. The spatial
  model is documented separately and can be integrated later.

## Mobile implementation checklist

1. Implement one typed API client and one connection-profile module.
2. Implement QR scan/manual LAN setup and secure token storage for the
   temporary profile.
3. Implement plan request validation and recurrence/AI occurrence expansion.
4. Implement create, read, retry, edit-future, cancel, and active-stop flows.
5. Implement state/error presentation using returned codes, never guessed
   device state.
6. Test against a mock `/api/v1` server first, then the Node-RED endpoint with
   duplicate delivery, timeout retry, delayed plan, active cancellation, and
   Pi restart cases.

## Phase 5 field commissioning record

The client module completes the repository-side API integration boundary. The
following checks require the actual React Native app and commissioned site
hardware; record date, software versions, plan IDs, and observed outcomes
before enabling broader targets:

1. Confirm the app stores the QR-provisioned token in iOS Keychain or Android
   Keystore, calls health successfully, and blocks scheduling when the Pi clock
   is unsynchronized.
2. Submit a short duration plan to `field-001` / `valve1`; verify the ordered
   sequencer phases and final result in the API and dashboard.
3. Submit a volume plan to `valve1`; verify the commissioned meter baseline,
   positive-total delta, target stop request, and final stop acknowledgement.
4. Exercise duplicate delivery and timeout retry with the same `plan_id`,
   revision, and body; then exercise delayed start, future cancellation, active
   stop request, Pi restart, radio loss, and flow-meter loss. Confirm no case
   resumes watering automatically after a restart or reports a requested stop
   as a confirmed physical stop.
5. Keep multi-valve duration disabled until the actual pump and pipe network
   demonstrate the requested valve set at its intended configuration. The
   current two-valve allow-list remains stress-test-only.

No site commissioning result is claimed by the repository implementation.
