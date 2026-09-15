# SoilNode end node

`SoilNode` packages the supplied `soil_sensor_node.ino` as a PlatformIO
example with the same standalone directory structure as `WaterLevel`.

This target is for the current ESP32-WROOM development-board prototype. It
does not use the preliminary production ESP32-C6 pinout. Each Class A cycle:

1. enables the RS485 soil-sensor power switch on GPIO27 and waits 2 seconds;
2. reads three holding registers from Modbus address 3 on UART1 GPIO16/GPIO17;
3. turns sensor power off and reads the 1S battery through an ADS1115 on
   GPIO21/GPIO22;
4. sends an eight-byte FPort 10 uplink and completes RX1/RX2;
5. retains the RadioLib session in RTC memory and sleeps for 600 seconds.

The automatic-direction RS485 converter requires no DE/RE GPIO.

## Build

From the repository root:

```text
pio run -e soil_node
```

Or open `examples/SoilNode` as a standalone PlatformIO project and build the
`upesy_wroom` environment.

## OTAA credentials

For a root build, copy `config/SoilNodeLoRaSecrets.example.h` to
`config/SoilNodeLoRaSecrets.h`. For a standalone build, copy the example
header from this project's `include` directory to
`include/SoilNodeLoRaSecrets.h`. Set `CONFIGURED` to `true` and enter this
node's JoinEUI, DevEUI, and AppKey. Both destination paths are Git-ignored.

The credentials contained in the supplied sketch were intentionally not added
to tracked files and should be rotated because they have been shared outside
the secrets store.

## Prototype pinout

| Function | GPIO |
| --- | ---: |
| Sensor power | 27 |
| RS485 RX / TX | 16 / 17 |
| ADS1115 SDA / SCL | 21 / 22 |
| LoRa NSS / DIO1 / RESET / BUSY | 5 / 26 / 14 / 25 |
| LoRa SCK / MISO / MOSI | 18 / 19 / 23 |
| LoRa TXEN / RXEN | 32 / 33 |

The supplied sketch named GPIO34 as the sensor-power output, but GPIO34 is
input-only on ESP32-WROOM. The current prototype uses output-capable GPIO27 for
the transistor power switch. It remains configurable in the root environment:

```ini
build_flags =
  ${env:esp32_wroom32d.build_flags}
  -D SOIL_NODE_SENSOR_POWER_PIN=27
```

The current prototype log reports about 3.514 V directly at ADS1115 A0, which
is already a plausible 1S battery voltage. The default battery divider ratio is
therefore `1.0`; override `SOIL_NODE_BATTERY_DIVIDER_RATIO` if the assembled
board is later confirmed to contain a divider.

## Uplink payload

FPort 10 carries eight big-endian bytes, matching the supplied sketch:

| Offset | Size | Meaning |
| --- | ---: | --- |
| 0 | 1 | Flags; bit 0 means the soil reading is valid |
| 1 | 2 | Signed temperature in 0.01 degrees C |
| 3 | 2 | Volumetric water content in 0.01 percent |
| 5 | 2 | Conductivity in 0.001 mS/cm |
| 7 | 1 | Battery voltage encoded as `(volts - 2.0) * 100` |

The firmware accepts and ignores a local eight-byte transmit echo, retries the
read three times, and requires the 11-byte `03 03 06 ...` CRC-valid sensor
response shown by the USB tool. EU868, sensor register scaling, the ADS1115
wiring, and the 600-second interval remain prototype values to validate before
field deployment.
