# No-flow-meter PCV node (valve_2)

The `pcv_low_power_class_a_without_flowmeter` environment builds this
directory's `src/main.cpp` as a separate Class A application. It uses the
shared safe latching-pulse driver and protocol, dual pressure
sensors, battery monitor, FPort 30 commands and FPort 31 status. Flow fields
are unavailable and the RS485 flow-meter transport is not started.

Valve_2 is configured for the same 12 V latching solenoid and L298N pulse
sequence as valve_1. OPEN energizes IN1, CLOSE energizes IN2, and each command
applies a 250 ms pulse before switching bridge power off. The build explicitly
enables actuation with `PCV_NO_FLOW_ACTUATION_ENABLED=1`. These polarity and
timing values are provisional until verified on the installed valve_2 hardware;
the GPIO27 active level and reset pull-downs also need physical validation.
The superseded Class C source and its `lorawan_keys` files have been removed.

The Class A target reuses the main application's validated structure while
injecting `UnavailableFlowMeter`, separate valve_2 credentials, and valve_2's
historical `valve_lora` NVS namespace for OTAA nonce continuity. Its
application state uses the distinct `node_state` key. A missing prior nonce
buffer blocks OTAA; do not erase NVS during migration.

The Class A source is selected explicitly in `platformio.ini`.

## Build and upload

```text
pio run -e pcv_low_power_class_a_without_flowmeter -t upload
pio device monitor
```

Serial Monitor runs at 115200 baud for diagnostics only. The node wakes,
measures, transmits, receives one queued Class A downlink in RX1/RX2, sends an
application result status when a command arrives, and enters timer deep sleep.
The current commissioning default is 10 seconds. It does not accept Serial commands.

## Hardware

| Signal | ESP32 GPIO |
|---|---|
| Valve IN1 / IN2 | 2 / 15 |
| L298N power enable | 27 |
| Battery ADC | 35 |
| RS485 RX / TX | 16 / 17 |
| I2C before (SDA/SCL) | 21 / 22 |
| I2C after (SDA/SCL) | 13 / 4 |

| SX1262 | ESP32 GPIO |
|---|---|
| MOSI / MISO / SCK | 23 / 19 / 18 |
| NSS / RESET / BUSY / DIO1 | 5 / 14 / 25 / 26 |
| RXEN / TXEN | 33 / 32 |

Both XDB401 pressure sensors are 0-1 MPa / 0-10 bar. The I2C address is probed
at `0x7F` first, then `0x6D`.

Every pin in these two tables, plus the battery divider and calibration
constants, is declared in `include/PressureNode2Config.h`. That file is the
only place to change them.

## LoRaWAN

EU868, OTAA 1.0.x, Class A. Use the
`tools/chirpstack/pcv_low_power_class_a_codec.js` codec in a *separate* Class A
device profile. Command FPort is 30 and status FPort is 31. The previous
FPort-10 codec is not compatible. Flow-meter readings decode as `null`.

Use unique `command_id` values. The Class A node receives queued commands only
after an uplink; it cannot receive while asleep. `flow_total_reset` is rejected
because no meter is fitted. OPEN/CLOSE use the shared latching-valve driver.
Each accepted command, including a repeat OPEN or CLOSE with a new command ID,
sends a pulse; status records the last command, not confirmed physical position.

## Safe commissioning sequence

1. Keep the solenoid supply disconnected for the first radio and telemetry
   test. Do not erase NVS: the old OTAA nonce history is required.
2. Flash only the `pcv_low_power_class_a_without_flowmeter` environment.
   Confirm Serial prints `PCV_LOW_POWER_CLASS_A`, `No flow meter fitted`, and
   `Valve pulses enabled`.
3. In ChirpStack, use a separate Class A device profile with the FPort-30/31
   codec. Confirm join/session restore, FPort-31 uplink, battery/pressure data,
   null flow data, then timer deep sleep and wake after about 10 seconds with
   the current commissioning setting.
4. Queue a non-actuating downlink such as
   `{"pcv":"none","command_id":2001}`. On the next wake, verify the result
   FPort-31 uplink and matching `last_command_id`. Repeat the same command ID
   and verify the duplicate is ignored.
5. With the solenoid supply disconnected, verify the GPIO2/GPIO15 polarity
   sequence, GPIO27 power gating and pulse timing for OPEN and CLOSE. Verify
   the reset pull-downs and measure pulse current with the valve installed
   before hydraulic actuation under supervision. Set
   `PCV_NO_FLOW_ACTUATION_ENABLED=0` for radio-only commissioning if needed.

### Credentials

The active Class A firmware reads the git-ignored
`config/PressureNode2LoRaSecrets.h`. A fresh clone uses the all-zero
`include/PressureNode2LoRaSecrets.example.h` fallback; copy its structure
to the ignored config header and enter only valve_2's JoinEUI, DevEUI
and AppKey. Never log or commit real keys.

The Class A firmware also requires the old `valve_lora/nonces` NVS entry. With
placeholder credentials or missing nonce history, OTAA is blocked and the
ESP32 deep-sleeps before retrying.

### ChirpStack

For the Class A firmware, use `tools/chirpstack/pcv_low_power_class_a_codec.js`
and JSON such as `{"pcv":"open","command_id":2001}` after validating the
installed valve. The target sends the shared 32-byte protocol-v2 FPort-31 status.

## Node-RED dashboard

`include/pressure_node2_dashboard_flow.json` is the Dashboard 2.0 flow for this
node. It subscribes to `.../event/+`, decodes the FPort 31 status object, and
shows the last uplink, the last downlink and the node state next to separate
pressure and battery-voltage history charts. Each chart has its own vertical
time-zoom buttons (30 minutes to 24 hours). The flow has no flow-meter display
because this device has no flow meter. Commands go to `.../command/down` with the codec's `object`
input, the same path as the manual JSON above.

The dashboard export contains the replacement chart group created in Node-RED
and confirmed by the user to appear on the right of the Valve 2 page. The
status/control group uses eight columns and the chart group uses four; on
narrower screens Dashboard 2.0 may wrap them into separate rows.

Sleep intervals are set in **seconds**, over the same 10..86400 s range as
`lorawan::MIN_REPORT_INTERVAL_SECONDS` and
`lorawan::MAX_REPORT_INTERVAL_SECONDS` in `include/PressureNode2Config.h`.
`{"sleep_seconds":12}` sets the interval alone; `{"pcv":"open","sleep_seconds":60}`
changes the valve and the interval in one command. The dashboard allocates a
fresh `command_id` for every command, because the node ignores an id it has
already accepted.

valve_2 is Class A, so a command waits in ChirpStack until the node's next uplink
and is applied in the receive window that follows it.
