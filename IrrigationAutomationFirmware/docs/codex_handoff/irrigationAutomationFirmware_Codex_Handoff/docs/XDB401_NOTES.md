# XDB401 Pressure Sensor Notes

## Current topology

Two pressure sensors:

- upstream of the PressureControlValve;
- downstream of the PressureControlValve.

Prototype Rev A places them on two independent ESP32 I2C controllers because identical addresses may collide.

Target Rev B should use an I2C multiplexer.

## Current trainee register assumptions

Candidate addresses:
- 0x7F
- 0x6D

Registers:
- pressure: 0x06
- temperature: 0x09
- measurement command/status: 0x30
- measurement command value: 0x0A

Current busy/ready test:
- mask `0x08`;
- ready when `(status & 0x08) == 0`.

Note:
`0x08` is bit 3 in zero-based bit numbering.

## Data format assumed by trainee code

Pressure:
- 3 bytes;
- big-endian;
- signed 24-bit;
- sign-extended to int32.

Temperature:
- 2 bytes;
- big-endian;
- signed 16-bit.

Current conversion assumptions:

```text
pressureBar = rawPressure / 8388608.0 * 10.0
temperatureC = rawTemperature / 256.0
```

Sensor full scale was assumed to be:
- 0–1 MPa
- 0–10 bar

## Assessment

The I2C mechanics are sensible:

- begin transmission;
- write register;
- repeated START using `endTransmission(false)`;
- request the exact byte count;
- validate received length.

However, the exact register map and conversion scaling remain the most important items to validate against the exact XDB401/S1204 sensor documentation.

Do not rewrite the working bus code before confirming the datasheet. The main risk is interpretation, not the repeated-start sequence.
