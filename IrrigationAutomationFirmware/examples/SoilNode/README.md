# SoilNode end node

`SoilNode` packages the supplied `soil_sensor_node.ino` as a PlatformIO
example with the same standalone directory structure as `WaterLevel`.

This target is for the current ESP32-WROOM development-board prototype. It
does not use the preliminary production ESP32-C6 pinout. Each Class A cycle:

1. enables the RS485 soil-sensor power switch on GPIO27 and waits 2 seconds;
2. reads three holding registers from Modbus address 3 on UART1 GPIO16/GPIO17;
3. turns sensor power off and reads the 1S battery through an ADS1115 on
   GPIO21/GPIO22;
4. sends an eight-byte FPort 10 uplink and completes RX1/RX2, accepting an
   optional sleep-interval command in those Class A receive windows;
5. sends an immediate FPort 11 application result when it receives an FPort 10
   command, then completes another RX1/RX2 cycle;
6. retains the RadioLib session in RTC memory and sleeps for the configured
   interval.

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
wiring, and the reporting interval remain prototype values to validate before
field deployment. The current 10-second default is for commissioning only.

The active wake interval is held in `sleepIntervalSeconds` in `main.cpp`. It is
initialized from `config::lorawan::DEFAULT_SLEEP_SECONDS` and passed to the
ESP32 timer wake-up configuration. A valid ChirpStack downlink persists the
new value in NVS. The FPort 10 command is opcode `0x01`, a two-byte big-endian
command ID (1-65534), then a four-byte big-endian interval in seconds;
accepted values are 10-86400 seconds. The earlier five-byte command without
an ID remains accepted for compatibility and uses acknowledgement ID `65535`.
Use `include/soil_node_class_a_codec.js` in the
ChirpStack device profile and queue, for example:

```json
{"sleep_seconds":600}
```

or:

```json
{"sleep_minutes":10}
```

The codec emits the seven-byte form when `command_id` is included. The
Dashboard flow supplies this ID automatically. The eight-byte FPort 11 result
is version `1`, two-byte command ID, one-byte status (`0` applied, `1` invalid,
`2` NVS storage failed), and four-byte active sleep interval. The node sends
the result after applying or rejecting the command. A failed result uplink
leaves delivery unconfirmed; check the Serial log and retry if necessary.
The interval command is idempotent, but this prototype does not deduplicate
command IDs across resets. Queue one command at a time: the extra Class A
receive window after the result uplink is not used for another application
command. An additional downlink in that window is logged and not processed.

## Node-RED Dashboard 2.0

Import `include/soil_node_dashboard_compact_flow.json` through Node-RED's **Import**
menu. Install Dashboard 2.0 (`@flowfuse/node-red-dashboard`) if its `ui-*`
nodes are unavailable. The flow creates a `/soil-compact/soil-node` page with
current readings, 24-hour history, network status, and sleep-interval control.
For an existing import, remove the old SoilNode flow before importing this
revision. Deploy and refresh the dashboard page. The 600-second input is only
an initial draft; changing it does not queue a command until **Queue interval**
is pressed.
Small +/− buttons to the right of the temperature, water-content, conductivity,
and battery charts change each chart's visible time range independently
(30 minutes to 24 hours, initially 6 hours). They do not change the node's
reporting interval or delete the chart's 24-hour history.
The **Narrow dashboard sidebar (220 px)** CSS template uses **CSS (All Pages)**
and sets the shared navigation menu to 220 px on desktop. If SoilNode shares
the Dashboard UI with Valve and Pump pages, import
`../PumpControl/include/dashboard2_shared_sidebar_width.json` into that Node-RED instance.
It targets the shared `My Dashboard` UI shown in the supplied flow. To change
the width, edit both `220px` declarations, then deploy and refresh.
Configure the imported MQTT broker too; `SOIL_APP_ID` and `SOIL_DEV_EUI` are
Node-RED environment variables.

Configure the imported **ChirpStack MQTT** broker for the Mosquitto host,
port, authentication, and TLS used by your ChirpStack installation. Set these
Node-RED environment variables before deploying:

| Variable | Value |
| --- | --- |
| `SOIL_APP_ID` | ChirpStack application UUID |
| `SOIL_DEV_EUI` | This SoilNode's 16-hex-digit DevEUI |

The MQTT input subscribes to `application/+/device/+/event/+`, then filters
both IDs in the flow. Give its broker credentials access only to the intended
application where possible. The flow does not contain OTAA credentials.

Set `include/soil_node_class_a_codec.js` as the device-profile codec in
ChirpStack. The flow accepts its decoded FPort 10 object and can decode the
eight raw bytes if ChirpStack supplies no object. Only FPort 10 uplinks update
measurements. Invalid soil readings clear temperature, VWC, and EC on the
dashboard; battery voltage remains visible.

The control queues an FPort 10 downlink through the codec using
`{"sleep_seconds":600,"command_id":1}`. Input must be an integer from 10 to
86400 seconds. This Class A node receives a queued downlink only after its
next uplink. The dashboard matches the FPort 11 application result by command
ID and shows the interval actually stored by the node. A `txack`, network ACK,
or later telemetry uplink alone is not proof that the interval was applied.

Readings show **stale** after 30 minutes without an uplink. This threshold is
only a display hint; adjust it in the dashboard template for the deployed
reporting interval. Stale does not prove that the node is offline. The current
prototype does not report GNSS, detailed soil-probe error reasons, or battery
state of charge, so the dashboard does not invent those values.
