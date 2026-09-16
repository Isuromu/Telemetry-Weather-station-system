# MainValve end node

`MainValve` packages the supplied FOSD-05E controller sketch as a standalone
PlatformIO example using the same directory layout as `SoilNode`. This is a
separate mains-powered main butterfly-valve node; it is not the repository's
latching pressure-control valve node.

The ESP32 communicates with the FOSD-05E actuator over an automatic-direction
RS485 converter, reads one 2302X pressure sensor over I2C, and uses an SX1262
LoRaWAN Class C session for remote target-angle commands. If Class C cannot be
activated, local Serial control remains available and Class A downlinks can
still arrive after status uplinks.

## Build

From the repository root:

```text
pio run -e main_valve
```

Or open `examples/MainValve` as a standalone PlatformIO project and build the
`upesy_wroom` environment.

## OTAA credentials

For a root build, copy `config/MainValveLoRaSecrets.example.h` to
`config/MainValveLoRaSecrets.h`. For a standalone build, copy the example
header from this project's `include` directory to
`include/MainValveLoRaSecrets.h`. Set `CONFIGURED` to `true` and enter this
node's JoinEUI, DevEUI, and AppKey. Both destination paths are Git-ignored.

Credentials present in the supplied sketch were deliberately not committed.
Because they were shared in source form, rotate the AppKey before deployment.

## Prototype pinout

| Function | GPIO |
| --- | ---: |
| RS485 RX / TX | 16 / 17 |
| Pressure I2C SDA / SCL | 21 / 22 |
| LoRa NSS / DIO1 / RESET / BUSY | 5 / 26 / 14 / 25 |
| LoRa SCK / MISO / MOSI | 18 / 19 / 23 |
| LoRa TXEN / RXEN | 32 / 33 |

## Payloads and codec

Install `include/main_valve_class_c_codec.js` from this example directory in the
ChirpStack v4 device profile. FPort 30 accepts a six-byte protocol-v1 target
command and FPort 31 carries a 15-byte status. Queue a command such as:

```json
{"angle_deg":45,"command_id":1}
```

Provide an integer `command_id` from 0 to 65534 and use a new ID for every new
command. Reusing an ID with the same angle is treated as a duplicate; reusing
it with a different angle is rejected. ID 65535 is reserved by the firmware.

## Safety and validation

The actuator uses mains voltage. Keep mains outside the ESP32/RS485 circuit
and use the actuator's certified isolated low-voltage supply interface.
Register meanings, the 1999-2999 position scaling, the pressure-sensor transfer
function, pressure interlock policy, and physical open/close direction must be
validated on the assembled hardware before field use.
