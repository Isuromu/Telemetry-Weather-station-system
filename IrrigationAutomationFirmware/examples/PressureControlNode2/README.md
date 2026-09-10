# PressureControlNode2 - standalone valve example

This is a second, independent valve end node, imported from the supplied
`klapan` project. It is deliberately **not** a variant of
`examples/PressureControlNode`: it is a self-contained single-file application
that talks to its own radio driver ([src/ValveLoRaWan.h](src/ValveLoRaWan.h))
and shares no code with `lib/PressureControlNode`.

Keep the two examples separate. `PressureControlNode` is the production
architecture with three runtime modes, the TUF-2000M flow meter and binary
LoRaWAN protocol v2. This example is a simpler bench/prototype node with a
text command protocol on FPort 10.

The drivers sit next to `main.cpp` in `src/` rather than in a per-example
`include/` directory, because PlatformIO's default `lib_ldf_mode = chain+` does
not follow `-I` build-flag paths when resolving library dependencies, so a
`Preferences.h` include reached only through such a directory is never found.

## Build and upload

```text
pio run -e pcv_low_power_class_a_without_flowmeter -t upload
pio device monitor
```

Serial Monitor runs at 115200 baud and accepts `open`, `close`, `status`, and
`help` continuously. Valve outputs are driven low at boot, and the Serial
console keeps working even when the OTAA join fails.

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

## LoRaWAN

EU868, OTAA 1.0.x, Class C. Uplink and the text downlink command port are both
FPort 10. Nonces and session are persisted in the `valve_lora` NVS namespace.

The feature is bench-grade: RadioLib `activateOTAA` and `sendReceive` block, so
Serial commands can stall during a join or an uplink, and the RS485 port is
initialised at 9600 8N1 without a sensor protocol behind it yet.

### Differences from the imported source

The `klapan` project pinned RadioLib 7.6.0. This example builds against the
7.7.1 release that the rest of the repository uses, so the two downlink
buffers in `ValveLoRaWan.h` use `RADIOLIB_LORAWAN_MAX_PAYLOAD_SIZE` (242)
instead of 7.6.0's `RADIOLIB_LORAWAN_MAX_DOWNLINK_SIZE` (250), which 7.7.1
removed. 242 is the application-payload maximum that `sendReceive` and
`getDownlinkClassC` actually write, and matches what `examples/PressureControlNode`
already uses for the same purpose.

### Credentials

`src/lorawan_keys.h` is git-ignored. It is present on a commissioned machine
and absent on a fresh clone, where [src/lorawan_keys_default.h](src/lorawan_keys_default.h)
supplies all-zero placeholders instead. To commission a board, copy the
default file to `lorawan_keys.h` and replace the JoinEUI, DevEUI, and AppKey
with the values for that node only. Never log or commit real keys.

With the placeholder keys the OTAA join fails and is retried every 60 seconds;
Serial control keeps working throughout.

### ChirpStack

Use `tools/chirpstack/pcv2_klapan_codec.js` as the device-profile payload
codec. Downlinks are JSON text commands, e.g. `{"command":"open"}`.

The 12-byte uplink payload is big-endian, version 1:

| Byte | Meaning |
|---|---|
| 0 | Version = 1 |
| 1 | bit0 valve commanded open, bit1 before-valid, bit2 after-valid |
| 2-3 | Battery, unsigned millivolts |
| 4-5 | Pressure before, signed millibar |
| 6-7 | Pressure after, signed millibar |
| 8-9 | Temperature before, signed 0.01 C |
| 10-11 | Temperature after, signed 0.01 C |

The validity bits distinguish a missing/errored sensor from a real zero
reading; the codec reports `null` for the former.
