# ChirpStack codecs for the PCV end node

Select the codec that matches the PlatformIO firmware environment. Both radio
variants use command FPort 30 and status FPort 31, but the interval has a
different meaning.

## `pcv_hybrid_class_c_codec.js`

Use only with `pcv_hybrid_class_c`. The ESP32 and SX1262 receiver remain on.
The interval controls periodic telemetry; it does not put the node to sleep.

```json
{"pcv":"open","command_id":1001}
```

```json
{"pcv":"close","command_id":1002}
```

```json
{"report_interval_minutes":10,"command_id":1003}
```

```json
{"pcv":"open","report_interval_minutes":10,"command_id":1004}
```

```json
{"flow_total_reset":true,"command_id":1005}
```

The ChirpStack device profile must have Class C support enabled. Serial
commands remain available at the same time.

## `pcv_low_power_class_a_codec.js`

Use only with `pcv_low_power_class_a`. The interval is real ESP32 deep-sleep
time. A queued command is delivered in RX1/RX2 after the next status uplink.
The current valve_1 commissioning build and codec allow 10-second sleep; this
temporary lower bound must return to at least 60 seconds before deployment.

```json
{"pcv":"open","command_id":2001}
```

```json
{"pcv":"close","command_id":2002}
```

```json
{"sleep_minutes":10,"command_id":2003}
```

```json
{"sleep_seconds":10,"command_id":2006}
```

```json
{"pcv":"close","sleep_minutes":10,"command_id":2004}
```

```json
{"flow_total_reset":true,"command_id":2005}
```

After receiving a command, the node sends an immediate acknowledgement status
containing the command result and `last_command_id`, then enters deep sleep.
Queue only one application command at a time because that acknowledgement also
opens another pair of Class A receive windows.

## Shared rules

- Every new command must use a new 16-bit `command_id`.
- Class C permits 60-86400 seconds. The current valve_1 Class A commissioning
  build permits 10-86400 seconds; 10 seconds is not a deployment setting.
- An exact duplicate is acknowledged but does not pulse the solenoid again.
- Reusing the same ID with different bytes is rejected.
- `{"pcv":"none","command_id":...}` is an explicit no-operation command;
  normally leaving the downlink queue empty is clearer.
- The field name `valve` is deliberately rejected to avoid confusing this PCV
  with the separate FC11C Main Valve.
- `pcv_last_commanded` is not physical PCV-position feedback.
- Battery voltage is decoded in volts, for example `12.435`.
- `flow_total_since_reset_liters` and `flow_total_since_reset_m3` come from
  the persistent ESP32 baseline. A reset does not erase the TUF accumulators.
- Both codecs implement protocol v2. Replace any previously installed v1
  codec before uploading the new firmware.

The `pcv_serial_only` firmware does not use ChirpStack or a codec.

## `pcv2_klapan_codec.js`

Not a PCV codec. This belongs to the separate `examples/PressureControlNode2`
bench node (`pio run -e pcv_low_power_class_a_without_flowmeter`), which
shares no code with the PCV
targets above. It uses FPort 10 for both directions, a 12-byte versioned
uplink, and JSON text downlinks such as `{"command":"open"}`. None of the
shared rules above apply: there is no `command_id`, no interval field, and no
flow totalizer. See `examples/PressureControlNode2/README.md`.
