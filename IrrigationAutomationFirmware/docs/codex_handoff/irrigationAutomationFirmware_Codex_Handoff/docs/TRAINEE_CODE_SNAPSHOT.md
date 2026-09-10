# Trainee Code Snapshot — Functional Content

The trainee firmware was a single-file Arduino program containing:

- `namespace Pin`:
  - `VALVE_IN1 = 16`
  - `VALVE_IN2 = 17`
  - `L298_POWER = 27`
  - `BATTERY_ADC = 35`
  - I2C1 SDA21/SCL22
  - I2C2 SDA13/SCL4

- XDB401 configuration:
  - full scale 10 bar
  - address 0x7F / 0x6D
  - pressure register 0x06
  - temperature register 0x09
  - measurement register 0x30
  - measurement command 0x0A

- battery divider:
  - 100 kOhm high side
  - 22 kOhm low side
  - calibration 1.0

- two `TwoWire` objects:
  - `i2cBefore(0)`
  - `i2cAfter(1)`

- `SensorReading { valid, pressureBar, temperatureC }`

- I2C helpers:
  - `devicePresent`
  - `findXdb401`
  - `writeRegister`
  - `readRegister`

- sensor read:
  - start measurement;
  - poll measurement register;
  - ready when `(status & 0x08) == 0`;
  - read 3-byte pressure;
  - read 2-byte temperature;
  - signed 24-bit pressure sign extension;
  - pressure formula using `8388608.0`;
  - temperature formula using `/256.0`.

- battery read:
  - 32 samples;
  - `analogReadMilliVolts`;
  - divider reconstruction.

- old actuator behavior:
  - `openValve()` enabled GPIO27 and used IN1 HIGH / IN2 LOW continuously;
  - `closeValve()` set both inputs LOW and switched power off;
  - `bool valveIsOpen` was used as the state.

- Serial commands:
  - `open`
  - `close`
  - `status`
  - `help`

- automatic `printStatus()` every 2 seconds.

The exact code pasted through chat lost the `||` operator in the `help/?` and CR/LF conditions. The actual repository version should be inspected before treating that as a source-code defect.
