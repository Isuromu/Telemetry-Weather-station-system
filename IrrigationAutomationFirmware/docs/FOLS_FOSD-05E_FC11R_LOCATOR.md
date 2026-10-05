# FOLS FOSD-05E / FC11R locator

This note keeps the supplied FOLS quotation separate from the supplied locator
instructions. The quotation identifies an offered product; it is not evidence
that every offered mechanical, electrical, or valve-body option is installed.

## Preserved sources

| Source | Original supplied filename | SHA-256 |
|---|---|---|
| [FOLS FLD971XS-10 DN50 UPVC butterfly-valve quotation](references/FOLS_FOSD-05E/FOLS_FLD971XS-10_DN50_UPVC_butterfly_valve_quotation.pdf) | `FOLS_quotation_Modulating_type_Electric_plastic_UPVC_butterfly_valve.pdf` | `3D16CB03FCC3F6BE20D67EF747EF254C6B31724E29B46EFE17FBD4C4211503B7` |
| [FC11R locator RS485 and infrared-remote instructions V1](references/FOLS_FOSD-05E/FOLS_FC11R_locator_RS485_and_infrared_remote_control_instructions_v1.pdf) | `LOCATOR_INFLARED_REMOTE_CONTROL_AND_RS485_additional_instructionsV1 (1).pdf` | `F8C851557AD81F6AF7B450E0258E42922659659D6A177BB02426481409E9667E` |

## Manual facts

- The quotation identifies one `FLD971XS-10` DN50 UPVC butterfly valve with a
  `FOSD-05E` actuator, `220VAC`, `4-20mA`, `RS485`, and modulating control
  (quotation PDF p. 1). Its actuator table lists FOSD-05E as 50 Nm, 20 s rated
  travel, 0-90 degrees, and 0.25 A working current (quotation PDF p. 4).
- The locator instructions define Modbus RTU at 9600 baud and give a default
  format of 8 data bits, one stop bit, and no parity. They recommend at least
  30 ms between locator instructions (locator instructions PDF p. 3).
- Register `0x0000` selects operating mode: a function-06 write of `0x0001`
  selects RS485 bus control, while `0x0000` selects analog control. Registers
  `0x0001`, `0x0002`, `0x0003`, and `0x0004` respectively provide actual
  position, target position, fault code, and reset/calibration state (locator
  instructions PDF p. 3).
- Position and target values use the `value - 1999` scale with one decimal
  place. The documented target range is `0x07CF` through `0x0BB7` (1999-2999),
  representing 0.0-100.0 percent (locator instructions PDF pp. 3-4).
- The documented four-register status query is Modbus function 03 starting at
  `0x0000`, count four: `01 03 00 00 00 04 44 09` for slave address 1 (locator
  instructions PDF p. 6).
- A calibration write is function 06 to register `0x0004` with `0x000A`.
  Reading `0x0004` returns `0x000A` while calibration is active and `0x0000`
  when it completes. The stated calibration duration is 30-120 seconds
  (locator instructions PDF p. 5).
- Locator menu `U14` sets a unique address from 0 through 25. `U22` defaults
  to 10 seconds and reports E1 when no communication instruction is received;
  it can be set from 0 through 240 seconds (locator instructions PDF p. 2).
  The manual says `U0 = x.2` starts in RS485 communication state, while
  `U0 = x.0` or `x.1` starts under analog or switching-signal control (locator
  instructions PDF p. 3).

## Project implementation and configuration

- `examples/MainValve/src/main.cpp` uses slave address 1, 9600 baud, Modbus
  functions 03 and 06, registers `0x0000` through `0x0004`, the 1999-2999
  position scale, and at least a 30 ms Modbus inter-transaction gap.
- MainValve writes bus mode before remote position commands and reads the
  documented four-register status block. These are project implementation
  choices, not commissioning proof.
- If full status is unavailable after bus-mode setup, MainValve reads the
  one-register actual position and writes that same valid raw value as the
  target. This supplies the documented new set value without requesting valve
  travel, then retries the four-register status read.
- During calibration, MainValve polls register `0x0004` every two seconds. It
  blocks target commands while the register reports `0x000A` and releases them
  only after `0x0000`; 120 seconds is a diagnostic threshold, not an automatic
  release time.
- After a pressure-I2C transport failure, MainValve reinitializes I2C and
  retries the complete pressure sample once. Reinitialization is limited to
  once per five seconds so a disconnected sensor cannot cause a reset loop.

## Hardware-confirmed observations

- On 2026-10-02, the user observed a successful bus-mode write followed by
  successful one-register mode probes while the four-register status block was
  unavailable. A target-register write restored full status immediately.
- On 2026-10-05, after a cold start, writing the valid observed actual position
  as the target restored full status before LoRaWAN setup. The serial log
  reported `Actuator full status recovered after safe target write.`
- The installed locator answers Modbus slave address 1; no local-menu check of
  U14 is needed for the current one-actuator bus.
- A calibration command was acknowledged, briefly interrupted full-status
  reads, and later returned a valid status. These observations do not establish
  the values of U0 or U22 in the installed locator.
- On 2026-10-05, first observed 22 kW pump start about 50 cm from MainValve
  interrupted pressure-sensor I2C reads with ESP32 error
  `ESP_ERR_INVALID_STATE [259]`. The preceding actuator status read completed;
  this observation does not establish the coupling path.
- Later on 2026-10-05, the same I2C error occurred with the pump off. The pump
  may worsen the fault, but is not its proven root cause.

## Unresolved items

- The installed locator's U0 startup mode and U22 communication-time setting
  cannot currently be inspected because its panel is inaccessible.
- Confirm whether the installed valve exactly matches the quotation's DN50
  UPVC, 220 VAC, and 4-20 mA options; the quotation alone is not commissioning
  evidence.
- Before deployment, diagnose intermittent MainValve I2C failures and shield
  low-voltage electronics and pressure-I2C wiring from 22 kW pump/VFD noise.
  Keep I2C inside a short local enclosure run and repeat pump-start testing
  with final cable routing, enclosure, grounding, and power arrangement.
- Repeat cold-start and calibration-completion tests before deployment.
