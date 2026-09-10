# LoRa and Gateway Context

## Radio hardware

Current pressure-control-node pinout is fixed:

- NSS GPIO5
- MOSI GPIO23
- MISO GPIO19
- SCK GPIO18
- DIO1 GPIO26
- BUSY GPIO25
- NRST GPIO14
- RXEN GPIO33
- TXEN GPIO32
- VCC 3.3 V

Previously used radio hardware in this project:
- DX-LR30-900M22S
- SX1262 family

## Related gateway context

The larger project has used:

- Raspberry Pi / Compute Module 4-side gateway hardware;
- RAK LoRa concentrator hardware;
- ChirpStack;
- Mosquitto;
- PostgreSQL;
- Redis.

This strongly suggests a LoRaWAN-capable deployment path, but the exact transport used by `irrigationAutomationFirmware` must be verified in the repository.

## Important ambiguity

Earlier field-node behavior was described as:

```text
send data
wait with timeout for acknowledgement
receive new sample rate from gateway
go to deep sleep
```

That describes application behavior, but it does not by itself prove raw LoRa or LoRaWAN.

Codex must not assume that a raw SX1262 packet protocol can directly replace an existing ChirpStack/LoRaWAN deployment.

## Bench example

A raw RadioLib example is still useful for:

- confirming SPI pins;
- confirming DIO1/BUSY/reset;
- checking RXEN/TXEN RF-switch control;
- validating radio transmit/receive hardware;
- exercising PCV commands without the full network stack.

Name that example clearly as `RawLoRaControl` so it is not confused with final LoRaWAN firmware.

## Final transport

If the repository confirms LoRaWAN:
- use the established join/session/device identity mechanism already used in the project;
- carry OPEN/CLOSE/STATUS semantics as application payloads;
- return ACK/status telemetry through the same network.

If the repository confirms custom raw LoRa:
- define a versioned packet structure;
- include command ID / transaction ID;
- include result status;
- include battery and pressure telemetry;
- implement timeout and duplicate-command handling.
