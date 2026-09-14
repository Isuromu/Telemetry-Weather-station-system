# WaterLevel end node

`WaterLevel` is the imported reservoir-level project with a telemetry-only
LoRaWAN Class A transport added around its existing measurement and load-control
logic.

Each wake cycle:

1. reads the GPIO35 battery divider;
2. reads the RD-RWG-01 pressure transmitter on UART2 GPIO16/GPIO17;
3. calculates depth and percentage for the existing 5 m range;
4. applies the existing GPIO27 load rule (battery at least 11.5 V and level at
   least 15 percent);
5. sends one FPort 40 uplink and completes RX1/RX2;
6. enters timer deep sleep while holding the selected GPIO27 output state.

Application downlinks are deliberately ignored. This first network integration
does not add remote load control or modify the existing threshold policy.

## Build

```text
pio run -e water_level
```

The build currently uses a provisional 900-second sleep interval. Change the
`WATER_LEVEL_SLEEP_SECONDS` build flag in the repository `platformio.ini` after
the deployment interval is selected. Values from 60 through 86400 seconds are
accepted.

## OTAA credentials

Copy `config/WaterLevelLoRaSecrets.example.h` to
`config/WaterLevelLoRaSecrets.h`, set `CONFIGURED` to `true`, and enter the
WaterLevel node's own JoinEUI, DevEUI, and AppKey. The destination file is
Git-ignored. A build without credentials remains valid: it measures, applies
the load rule, reports the missing credentials over Serial, and sleeps without
starting OTAA.

The radio pinout and EU868 configuration match `PressureControlNode`:

- NSS GPIO5, DIO1 GPIO26, RESET GPIO14, BUSY GPIO25;
- SPI SCK GPIO18, MISO GPIO19, MOSI GPIO23;
- TX enable GPIO32 and RX enable GPIO33.

Use `tools/chirpstack/water_level_class_a_codec.js` as the ChirpStack codec.

## Uplink payload

FPort 40 carries a 10-byte big-endian payload:

| Offset | Size | Meaning |
| --- | ---: | --- |
| 0 | 1 | Protocol version (`1`) |
| 1 | 1 | Flags: bit 0 pressure valid, bit 1 load on |
| 2 | 2 | Battery voltage in millivolts |
| 4 | 2 | Signed pressure in millibar |
| 6 | 2 | Depth in millimetres |
| 8 | 2 | Water level in 0.1 percent |

The imported standalone `platformio.ini` is retained as historical source
material. Builds integrated with this repository use the root `water_level`
environment.
