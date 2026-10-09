# Mevachilik mobile design reference

## Source

Public prototype: [Mevachilik - Irrigation screens](https://www.figma.com/proto/EBbML1UYHF8ye1orO9YbVy/Mevachilik-%E2%80%A2-Irrigation-screens?node-id=1-44&starting-point-node-id=1%3A44).

The prototype is a product-design reference, not an API or hardware
specification. Its unconnected frames must not be treated as an implemented
mobile behavior until the React Native app contract is approved.

## Preserved mockup screenshots

These PNGs are a local review snapshot of the public prototype. They keep the
schedule discussion available when the live Figma design changes or is
temporarily inaccessible. The Figma file remains the visual source of truth.

| Screenshot | Reviewed behavior |
|---|---|
| [Live irrigation dashboard](figma-mockups/01_live_irrigation_dashboard.png) | Pump summary, field cards, current volume/target, and watering status. |
| [Field detail and schedule](figma-mockups/02_field_detail_and_schedule.png) | Field progress, schedule list, and AI-plan marker. |
| [Schedule plan form](figma-mockups/03_schedule_plan_form.png) | Date/time, stop-rule choice, target, estimate, recurrence, and add-to-calendar action. |
| [Start confirmation](figma-mockups/04_start_confirmation.png) | Target, expected duration, latest finish, assigned valve, and readiness checklist. |
| [Watering completed](figma-mockups/05_watering_completed.png) | Actual volume/duration, moisture change, stop reason, and final system state. |
| [Connection lost](figma-mockups/06_connection_lost.png) | Lost-connection presentation and retained automatic-stop expectation. |
| [Field map](figma-mockups/07_field_map.png) | Satellite base map, field-boundary polygons, per-field moisture colour, and field selection. |
| [Irrigation dashboard — pump stopped](figma-mockups/08_irrigation_dashboard_pump_stopped.png) | Pump-off summary and six-field list with moisture and watering-need status. |
| [Daily plan — completed](figma-mockups/09_daily_plan_completed.png) | Field/day agenda, completed execution, time zone, and delivered volume. |
| [AI plan saved](figma-mockups/10_ai_plan_saved.png) | One AI recommendation becomes three dated calendar schedules after confirmation. |
| [AI plan review](figma-mockups/11_ai_plan_review.png) | AI rationale, sensor/weather freshness, suitable window, forecast reassessment, and explicit user confirmation. |

## Watering-plan observations

The reviewed screens present human and AI plans as the same scheduling object.
The source of a plan changes its label and explanation, not its execution path.

Visible plan concepts are:

| Concept | Mobile presentation |
|---|---|
| Field | Named orchard/field and crop information. |
| Start | Date and start time, including an immediate-start path. |
| Stop rule | Time, water volume, or soil moisture choices in the mockup. |
| Target | A rule value, for example 40 cubic metres. |
| Estimate | Expected duration and finish time. |
| Recurrence | At least a one-time plan; the mobile application owns recurrence expansion. |
| AI provenance | An `AI plan` marker and a compact explanation. |
| Readiness | Controller, valve, metering, and pump readiness before start. |
| Progress | Delivered/target volume, status, and current field reading. |
| Completion | Actual volume, duration, before/after moisture, and stop reason. |
| Daily agenda | Date, field, completed/upcoming status, delivered volume, and the displayed local time zone. |
| AI proposal | A dated set of recommendations with rationale and data-freshness labels; it remains a draft until the user confirms. |

## Gateway scope chosen for version 1

The Raspberry Pi gateway receives only the approved, executable fields defined
in `../watering-plan/AI_WATERING_PLAN_ROADMAP.md`. It does not receive crop,
growth-stage, AI prompt, weather, or recurrence-rule records.

The mobile app expands recurrence and a confirmed multi-date AI recommendation
into individual dated plan occurrences. It sets `source` to `human` or `ai`,
uses one optional `plan_group_id` for occurrences from the same calendar or AI
recommendation, includes an optional short explanation, and sends `start_at`,
`valid_until`, target valves, stop rule, and hard runtime. The prototype shows
Tashkent time (UTC+5); the payload must retain the IANA time zone
`Asia/Tashkent` as well as its offset.

AI-only context such as sensor freshness, weather timestamp, forecast review,
and detailed agronomic rationale belongs to the mobile/future-server planning
record. The gateway receives the approved executable occurrences plus compact
display metadata; it does not decide whether to reassess a forecast.

The Figma moisture stop option is reserved for a later gateway schema revision.
Version 1 permits duration plans and a one-metered-valve volume plan only.

The handoff-ready mobile implementation guide is
[REACT_NATIVE_WATERING_PLAN_INTEGRATION.md](REACT_NATIVE_WATERING_PLAN_INTEGRATION.md).

## React Native integration responsibilities

- Store the paired local-gateway token in platform secure storage.
- Retry idempotent plan delivery with the same `plan_id` and `revision`.
- Treat gateway acknowledgement of active-plan cancellation as `stop requested`.
- Display the returned gateway state and final execution record; do not infer
  physical valve position from the client-side request.
- Generate prototype `valid_until` as `start_at + 10 hours` until the design
  exposes a user-controlled delay-window setting.
