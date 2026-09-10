# PCV end-node runtime variants

## Selection table

| PlatformIO environment | Control inputs | LoRaWAN class | Interval meaning | ESP32 state between cycles |
|---|---|---|---|---|
| `pcv_serial_only` | Serial | disabled | none | awake |
| `pcv_hybrid_class_c` | Serial + Gateway | Class C | telemetry/report interval | awake; SX1262 continuously receiving |
| `pcv_low_power_class_a` | Gateway; Serial output only | Class A | real deep-sleep time | timer deep sleep |

All three builds use the same `PressureControlValve`, sensor, battery,
TUF-2000M and payload-protocol implementations. They differ only in runtime
policy. A successful latching pulse records `last commanded`; it is not
physical PCV-position feedback.

## Serial-only firmware

Build with:

```text
pio run -e pcv_serial_only
```

The SX1262 is never initialized. Serial Monitor remains active at 115200 baud
and accepts all pressure-node commands. No LoRaWAN credentials or ChirpStack
codec are required.

## Hybrid Class C firmware

Build with:

```text
pio run -e pcv_hybrid_class_c
```

Operating sequence:

```text
boot -> initialize hardware -> OTAA join/restore -> select Class C
     -> send initial status -> keep SX1262 in continuous receive
loop -> accept Serial commands and Class C downlinks in parallel
timer -> read sensors and send periodic status
command -> pulse PCV/change report interval/reset local flow total
        -> immediately send result status
```

The ESP32 and SX1262 receiver remain powered. This gives low command latency
but is the highest-consumption variant. Short synchronous operations such as a
sensor read, PCV pulse, or uplink can briefly delay Serial parsing; a received
Class C packet is latched by the radio interrupt and processed by the loop.

Configure the ChirpStack device profile as Class C and install
`tools/chirpstack/pcv_hybrid_class_c_codec.js`. Example JSON:

```json
{"pcv":"open","command_id":1001}
```

```json
{"pcv":"close","report_interval_minutes":10,"command_id":1002}
```

`report_interval_minutes` controls only periodic status. It never causes ESP32
or radio sleep.

## Low-power Class A firmware

Build with:

```text
pio run -e pcv_low_power_class_a
```

Operating sequence, matching the supplied soil-node pattern:

```text
timer/cold boot -> initialize hardware and restore/join LoRaWAN
                -> read battery, pressures and commissioned TUF-2000M
                -> send status on FPort 31
                -> receive one queued command on FPort 30 in RX1/RX2
                -> execute PCV/interval/flow-total-reset command
                -> immediately send acknowledgement status with command ID
                -> force H-bridge safe, sleep SX1262, enter ESP32 deep sleep
```

Serial prints diagnostics while the CPU is awake but does not accept commands.
The Gateway cannot reach a sleeping Class A node immediately: ChirpStack holds
the downlink until the next uplink opens RX1/RX2. Queue only one application
command at a time because the acknowledgement uplink also opens receive
windows, just as in the supplied soil firmware.

Install `tools/chirpstack/pcv_low_power_class_a_codec.js`. Example JSON:

```json
{"pcv":"open","command_id":2001}
```

```json
{"pcv":"close","sleep_minutes":10,"command_id":2002}
```

`sleep_minutes` is real ESP32 timer deep sleep. Accepted values are 1-1440
minutes. Normal command latency is up to the remaining sleep duration plus
network timing.

## Credentials and retained state

1. Copy `config/PressureNodeLoRaSecrets.example.h` to
   `config/PressureNodeLoRaSecrets.h`.
2. Enter unique JoinEUI, DevEUI and AppKey for this PCV end node.
3. Set `CONFIGURED = true` only after replacing all placeholders.
4. Never reuse credentials from the FC11C or soil node.

The real secrets file is ignored by Git. OTAA nonce state is stored in NVS.
The complete RadioLib session is mirrored to RTC memory after joins, uplinks
and downlinks. The interval, last accepted command, last successfully
commanded PCV state, and local flow-total baseline are stored in NVS only when
they change.

## Shared FPort 30 command

The command is exactly 10 bytes, big-endian:

| Byte | Field | Meaning |
|---:|---|---|
| 0 | version | `2` |
| 1 | flags | bit 0 PCV action; bit 1 interval; bit 2 flow-total reset |
| 2 | PCV action | `0` none, `1` open, `2` close |
| 3 | reserved | must be `0` |
| 4-7 | interval seconds | 60-86400 with bit 1 set; otherwise zero |
| 8-9 | command ID | unsigned 16-bit identifier |

Every new command must use a new `command_id`. An exact duplicate is reported
but cannot pulse the solenoid twice. Reusing the same ID with different bytes
is rejected. In Class C the interval is a report interval; in Class A it is
deep-sleep time. Bit 2 reads the current positive TUF accumulator and stores it
as the new local NVS baseline; it does not erase the meter.

Example reset JSON in either matching codec:

```json
{"flow_total_reset":true,"command_id":3001}
```

## Shared FPort 31 status

The protocol v2 status is exactly 32 bytes, big-endian:

| Bytes | Field | Scale/sentinel |
|---:|---|---|
| 0 | version | `2` |
| 1 | validity flags | pressure, battery, flow, scale and diagnostic flags |
| 2 | PCV last commanded | `0` unknown, `1` open, `2` closed |
| 3 | status reason | startup/report/command result/error |
| 4-5 | upstream pressure | signed x100 bar; `0x8000` unavailable |
| 6-7 | upstream water temperature | signed x100 degrees C; `0x8000` unavailable |
| 8-9 | downstream pressure | signed x100 bar; `0x8000` unavailable |
| 10-11 | downstream water temperature | signed x100 degrees C; `0x8000` unavailable |
| 12-13 | battery voltage | millivolts; `0xFFFF` unavailable |
| 14-17 | flow rate | signed x1000 m3/h; `0x80000000` unavailable |
| 18-19 | water velocity | signed x1000 m/s; `0x8000` unavailable |
| 20-21 | TUF error bits | REG0072; `0xFFFF` unavailable |
| 22-25 | interval seconds | report interval or deep-sleep time by variant |
| 26-27 | last command ID | `0xFFFF` if none accepted |
| 28-31 | volume since local reset | unsigned litres; `0xFFFFFFFF` unavailable |

The codecs expose battery voltage in volts. The XDB401 engineering scale is
still unvalidated, so pressure values must not yet be treated as trusted PCV
feedback. They expose the volume as both `flow_total_since_reset_m3` and
`flow_total_since_reset_liters`.

## Hardware notes

- L298N IN1 is GPIO2 and IN2 is GPIO15, with the installed 20 kOhm pull-downs.
- GPIO27 must keep the L298N supply OFF through reset and deep sleep.
- UART2 RS485 RX is GPIO16 and TX is GPIO17.
- TUF-2000M M46 address 1, 9600 8N1, and MODBUS_RTU are confirmed. Normal
  flow/velocity and accumulated-volume telemetry use the hardware-confirmed
  `LOW_WORD_FIRST` decoder. The assembled ESP32 link is working.
- Class C current must be measured on the assembled battery/regulator/radio
  hardware before deployment.
