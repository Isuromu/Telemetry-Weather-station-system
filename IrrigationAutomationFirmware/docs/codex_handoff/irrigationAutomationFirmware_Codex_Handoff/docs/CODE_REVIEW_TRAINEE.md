# Review of Trainee Firmware

## Good decisions

- Pin constants were grouped in a namespace.
- Two `TwoWire` instances were used for two identical-address sensors.
- I2C presence detection was implemented.
- Register reads used a repeated START.
- Byte count was checked.
- 24-bit sign extension was implemented correctly.
- Battery ADC used averaging.
- `analogReadMilliVolts()` was used rather than manually assuming an ADC reference.
- The power gate was enabled before driving the L298N.

## Problems to fix

### 1. Generic `valve` naming

The project has multiple valve-related devices.

Rename the current one to `PressureControlValve`.

### 2. `closeValve()` is wrong for the installed latching solenoid

Old behavior:
- IN1 LOW
- IN2 LOW
- L298 power OFF

That merely removes power.

Correct latching behavior:
- power bridge;
- apply reverse polarity for a pulse;
- idle bridge;
- power bridge off.

### 3. State is falsely initialized as CLOSED

Old:
```cpp
bool valveIsOpen = false;
```

After reset this does not describe physical reality.

Use:
- Unknown
- Open
- Closed

and distinguish commanded state from verified physical state.

### 4. Hardware logic, application logic and Serial UI are mixed

Move:
- battery code -> `BatteryMonitor`;
- XDB401 code -> `PressureSensorXDB401`;
- latching control -> `PressureControlValve`;
- Serial parsing -> example/application layer.

### 5. Automatic status every two seconds

Useful for bench debugging, but inappropriate as the default architecture of a solar/battery node.

Keep it only as an explicit diagnostic/watch mode if desired.

### 6. XDB401 scaling is not yet proven

Do not treat the formula as production-calibrated until the exact datasheet is verified.

### 7. Pasted syntax errors

The Telegram/chat copy lost `||` in at least these conditions:

```cpp
input == "help" || input == "?"
```

and:

```cpp
character == '\r' || character == '\n'
```

Check the actual trainee source before assuming the repository has the same syntax error.

## Recommended command set for Serial example

```text
help
status
battery
pressure
pcv open
pcv close
pcv state
```
