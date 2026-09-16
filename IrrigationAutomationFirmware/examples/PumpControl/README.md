# PumpControl example

This example is the first commissioning application for the configured
Grandfar 2CP50/160B pump and DELIXI CDI-E100 VFD.

Build it with:

```text
pio run -e pump_control_example
```

The example initializes Serial at 115200 baud and RS-485 at the CDI-E factory
setting of 9600 baud, 8N1, slave address 1. It checks communication and VFD
configuration, sends only a stop command during boot, and then waits for text
commands or LoRaWAN commands. It never starts the pump automatically.

Start with `help`, `vfd ping`, `vfd config check`, and `pump status`.

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

RadioLib error `-1116` means the node did not receive a JoinAccept. If the
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

Use a new command ID for each operation. ID 65535 is reserved. The last ID and
command are stored in NVS before issuing a VFD command. A repeated ID with
the same operation and argument is ignored; an older ID or reuse with a
different command is rejected. IDs can wrap from 65534 to 0. Uplink FPort 51
is a 17-byte status containing communication/configuration/running flags,
command result and ID, commanded/actual frequency, current, VFD fault,
output voltage, run state, and communication error. The codec exposes these
as named fields. A status uplink is sent after a remote command and every
60 seconds. Local Serial commands continue to work.

The build verifies firmware compilation; OTAA join, Class C reception,
RS485 behavior, and pump operation require validation on the assembled node.

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
profile for named uplink fields. The flow also decodes the raw 17-byte status
payload if ChirpStack does not include an `object`.

The dashboard shows pump state, frequency, current, fault, radio connection,
and last uplink. Commands use binary FPort 50 downlinks and one pending command
ID. The ID is synchronized from the first FPort 51 status after Node-RED starts;
wait for that status before using the controls. A matching status changes
`Queued` to the device-reported result. An unmatched command times out after
150 seconds as `No device confirmation`; it is not automatically retried.
Stop remains available during a pending command, but an earlier downlink may
still be waiting in ChirpStack. Network Stop is not a physical emergency stop.
Node-RED flow context is in memory unless persistent context storage is
configured in Node-RED. After a restart, wait for a fresh device uplink to
resynchronize command IDs.
