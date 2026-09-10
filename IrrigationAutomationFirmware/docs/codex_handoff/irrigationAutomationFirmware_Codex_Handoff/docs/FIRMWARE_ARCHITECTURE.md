# Firmware Architecture

## Main rule

Transport must not own hardware behavior.

Serial, LoRa and future RS485/network commands should call common services.

## Proposed layers

```text
Application
  |
  +--> IrrigationController / command dispatcher
          |
          +--> PressureControlValve
          +--> PressureSensorXDB401 (upstream)
          +--> PressureSensorXDB401 (downstream)
          +--> BatteryMonitor
          |
          +--> transport adapters
                 +--> Serial
                 +--> LoRa
                 +--> future RS485/network
```

## Modules

### `BoardPins.hpp`

Single source of truth for physical GPIO assignments.

### `SystemConfig.hpp`

Central constants:

- ADC divider values;
- PCV power active level;
- PCV pulse timings;
- XDB401 candidate addresses;
- sensor full scale;
- LoRa parameters used for bench bring-up.

### `BatteryMonitor`

Responsibilities:

- ADC configuration;
- sample averaging;
- divider conversion;
- calibration factor.

Should not print to Serial internally.

### `PressureSensorXDB401`

Responsibilities:

- discover known addresses on the given I2C bus;
- trigger measurement;
- wait for measurement-ready condition;
- read pressure and temperature;
- return a typed result with validity/error information.

Should not know whether it is the upstream or downstream sensor. That role belongs to the application.

### `PressureControlValve`

Responsibilities:

- safe pin initialization;
- L298N power sequencing;
- polarity pulse generation;
- commanded-state tracking;
- no Serial/LoRa logic.

### Application service

Responsibilities:

- read both pressures;
- calculate pressure difference;
- decide whether verification is available;
- execute desired PCV action;
- collect status response.

## Serial example

Suggested commands:

```text
help
status
battery
pressure
pcv open
pcv close
pcv state
```

For the diagnostic example, periodic watch mode can be added if useful, but it should not be the default production behavior.

## LoRa example

Minimum command semantics:

- OPEN
- CLOSE
- STATUS

Do not create a second PCV implementation for LoRa.

For raw LoRa bench bring-up, a small binary or compact textual protocol is acceptable.

For deployed gateway operation, inspect whether the project is using LoRaWAN/ChirpStack and implement the transport accordingly.

## Future low-power behavior

The soil-node project already established a preferred pattern:

```text
wake
self-check
measure
transmit
wait for ACK / updated interval
sleep
```

The pressure-control node may later adopt a related event-driven/low-duty approach, but valve-control commands and VFD/RS485 integration may require different awake behavior.

Do not force a deep-sleep policy into the first PCV bench-control firmware.
