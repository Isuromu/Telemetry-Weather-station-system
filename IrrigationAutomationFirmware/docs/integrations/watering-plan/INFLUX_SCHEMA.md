# InfluxDB schema for irrigation telemetry and operation events

InfluxDB v2.9.1 is local to the Raspberry Pi. It is currently almost empty and
carries one demo write from the MainValve flow (`org=amudario`,
`bucket=main_valve`, `measurement=main_valve_data`, field `freq`). That demo is
left untouched; this document defines the buckets and measurements the Phase 6
server and the Node-RED adapter will use.

InfluxDB is the telemetry and operation-event store. It is **not** the plan
store; plans and lifecycle live in SQLite.

## Buckets

| Bucket | Retention | Purpose |
|---|---|---|
| `irrigation_telemetry` | 90 days | Raw device telemetry, one point per device report |
| `irrigation_events` | 365 days | Operation requests, acknowledgements, sequencer phases, results |

Keep the existing `main_valve` bucket. Do not rename or delete it. Both new
buckets use organization `amudario`.

Use scoped InfluxDB API tokens, not the admin username/password:

- Node-RED: write access to `irrigation_telemetry` and `irrigation_events`.
- Server: read access to both, write access to `irrigation_events` only if it
  also mirrors plan lifecycle events.

Never put the token in flow JSON, templates, logs, or git. In
`node-red-contrib-influxdb` the token belongs in the config node credentials.
The InfluxDB admin password is for the local UI only; it must not be reused as
an application token.

## Design rules

- Tags carry low-cardinality, queryable dimensions only.
- Fields carry values. Never put free-form messages or raw payloads in tags.
- Stable field types per measurement. Booleans are written as `0`/`1` integers.
- Timestamps are UTC. Telemetry uses the device observation time; events use
  the event time, not ingestion time.
- One measurement per signal class, not per device.
- `operation_id`, `plan_id`, and `plan_revision` are tags because operations are
  queried by them. They grow with the number of runs; event retention caps
  cardinality. At a few runs per day this is modest.
- Do not mirror raw Node-RED logs. Use journald/rotating files for logs; write
  only operation-relevant events here.

## Measurement: `device_telemetry`

Bucket: `irrigation_telemetry`.

Source: ChirpStack `event/up` MQTT messages. A site-local capture with real
payloads exists at `docs/CHIRPSTACK_DEVICE_EVENTS.md`, but it is intentionally
not committed because it contains DevEUIs, tenant, application, and gateway
identifiers. The point timestamp is the envelope `time` (ChirpStack receive
time), not `rxInfo[].nsTime` and not ingestion time.

Tags:

| Tag | Values | Notes |
|---|---|---|
| `site_id` | `s01` | Configured per deployment |
| `device_id` | `main_valve`, `pump`, `valve1`, `valve2`, `soil`, `water_level` | Stable logical key used by the sequencer |
| `device_type` | `main_valve`, `pump`, `valve`, `soil`, `water_level` | Query dimension |
| `field_id` | `field-001` or empty | Only for commissioned field mappings |
| `dev_eui` | device EUI | Correlates with ChirpStack; supplied at runtime, never committed |

Radio metadata is lifted from `rxInfo[0]`; keep the first gateway entry.

Fields — keep the decoded `object` key names. Types are fixed per field across
devices to avoid InfluxDB field-type conflicts. Booleans become `0`/`1`.
Omit a field when the source is `null`; never coerce absent meter data to zero.

Common fields for every device:

| Field | Source | Type |
|---|---|---|
| `rssi_dbm` | `rxInfo[0].rssi` | integer |
| `snr_db` | `rxInfo[0].snr` | float |
| `f_port` | `fPort` | integer |
| `f_cnt` | `fCnt` | integer |
| `received_at_ms` | server/Node-RED receive time | integer |

Main Valve (`device_id=main_valve`):

| Field | Source key | Type |
|---|---|---|
| `pressure_bar` | `pressure_bar` | float |
| `pressure_temperature_c` | `pressure_temperature_c` | float |
| `pressure_valid` | `pressure_valid` | integer 0/1 |
| `actual_angle_deg` | `actual_angle_deg` | float |
| `target_angle_deg` | `target_angle_deg` | float |
| `valve_moving` | `valve_moving` | integer 0/1 |
| `actuator_online` | `actuator_online` | integer 0/1 |
| `actuator_fault_code` | `actuator_fault_code` | integer |
| `actuator_mode` | `actuator_mode` | string |
| `rs485_bus_mode` | `rs485_bus_mode` | integer 0/1 |
| `command_phase` | `command_phase` | string |
| `reason` | `reason` | string |
| `reason_code` | `reason_code` | integer |
| `reported_command_id` | `reported_command_id` | integer |
| `last_command_id` | `last_command_id` | integer |
| `overpressure` | `overpressure` | integer 0/1 |
| `lorawan_active` | `lorawan_active` | integer 0/1 |
| `class_c_active` | `class_c_active` | integer 0/1 |
| `protocol_version` | `protocol_version` | float |

