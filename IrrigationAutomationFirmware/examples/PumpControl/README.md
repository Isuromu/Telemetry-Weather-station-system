# PumpControl example

This example is the first commissioning application for the configured
Grandfar 2CP50/160B pump and DELIXI CDI-E100 VFD.

Build it with:

```text
pio run -e pump_control
```

The example initializes Serial at 115200 baud and RS-485 at the CDI-E factory
setting of 9600 baud, 8N1, slave address 1. It checks communication and VFD
configuration, sends only a stop command during boot, and then waits for text
commands or LoRaWAN commands. It never starts the pump automatically.

Start with `help`, `vfd ping`, `vfd config check`, and `pump status`.

## Pressure-control capability and planned direction

The supplied DELIXI CDI-E manual documents a built-in PID controller in
Section 7.1.15 (`Функции PID`, printed page 207 / PDF page 214) and a
constant-pressure water-supply example in Section 7.2.1 (`Контроль PID подачи
воды постоянного давления`, printed pages 215-218 / PDF pages 222-225). The
example uses VF1 for pressure feedback and requires `P0.0.04 = 8` to select PID
as the frequency source. Group P4.0 contains the PID setpoint, feedback,
direction, gain, integration, derivative, filtering, feedback-loss, and stop
parameters.

This capability is recorded for later bench testing; it is not enabled by the
current firmware. The commissioned profile deliberately expects `P0.0.04 = 9`
and commands frequency over Modbus. Running `vfd config apply CONFIRM` with the
current profile would therefore select communication frequency control rather
than the inverter's internal PID source.

The planned project direction is to implement constant-pressure logic using
the electronic pressure sensor installed upstream of MainValve, between the
pump and the valve. The supervisory controller will use that pressure value to
request bounded PumpNode frequency changes through the existing Modbus
frequency command. Exact pressure targets, alarm thresholds, hysteresis,
frequency ramp limits, stale-data behavior, and restart policy remain to be
approved and commissioned before this control is enabled.

The MainValve pressure signal currently reaches the integrated controller over
LoRaWAN/MQTT. That path can support slow pressure optimization, but it is not a
substitute for local high-high-pressure protection: network delay or loss must
not be allowed to defeat an independent pressure-relief device, hardwired trip,
or other protection required by the hydraulic installation. Until the control
policy and hardware protection are validated, PumpControl remains explicit
fixed-frequency control and must not start automatically.

## LoRaWAN setup

The pump uses its own SX1262/DX-LR30 OTAA identity and EU868 Class C session.
Copy `examples/PumpControl/include/PumpControlLoRaSecrets.example.h` to
`config/PumpControlLoRaSecrets.h`, set `CONFIGURED = true`, and enter this
device's JoinEUI, DevEUI, and AppKey. The destination is Git-ignored. A local
header has been provisioned for this build. Register the same device in
ChirpStack and install `include/pump_control_class_c_codec.js` in its device
profile. Confirm EU868 against the gateway's configured region before radio
use. If Class C activation is not acknowledged, the node still checks Class A
receive windows after its periodic uplinks.

The user confirmed the Pump radio pinout: NSS 5, DIO1 26, RESET 14, BUSY 25,
SCK 18, MISO 19, MOSI 23, TXEN 32, RXEN 33. It matches MainValve. RS485
remains RX 16 / TX 17.

RadioLib error `-1116` means the node did not receive a JoinAccept. The example
prints a readable meaning for common RadioLib errors, the join-attempt number,
and the scheduled retry delay. Retries start near 60 seconds and use randomized
exponential backoff capped at 15 minutes. If the
gateway's LoRaWAN Frames tab shows no JoinRequest, first confirm the Pump
radio wiring (especially TXEN and antenna), supply, and EU868 channel plan.
The JoinEUI and AppKey cannot explain a gateway receiving no RF frame. If the
gateway sees a JoinRequest but the device page does not, compare the DevEUI
shown in the frame with the registered device. If ChirpStack sends a
JoinAccept, investigate the downlink path and RadioLib RX-window behavior.


## Remote protocol

