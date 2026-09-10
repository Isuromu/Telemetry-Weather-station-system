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
