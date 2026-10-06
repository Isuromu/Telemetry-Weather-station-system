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

Sensor full scale:
- 0–1 MPa
- 0–10 bar

Confirmed 2026-10-06 by the supplied unit's printed label
(`docs/references/XDB401/XDB401_installed_sensor_label.jpg`, reading
`SUP: 3.3 VDC`, `OUT: I2C`, `RANGE: 0-1 Mpa`), so this full scale and
`ASSUMED_FULL_SCALE_BAR = 10.0F` hold and the installed unit is an I2C-output
sensor. The range does not confirm the divisor above it, which is what turns raw
counts into bar.

## Supplied 485 protocol document, reviewed and rejected

A "485 MODBUS TRANSMITTER COMMUNICATION PROTOCOL" PDF supplied on 2026-10-06
documents this transmitter family's RS485/Modbus output only. It contains no I2C
content (no I2C, SCL or SDA anywhere), names no model in its text or metadata,
and describes a holding-register map whose number 0x06 is a pressure *unit* code
where the firmware reads pressure. It was therefore removed from
`docs/references/XDB401/` instead of being filed as this unit's datasheet: it is
not evidence for the I2C address, framing, register map, data width or scaling,
and those stay unresolved as recorded above. Obtain the I2C protocol document for
this model, or read the unit's registers, to close them.

## Assessment

The I2C mechanics are sensible:

- begin transmission;
- write register;
- repeated START using `endTransmission(false)`;
- request the exact byte count;
- validate received length.

However, the exact register map and conversion scaling remain the most important
items to validate against the exact XDB401/S1204 sensor documentation: the
register map (0x06 / 0x09 / 0x30 / 0x0A, busy mask 0x08), the big-endian signed
24-bit pressure format and its `/ 8388608` divisor, and the `/ 256.0`
temperature scaling. None of those follows from the 0–1 MPa range, so closing
them needs the datasheet or a reference comparison — a calibrated gauge on the
same port, or a known static head — at more than one pressure.

The range confirmation does change one reported detail: `PressureNodeConfig.h`
and `PressureNode2Config.h` now set `ENGINEERING_SCALE_VALIDATED = true`, so a
pressure reading reports `ReadingStatus::Valid` instead of `ValidUncalibrated`.
That flag only sets uplink flags 0x10/0x20 and the status text; the conversion
above is unchanged, and no interlock or command path reads it.

Do not rewrite the working bus code before confirming the datasheet. The main risk is interpretation, not the repeated-start sequence.