Pump (`device_id=pump`):

| Field | Source key | Type |
|---|---|---|
| `commanded_frequency_hz` | `commanded_frequency_hz` | float |
| `actual_frequency_hz` | `actual_frequency_hz` | float |
| `motor_current_a` | `motor_current_a` | float |
| `output_voltage_v` | `output_voltage_v` | float |
| `vfd_fault_code` | `vfd_fault_code` | integer |
| `running` | `running` | integer 0/1 |
| `run_state` | `run_state` | string |
| `command_result` | `command_result` | string |
| `communication_ok` | `communication_ok` | integer 0/1 |
| `communication_error_code` | `communication_error_code` | integer |
| `configuration_valid` | `configuration_valid` | integer 0/1 |
| `frequency_armed` | `frequency_armed` | integer 0/1 |
| `manual_mode` | `manual_mode` | integer 0/1 |
| `lorawan_active` | `lorawan_active` | integer 0/1 |
| `class_c_active` | `class_c_active` | integer 0/1 |
| `last_command_id` | `last_command_id` | integer |
| `join_attempt_count` | `join_attempt_count` | integer |
| `previous_join_error` | `previous_join_error` | string |
| `previous_join_error_code` | `previous_join_error_code` | integer |
| `previous_join_retry_seconds` | `previous_join_retry_seconds` | float |

Valve 1 and Valve 2 (`device_id=valve1|valve2`):

| Field | Source key | Type |
|---|---|---|
| `battery_voltage_v` | `battery_voltage_v` | float |
| `battery_soc_percent` | `battery_soc_percent` | float |
| `water_velocity_m_s` | `water_velocity_m_s` | float |
| `flow_rate_m3_h` | `flow_rate_m3_h` | float |
| `flow_total_since_reset_m3` | `flow_total_since_reset_m3` | float |
| `upstream_pressure_bar` | `upstream_pressure_bar` | float |
| `downstream_pressure_bar` | `downstream_pressure_bar` | float |
| `upstream_temperature_c` | `upstream_temperature_c` | float |
| `downstream_temperature_c` | `downstream_temperature_c` | float |
| `upstream_scale_validated` | `upstream_scale_validated` | integer 0/1 |
| `downstream_scale_validated` | `downstream_scale_validated` | integer 0/1 |
| `tuf_error_bits` | `tuf_error_bits` | integer |
| `pcv_last_commanded` | `pcv_last_commanded` | string |
| `pcv_position_verified` | `pcv_position_verified` | integer 0/1 |
| `status_reason` | `status_reason` | string |
| `runtime_mode` | `runtime_mode` | string |
| `sleep_seconds` | `sleep_seconds` | float |
| `sleep_minutes` | `sleep_minutes` | float |
| `last_command_id` | `last_command_id` | integer |
| `protocol_version` | `protocol_version` | float |

Use `flow_total_since_reset_m3` as the canonical volume baseline. Do not write
`flow_total_since_reset_liters` as a second source of truth. A `null` total means
meter evidence is unavailable; volume plans must be rejected or stopped rather
than treating it as zero.

Soil (`device_id=soil`):

| Field | Source key | Type |
|---|---|---|
| `temperature_c` | `temperature_c` | float |
| `ec_ms_cm` | `ec_ms_cm` | float |
| `vwc_percent` | `vwc_percent` | float |
| `battery_voltage_v` | `battery_voltage_v` | float |
| `sensor_valid` | `sensor_valid` | integer 0/1 |
| `runtime_mode` | `runtime_mode` | string |

Water Level (`device_id=water_level`):

| Field | Source key | Type |
|---|---|---|
| `water_level_percent` | `water_level_percent` | float |
| `depth_m` | `depth_m` | float |
| `pressure_bar` | `pressure_bar` | float |
| `pressure_valid` | `pressure_valid` | integer 0/1 |
| `load_on` | `load_on` | integer 0/1 |
| `battery_voltage_v` | `battery_voltage_v` | float |
| `runtime_mode` | `runtime_mode` | string |
| `protocol_version` | `protocol_version` | float |

## ChirpStack event caveats

- **ACK is only produced for `confirmed:true` downlinks.** Current commands use
  `confirmed:false`, so there may be no `event/ack`. Do not treat a missing ACK
  as a failed command.
- Device acceptance is confirmed by the next UP: `reported_command_id` and
  `command_phase` on the Main Valve, `last_command_id` plus `pcv_last_commanded`
  on the valves, and `last_command_id` plus `command_result` on the pump.
- `event/txack` confirms transmission to the gateway, not device execution.
- `event/log` carries codec and network errors. Mirror those as
  `operation_event` with `outcome=error` when they relate to an active
  `operation_id`; otherwise leave them in Node-RED logs.
- `deduplicationId` is the natural idempotency key for telemetry ingestion.
  Use it to avoid duplicate points on MQTT redelivery; it does not need to be a
  tag.

## Measurement: `operation_event`

