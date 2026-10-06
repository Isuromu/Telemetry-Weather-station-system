# XDB401 Pressure Sensor Notes

## Current topology

Two pressure sensors:

- upstream of the PressureControlValve;
- downstream of the PressureControlValve.

Prototype Rev A places them on two independent ESP32 I2C controllers because identical addresses may collide.

Target Rev B should use an I2C multiplexer.

MainValve carries the same sensor and the same assumptions, and reads it through
the same `lib/PressureSensorXDB401` driver
(`examples/MainValve/src/main.cpp`): address 0x7F with the 0x6D alternate probe,
register 0x06 pressure and 0x09 temperature, control register 0x30 with start
command 0x0A and busy mask 0x08, signed 24-bit pressure plus signed 16-bit
temperature, `raw / 8388608 * full scale` and `raw / 256`, 0-1 MPa full scale.
It has no scale-validated flag — its `pressure_valid` status bit is a -0.2 to
10.5 bar plausibility gate, not a calibration claim, and its 0x10 overpressure
bit plus the closing interlock on `MAX_PRESSURE_BAR = 4.0` are the first things
a mis-scaled pressure would disturb. The register map and counts-to-bar questions
below therefore cover MainValve as well, and one register read settles all three
sensors.

MainValve's conversion poll budget and its I2C bus recovery are the two places
where it does not behave like the PCV nodes; both are in the next section.

## MainValve read timing and bus recovery

The driver is shared, but the timing and the fault response are not, and neither
is settable through the sensor's register map.

**Poll budget.** The driver waits for a conversion by polling the control register
`readyPollAttempts` times with `readyPollIntervalMs` between attempts, so the
window is their product. The PCV nodes accept 50 ms (10 attempts at 5 ms);
MainValve waits up to 200 ms, matching the window its pre-driver code used, and
so sets 100 attempts at 2 ms. Narrowing it to the PCV values is a one-line
`Xdb401Configuration` change, deliberately not taken: nothing measured on this
board justifies accepting a shorter window.

**Bus recovery.** `lib/PressureSensorXDB401` has no recovery path. On a failed
register transaction it returns `ReadError`, and on an unconverted sample
`Timeout`, and stops. MainValve wraps the read with `recoverPressureI2c()`
(`examples/MainValve/src/main.cpp`), which on a recoverable status calls
`Wire.end()`, clocks SDA free with up to nine SCL pulses and a STOP
(`clearPressureI2cBus`), restarts the bus with `beginPressureI2c()`, and
re-probes with `pressureSensor.begin()`. Recovery is rate-limited to one attempt
per `PRESSURE_I2C_RECOVERY_MIN_INTERVAL_MS` (5000 ms), and the failure is logged
once per change of status rather than once per sample. The PCV nodes have no
equivalent and report a bus-level failure only as a reading with no sample.

`NotFound` is recoverable too, and that is not optional. `begin()` clears its
cached address before probing (`PressureSensorXDB401.cpp:19`) and only restores
it on a successful probe, and `read()` returns `NotFound` without touching the
bus whenever the address is zero (`:75-78`). So a re-probe that fails — exactly
what pump EMI produces when it wedges the bus mid-recovery — erases the known
address and leaves the node reporting `NOT_FOUND` forever, recoverable only by
power cycle. Measured 2026-10-06: after a pump start the node logged
`i2c_master_transmit_receive failed: [259] ESP_ERR_INVALID_STATE`, then
`Pressure read unavailable: NOT_FOUND` indefinitely; stopping the pump did not
clear it and only an ESP32 restart restored the 0x7F probe. Treating `NotFound`
as recoverable lets the rate-limited path keep re-probing until the bus answers.

That fix removes the latch, not the EMI. The pump start itself is still what
breaks the bus, so the I2C wiring (`PRESSURE_SDA_PIN` 21 / `PRESSURE_SCL_PIN`
22), its pull-ups and its routing relative to the pump cabling remain the
underlying item.

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
