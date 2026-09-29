# RD-RWG-01 water-level / pressure sensor

## Source and scope

Source: HONDE Technology, [RD-RWG-01 user manual](references/RD-RWG-01/RD-RWG-01_water_pressure_user_manual.pdf),
English, 9 PDF pages; revision not identified. Page references below are
1-based PDF pages. The supplied [usage illustration](references/RD-RWG-01/usage.jpg)
shows a submerged level-probe application; its marketing claims do not establish
the installed unit's range, construction or ingress-protection rating.

## Manual facts

- Section 4, p. 2: RS485 output; supply **12-36 VDC**, typical 24 V;
  stated accuracy 0.2%; medium temperature -20 to 75 degrees C and ambient
  temperature -30 to 80 degrees C.
- The range line on p. 2 says `0~10meters (-0.1~0~60Mpa)`: the level range is
  **0-10 m**, with the parenthetical being the pressure family's range, not a
  second level span. Read as the manual's stated level range. The purchased
  unit's own span is normally on its nameplate and in `0x0005`/`0x0006`, so
  confirm it there rather than from this line alone.
- Section 5, p. 3: the drawing shows **blue = supply positive, black = supply
  negative, red = RS485 A, white = RS485 B**. Verify the actual cable against
  its supplier markings before using these colors as a wiring instruction.
- Section 6, pp. 3-5: half-duplex Modbus RTU, 8 data bits, no parity, 1 stop
  bit; baud rates 1200 through 115200 are listed. The examples use address 1
  and 9600 baud; they are not a readback of this project's sensor.
- Values are signed 16-bit integers, with register data high byte first;
  the CRC uses polynomial `0xA001` and is transmitted low byte first.

### Read registers and scaling

Source: section 6, p. 5. These are **wire addresses**: request `0x0004`
for the measurement; do not subtract one as the TUF-2000M driver does.

| Address | Meaning | Interpretation |
|---|---|---|
| `0x0000` | Slave address | Manual lists 1-255; confirm the commissioned address |
| `0x0001` | Baud selection | 0..7 = 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200 |
| `0x0002` | Primary variable unit | 0 MPa; 1 kPa; 2 Pa; 3 bar; 4 mbar; 5 kg/cm2; 6 psi; 7 mH2O; 8 mmH2O |
| `0x0003` | Decimal places | 0..3 |
| `0x0004` | Measurement | Signed raw value divided by `10^decimal_places`, in the reported unit |
| `0x0005`, `0x0006` | Range zero / full point | Signed raw range values; interpret with unit and decimals |
| `0x000C` | Zero offset | Calibration offset; do not change during routine reads |

Read function is `0x03`. The example on pp. 3-4 reads raw `0x000A` and
three decimals, producing 0.010; the manual describes this example in metres.
Actual units still need to be read from `0x0002`.

### Configuration limits and source inconsistency

Pages 5-9 permit user changes to address, baud rate and zero offset using
function `0x06`; ordinary users must not change factory calibration data.
Page 6 defines `0x000F` value 0 as save to the user area and `0x0010` value 1
as restore factory parameters. These are not part of normal measurement.

The save example is inconsistent: p. 4 ends `02 06 00 0F 00 00` with
`B9 FA`, while p. 7 prints `B9 C9`. Recalculating CRC-16/MODBUS over the
six request bytes gives **B9 FA**. This is a calculated correction, not a
hardware-tested write. Generate CRCs rather than copying that example.

## Project implementation and configuration

The [WaterLevel configuration](../examples/WaterLevel/src/WaterLevelConfig.h)
and [implementation](../examples/WaterLevel/src/main.cpp) currently use:

- UART2 RX GPIO16 / TX GPIO17, 9600 8N1, unit 1, automatic RS485 direction;
- separate function-03 reads of unit, decimals and measurement;
- signed decoding and conversion to depth using water density 1000 kg/m3
  and gravity 9.81 m/s2, with **5 m** used as the project's percentage range.
  This disagrees with the manual's 0-10 m level range (see above), so the
  percentage is reported against an unconfirmed span and reads **half** the
  true fill if the installed unit is 10 m. `RANGE_METERS` in
  `WaterLevelConfig.h` is the single place to change it;
- fallback unit 7 (mH2O) and three decimals if metadata reads fail or are
  invalid. A successful value read can therefore produce telemetry based on
  assumed metadata; it is not proof that the fallback matches the sensor.

These are source-code settings, not hardware-confirmed sensor properties.
The application does not write sensor configuration or calibration registers.

## Hardware-confirmed observations

No sensor nameplate, raw register capture, supply measurement or calibrated
depth comparison was identified in the reviewed project records. The supplied
manual and usage illustration alone do not establish commissioning results.

## Unresolved items

- **Before production, settle the percentage range for the installed sensor and
  the existing pool.** Two questions, and `RANGE_METERS` is the only place they
  are answered: (a) the installed unit's span — the manual states 0-10 m, so
  the code's 5 m is unconfirmed and halves `levelPercent` if the unit is 10 m;
  and (b) what **100 %** should mean for this pool — the sensor's full span, or
  the pool's filled depth at the probe. A pool shallower than the sensor's
  range would never reach 100 % under the span reading, so pick deliberately
  rather than inheriting the 5 m default. Only `levelPercent` is affected:
  `depthMeters` and `pressureBar` are measured values and neither
  `updateLoadControl()` nor anything physical depends on the percentage (the
  level argument at `main.cpp:222` is commented out).
- Confirm installed model/range, pressure reference type and cable colors.
- Capture unit, decimals, range endpoints, address and baud from the real device;
  compare readings at known depths, including the zero reference.
- Verify voltage at the sensor under load: the manual's minimum is 12 V,
  while the WaterLevel load policy contains an 11.5 V battery threshold.
  That threshold does not guarantee a valid sensor supply.
- Measure current and stabilization time before defining power cycling.
- Decide whether metadata-read failure should invalidate a measurement instead
  of using the current fallback. This documentation update changes no firmware.
- Reconcile the existing application documentation with current source: the
  WaterLevel README/context describe 900-second sleep and a 15-percent level
  gate, while the header defaults to 10 seconds (the root 900-second override
  is commented out) and `updateLoadControl()` currently ignores level. These
  are code/document differences, not hardware-confirmed operating requirements.