Downlink FPort 50 is six bytes: version `1`, operation, command ID (big-endian
uint16), argument (big-endian uint16). Operations are `1` stop, `2` software
free-stop, `3` set frequency in 0.01 Hz units, `4` start. Other operations
require argument zero. Valid frequency remains the motor profile's 10..50 Hz.
Start requires an explicit frequency command since boot and a valid VFD
configuration, and the pump driver checks fault and run state before starting.
The software free-stop is not a physical emergency-stop circuit.

Remote Start and Stop have two reports. The first reports `in_progress` after
the VFD accepts the command. Start reports final `accepted` only when the VFD
is in Forward state and measured output frequency is within 0.25 Hz of the
requested frequency. Stop reports final `accepted` only when the VFD reports
Stopped and measured output frequency is at most 0.25 Hz. If either condition
is not reached within 120 seconds, the final result is `failed`.

Use a new command ID for each operation. ID 65535 is reserved. The last ID and
command are stored in NVS before issuing a VFD command. A repeated ID with
the same operation and argument is ignored; an older ID or reuse with a
different command is rejected. IDs can wrap from 65534 to 0. Uplink FPort 51
uses a 22-byte protocol-v2 status containing communication/configuration/running flags,
command result and ID, commanded/actual frequency, current, VFD fault,
output voltage, run state, communication error, previous join error, join
attempt count, and previous retry delay. The codec also accepts the older
17-byte protocol-v1 status. A two-byte FPort 50 payload `01 05` requests status
without consuming a pump command ID or operating the VFD. Status is sent every
15 seconds while running, every 60 seconds while stopped, and promptly after a
command, refresh request, or important state change. Local Serial commands
continue to work.

The build verifies firmware compilation; OTAA join, Class C reception,
RS485 behavior, and pump operation require validation on the assembled node.

## Manual downlink commands

The Node-RED controls are the normal path. For a manual test, queue the same
commands as JSON in the ChirpStack device page, or through the codec's
`encodeDownlink`. The installed codec turns each of these into the FPort 50
bytes. Arm the frequency first, because start stays refused until a frequency
has been commanded since boot:

```json
{"command":"set_frequency","frequency_hz":35,"command_id":1000}
```

```json
{"command":"start","command_id":1001}
```

```json
{"command":"stop","command_id":1002}
```

```json
{"command":"estop","command_id":1003}
```

`command` is `stop`, `estop`, `set_frequency`, or `start`. `frequency_hz` is
used only by `set_frequency` and must be a number from 10 to 50; it is required
there and rejected elsewhere. `command_id` must be an integer from 0 to 65534,
and every new command needs a new ID, since a repeated ID is a duplicate and an
older ID is rejected. `stop` here is the same deceleration stop as the local
`pump stop`; `estop` is the software free-stop, which is not a physical
emergency-stop circuit.

The same commands can be published directly to the broker the dashboard uses,
on the ChirpStack v4 command topic:

```json
{"devEui":"<devEui>","confirmed":false,"fPort":50,"data":"AQMD6A2s"}
```

The topic is `application/<application-id>/device/<devEui>/command/down`, and
`data` is the six downlink bytes base64-encoded. The table below gives the
bytes and that encoding for the four commands above.

| JSON command | FPort 50 bytes | `data` |
| --- | --- | --- |
| `set_frequency` 35 Hz, ID 1000 | `01 03 03 e8 0d ac` | `AQMD6A2s` |
| `start`, ID 1001 | `01 04 03 e9 00 00` | `AQQD6QAA` |
| `stop`, ID 1002 | `01 01 03 ea 00 00` | `AQED6gAA` |
| `estop`, ID 1003 | `01 02 03 eb 00 00` | `AQID6wAA` |

A manual command that uses an ID the dashboard is about to use will make the
dashboard's queued command look like a duplicate; leave the controls idle while
testing by hand, and wait for a fresh FPort 51 status before using them again.
The result of a manual command is only visible in that status uplink, since the
dashboard tracks the commands it queued itself.

## Node-RED Dashboard 2.0

Import `include/pump_control_dashboard2_flow.json` into Node-RED. It replaces
the old Pump controls that sent text commands on FPort 10. Disable or remove
those old controls after importing, so operators do not have two command
panels. The new Pump group references the same Dashboard base, page, theme,
and MQTT broker IDs as the MainValve example. After import, check that Node-RED
has placed the Pump group on the intended page and has not made duplicate
configuration nodes. The included MQTT broker defaults to `localhost:1883`;
select the deployed ChirpStack broker if different.

