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

## Node-RED Dashboard 2.0

Import `include/main_valve_dashboard2_updated_flow.json` using Node-RED's **Import**
menu. It is based on the supplied live flow and uses that flow's ChirpStack
application, device EUI, MQTT broker, and Dashboard page. Remove the old
MainValve flow nodes before importing it, so two copies do not send commands.
It uses the existing `/dashboard/page1` page in the supplied flow. Check that
the imported MQTT broker points to the broker
used by ChirpStack. Do not put the AppKey in Node-RED. Deploy, then wait for
an FPort 31 status uplink before sending the first command.

The dashboard subscribes to ChirpStack MQTT events, filters by application and
DevEUI, and displays FPort 31 status. It uses the ChirpStack decoded `object`
when available and decodes the base64 payload itself otherwise.
Controls provide 0-degree close, 45-degree half, 90-degree open, custom angle,
and percent travel. The first command becomes active; later commands wait in
an ordered Node-RED queue (maximum 20). The dashboard shows the active command
separately and gives each waiting item an X button. Removing a waiting item
does not cancel a command already sent to ChirpStack. A command ID is assigned
when an item becomes active, and the six-byte FPort 30 downlink is published.
Only a matching FPort 31 v2 `finished` report automatically dispatches the
next waiting item. `accepted` and `moving` do not advance the queue.

A `rejected` report pauses the waiting queue until an operator resumes it.
A `failed` report or five minutes without a final report also pauses the queue
and displays an inspection/unlock button. After inspecting the physical valve
and ChirpStack queue, unlock the active command, then explicitly resume the
waiting queue. A late `finished` report after the five-minute timeout leaves
the queue paused. The firmware also rejects new remote commands while it is
tracking an active movement. The waiting queue is held in Node-RED flow
context; its survival across a Node-RED restart depends on the configured
context store. In Class A
fallback, a queued downlink can wait until the next uplink; a dashboard
timeout does not prove that the command will never execute.

The uplink is protocol v2, 18 bytes on FPort 31. Bytes 0-14 retain the
previous layout except for version=2. Bytes 15-16 carry the reported command
ID, including a rejected attempted ID; byte 17 is command phase: 0 none,
1 accepted, 2 moving, 3 finished, 4 rejected, 5 failed. The ChirpStack codec
also decodes old v1 uplinks for display, but v1 cannot unlock a command.
The six-byte FPort 30 downlink remains protocol v1.

Movement is inferred from the actuator's Modbus actual-position register.
The firmware reports `moving` after a 0.5-degree change from the accepted
position and `finished` after two consecutive 2-second polls within 1 degree
of target with no actuator fault. A 180-second movement timeout is a
commissioning value to validate against measured full-travel time. Status
events are sent at least 5 seconds apart; radio delivery is not guaranteed.

`bus`, `clear`, `calibrate`, `reset`, and `status` are local Serial commands in
the current firmware. The LoRaWAN protocol does not expose them, so this
dashboard does not send invented remote command bytes.

## Payloads and codec

Install `include/main_valve_class_c_codec.js` from this example directory in the
ChirpStack v4 device profile. FPort 30 accepts a six-byte protocol-v1 target
command and FPort 31 carries an 18-byte protocol-v2 status. Queue a command such as:

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