Bucket: `irrigation_events`.

Tags:

| Tag | Example | Notes |
|---|---|---|
| `operation_id` | UUID | Stable across retries |
| `plan_id` | UUID | Empty for manual dashboard runs |
| `plan_revision` | `1` | Empty when not applicable |
| `event_type` | `requested`, `accepted`, `refused`, `started`, `stopping`, `stopped`, `completed`, `failed`, `timeout` | Query dimension |
| `source` | `server`, `node_red`, `dashboard` | Who caused the event |
| `outcome` | `ok`, `refused`, `error` | May be empty for in-flight events |

Fields:

| Field | Type | Meaning |
|---|---|---|
| `message` | string | Short operator-readable reason |
| `detail_json` | string | Bounded structured detail; never credentials or raw payloads |
| `duration_ms` | integer | Elapsed time where meaningful |
| `duplicate` | integer 0/1 | Retry resolved to an existing operation |

## Measurement: `command_ack`

Bucket: `irrigation_events`.

Tags:

| Tag | Example |
|---|---|
| `operation_id` | UUID |
| `device_id` | `main_valve`, `pump`, `valve1` |
| `command` | `open`, `close`, `start`, `stop` |
| `result` | `accepted`, `refused`, `lost` |

Fields: `latency_ms` (integer), `reason` (string, bounded).

## Measurement: `sequencer_phase`

Bucket: `irrigation_events`.

Tags:

| Tag | Example |
|---|---|
| `operation_id` | UUID |
| `phase` | `readiness`, `open`, `start`, `running`, `stop`, `close`, `teardown`, `fault` |
| `device_id` | `main_valve`, `pump`, `valve1`, or empty |

Fields: `ok` (integer 0/1), `detail` (string, bounded).

## Cardinality and retention

- Never tag `message`, `reason`, timestamps, raw JSON, or device EUIs.
- Event measurements use `operation_id`, so series grow per run. The 365-day
  retention is a deliberate cap; reduce it if cardinality becomes a problem.
- No downsampling task is required initially. Add a retention/downsampling task
  only after measuring daily point volume.

## Line protocol examples

```text
device_telemetry,site_id=s01,device_id=valve1,device_type=valve,field_id=field-001,dev_eui=<dev-eui> downstream_pressure_bar=0.0,flow_total_since_reset_m3=12.345,flow_rate_m3_h=1.2,battery_voltage_v=12.7,rssi_dbm=-67i,snr_db=7.75,f_port=31i,f_cnt=5931i 1760700000000000000
operation_event,operation_id=1f0c...,plan_id=9ab2...,plan_revision=1,event_type=started,source=node_red,outcome=ok duration_ms=4200i 1760700004200000000
command_ack,operation_id=1f0c...,device_id=main_valve,command=open,result=accepted latency_ms=850i 1760700000850000000
sequencer_phase,operation_id=1f0c...,phase=running,device_id=valve1 ok=1i,detail="flow stable" 1760700005000000000
```

## Read patterns

Latest per device, then freshness in the server:

```flux
from(bucket: "irrigation_telemetry")
  |> range(start: -15m)
  |> filter(fn: (r) => r._measurement == "device_telemetry")
  |> last()
```

Operation timeline:

```flux
from(bucket: "irrigation_events")
  |> range(start: -24h)
  |> filter(fn: (r) => r.operation_id == "<operation-id>")
  |> sort(columns: ["_time"])
```

Freshness rule: a device is `stale` when `now - _time` exceeds its configured
limit; it is `unavailable` when no point exists in the query window. Never
present cached or stale readings as live.

## Node-RED write path

Use the already installed `node-red-contrib-influxdb` 0.7.0 nodes. Add new
nodes in the existing irrigation flow; do not remove or rewrite dashboard nodes.
Use a separate `influxdb out` node per measurement (telemetry, operation event,
command acknowledgement, sequencer phase) so measurement naming stays explicit.
Configure the token in the InfluxDB config node credentials, and set the
organization, bucket, and millisecond precision.

The adapter must write:

1. `operation_event` when a command is requested, accepted, refused, started,
   stopping, stopped, completed, failed, or times out.
2. `command_ack` for every device acknowledgement or lost downlink.
3. `sequencer_phase` on each ordered-sequence transition.
4. `device_telemetry` for device uplinks used by sequencing and result checks.

## Verification

```sh
# Write a test point (token from the environment; never echo it)
influx write --org amudario --bucket irrigation_events \
  'operation_event,operation_id=verify,event_type=requested,source=node_red,outcome=ok message="schema check"'

# Query it back
influx query --org amudario '
from(bucket:"irrigation_events")
  |> range(start:-10m)
  |> filter(fn:(r)=>r.operation_id=="verify")'

# Cardinality check
influx query --org amudario 'import "influxdata/influxdb" influxdb.cardinality(bucket:"irrigation_events", start:-30d)'
```

Acceptance: a supervised duration run produces a queryable operation timeline
with request, acknowledgement, phase transitions, and a terminal outcome.