The flow is configured for the Pump application ID and DevEUI from the supplied
dashboard. These values appear in the MQTT input topic and two Function nodes;
update all three if the ChirpStack registration changes. No AppKey is stored
in the flow. Install `pump_control_class_c_codec.js` in the ChirpStack device
profile for named uplink fields. The flow decodes the raw protocol-v2 payload
if ChirpStack does not include an `object`, while retaining protocol-v1 support.

The dashboard shows pump state, frequency, current, fault, radio connection,
and last uplink. A compact colored LoRaWAN state and icon-only refresh control
sit below the Pump subtitle. Refresh sends the non-actuating `01 05` request,
uses a loading spinner, and is protected by a 10-second UI and Node-RED rate
limit. Commands use binary FPort 50 downlinks and one pending command
ID. The ID is synchronized from the first FPort 51 status after Node-RED starts;
wait for that status before using the controls. A matching status changes
`Queued` to the device-reported result. An unmatched command times out after
150 seconds as `No device confirmation`; it is not automatically retried.
Stop remains available during a pending command, but an earlier downlink may
still be waiting in ChirpStack. Network Stop is not a physical emergency stop.
Node-RED flow context is in memory unless persistent context storage is
configured in Node-RED. After a restart, wait for a fresh device uplink to
resynchronize command IDs.

A node that has not joined cannot transmit its local join error or exact retry
deadline through LoRaWAN. While status is missing, the dashboard therefore
shows `Join not confirmed / node offline`, explains the automatic retry policy,
and explicitly marks the exact next attempt as unavailable. After a successful
join, the first status reports the attempt count, previous RadioLib error, and
the retry delay preceding that join. An exact live countdown during a failed
join would require an out-of-band channel such as USB Serial or gateway-level
JoinRequest monitoring.

Last uplink timestamps are rendered in Uzbekistan time instead of exposing the
raw ChirpStack timestamp.

The dashboard marks telemetry offline after 45 seconds when the last report
said the pump was running, or after 75 seconds when it said the pump was
stopped. Once offline, cached Class C, VFD communication, and VFD fault values
are shown as unavailable. Dashboard commands other than Stop are blocked until
a fresh status arrives; Stop remains available as a safety action.

## Deferred commissioning issue: USB disconnect when the motor starts

During the 2026-09-24 commissioning test, the VFD acknowledged an 18 Hz
frequency command and the forward-run command. Serial printed
`[PUMP] Start command accepted.`, immediately followed on the Windows host by:

```text
Serial read error: ClearCommError failed
(PermissionError(13, 'Access is denied.', None, 5))
```

The accepted message is printed only after the Modbus frequency and RUN writes
have succeeded. The subsequent error therefore indicates that Windows lost the
USB-UART connection when the VFD/motor energized; it is not a rejected pump
command or a `PumpController` state error. The current prototype's automatic-
direction RS-485 converter is not galvanically isolated, so VFD-generated EMI,
a ground-potential disturbance, or a controller/USB supply disturbance is the
leading explanation. Another process taking the COM port is less likely but
should still be excluded.

Before further laptop-connected motor testing:

- determine whether LoRaWAN telemetry continues after the USB failure; if it
  does, the ESP32 remained active and the failure is limited to USB/host access;
- check whether the COM port disappears/reappears and whether reconnecting shows
  an ESP32 boot banner; a boot banner or LoRaWAN restart indicates a controller
  reset or power disturbance;
- replace the prototype interface with a galvanically isolated RS-485
  transceiver with isolated power, and use a USB isolator for commissioning;
- verify protective earth and VFD/motor-cable shielding according to the VFD
  manual, using personnel qualified for the 380 V installation;
- keep USB and shielded twisted-pair RS-485 wiring separated from VFD input and
  U/V/W motor conductors, and verify that termination exists only at the two
  physical ends of the RS-485 trunk;
- if the ESP32 resets, inspect its regulated supply, grounding, local
  decoupling, and transient behavior during motor startup;
- confirm that only one serial-monitor application has the COM port open.

This item is intentionally deferred. A firmware change cannot preserve a
Windows COM handle when the USB-UART hardware or host controller disconnects.
Do not treat LoRaWAN control as a replacement for the required hardwired stop
and safe isolation arrangement.
