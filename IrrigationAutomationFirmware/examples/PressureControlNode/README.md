# PCV end-node firmware variants

This shared application source builds three separately named binaries. The
hardware drivers, PCV pulse sequence, measurements and binary LoRaWAN
protocol are common; only the runtime policy changes.

## `pcv_serial_only`

```text
pio run -e pcv_serial_only
```

The radio is not initialized. Serial Monitor at 115200 baud accepts `help`,
`status`, `battery`, `pressure`, `flow`, `flow probe`, `flow total`,
`flow total reset` (or `flow reset`), `pcv open`, `pcv close`, and `pcv state`
continuously.

## `pcv_hybrid_class_c`

```text
pio run -e pcv_hybrid_class_c
```

The ESP32 stays awake, Serial commands remain active, and the SX1262 stays in
LoRaWAN Class C continuous receive. Gateway PCV commands can therefore be
delivered without waiting for the next periodic uplink. The configured
`report_interval` controls telemetry only. Use
`tools/chirpstack/pcv_hybrid_class_c_codec.js` and enable Class C in the
ChirpStack device profile.

## `pcv_low_power_class_a`

```text
pio run -e pcv_low_power_class_a
```

The node wakes, reads status, sends on FPort 31, accepts one queued FPort 30
command in RX1/RX2, executes it, sends an immediate acknowledgement status, and
enters ESP32 timer deep sleep. Serial produces diagnostics only and accepts no
commands. Use `tools/chirpstack/pcv_low_power_class_a_codec.js`; `sleep_time`
controls real deep-sleep duration.

The current valve_1 commissioning build defaults to 10-second sleep and
accepts `sleep_seconds` down to 10 through its matching codec. This short
interval is for bench testing only; restore the Class A PlatformIO flags and
codec lower bound to at least 60 seconds before deployment.

### Credentials

The active Class A firmware reads the git-ignored
`config/PressureNodeLoRaSecrets.h`. A fresh clone uses the all-zero
`include/PressureNodeLoRaSecrets.example.h` fallback; copy its structure 
to the ignored config header and enter only valve_1's JoinEUI, DevEUI
and AppKey. Never log or commit real keys.

Both LoRaWAN targets require a node-specific ignored
`config/PressureNodeLoRaSecrets.h`. Never copy credentials from another end
node. See `docs/PRESSURE_NODE_LORAWAN.md`.

The TUF-2000M M46 slave address is confirmed as 1, and the commissioned
device's REAL4 layout is confirmed as `LOW_WORD_FIRST`. Normal `flow` and
LoRaWAN telemetry use that decoder. `flow probe` remains available to print
the raw response and both interpretations for diagnostics.

REG0113-REG0118 provide net, positive, and negative accumulated cubic metres.
`flow total reset` stores the current positive total as a persistent ESP32
baseline and reports later water use relative to it; it never erases the
meter's own accumulators. Both ChirpStack codecs use protocol v2 and expose the
local total in litres/m3. See `docs/FLOW_TOTALIZER.md`.

Valve_1 also reads the EPEVER LS1024B once at each boot/wake and prints its
battery voltage, LOAD current, battery status, and battery temperature. The
controller and TUF-2000M share UART2 GPIO16/GPIO17 through the existing
automatic-direction RS-485 bus; firmware switches baud rates and restores the
TUF-2000M 9600 baud setting after the EPEVER read. EPEVER access is read-only:
firmware does not write or alter its battery profile.

## Node-RED Dashboard 2.0

Import `include/pressure_node_dashboard_flow.json` for valve_1. It follows
the working valve_2 dashboard layout: status and commands on the left, with a
new chart group on the right. Pressure (upstream and downstream), battery
voltage, and measured TUF-2000M flow rate each have a history chart with
independent vertical time-zoom buttons. Water velocity and delivered volume
since the local baseline remain in the status panel, along with a flow-total
reset command. The reset changes only the ESP32 baseline; it does not clear
the meter's accumulated registers. The dashboard uses the Class A FPort 30/31
codec and queues downlinks until the next uplink receive window.

Before deploying the imported flow, replace `SET_VALVE1_APP_ID` and
`SET_VALVE1_DEV_EUI` in the MQTT-in topic and both Function nodes with this
device's ChirpStack application ID and DevEUI. Use the valve_1 device profile
with `pcv_low_power_class_a_codec.js` and check that the shared `localhost:1883`
MQTT broker and Dashboard 2.0 base match your Node-RED installation. Do not
use the valve_2 topic or its credentials for valve_1. The sleep-interval input
uses the current 10-86400 second commissioning range; increase the minimum
before field deployment together with firmware and codec.
