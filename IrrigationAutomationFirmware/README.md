# IrrigationAutomationFirmware

Modular PIOArduino firmware for a multi-node irrigation automation system.
The repository currently contains two distinct end-node families:

- a pump/VFD end node controlling a Grandfar 2CP50/160B through a DELIXI
  CDI-E100G2R2T4B over Modbus RTU;
- a Prototype Rev A PCV end node with a latching open/close solenoid, two I2C
  pressure sensors, battery-voltage monitoring, a TUF-2000M/TS-2 over RS-485,
  and separate Serial-only, LoRaWAN Class C, and LoRaWAN Class A targets.

## Current target

- ESP32 DevKitC V4 / ESP32-WROOM-32D
- PIOArduino 55.03.311, Arduino-ESP32 3.3.11
- Debug Serial: 115200 baud
- Pump/VFD RS-485: UART2, RX GPIO16, TX GPIO17
- PCV-node TUF RS-485: UART2 GPIO16/GPIO17, 9600 8N1, address 1,
  automatic direction
- PCV-node L298N: IN1 GPIO2, IN2 GPIO15, switched power GPIO27
- LoRa DX-LR30-900M22S: RadioLib LoRaWAN Class C/Class A integration
- ESP32-H2: architecture placeholder only; pinout pending

## Safety defaults

The firmware never starts the pump automatically. On boot it initializes the
bus, checks communication, issues a deceleration-stop command, validates the
VFD configuration, and waits for commands. A new frequency must be explicitly
set after every boot before `pump start` is accepted.

`pump estop` sends the CDI-E **free stop** command. It is a software command and
does not replace a hardwired emergency-stop circuit.

This system controls 380 V equipment. Read [SAFETY.md](docs/SAFETY.md) before
wiring, uploading, or commissioning.

## Build

Open this folder as a PlatformIO project in VS Code. The default environment is
`esp32_wroom32d`.

```text
pio run -e esp32_wroom32d
pio run -e esp32_wroom32d -t upload
pio device monitor -b 115200
```

The standalone example can also be built:

```text
pio run -e pump_control_example
```

The pressure-control end node has three deliberately distinct builds:

```text
pio run -e pcv_serial_only
pio run -e pcv_hybrid_class_c
pio run -e pcv_low_power_class_a
```

Protocol checks contain compile-time assertions for the manual's known CRC,
parameter-address encoding, and frequency scaling. They can be built without
uploading to hardware:

```text
pio test -e protocol_compile_tests --without-uploading --without-testing
pio test -e pressure_logic_compile_tests --without-uploading --without-testing
```

The pressure-node tests contain executable Unity cases and compile-time
assertions. The command above verifies them without requiring attached
hardware; executing the Unity cases requires an ESP32 test target or a host C++
toolchain.

## First commands

```text
help
vfd ping
vfd info
vfd config check
pump status
pump freq 10
pump start
pump telemetry
pump stop
```

Do not run the pump dry. Confirm that it is primed, valves are correctly set,
rotation is safe to test, and the motor current stays below the 3.3 A motor
nameplate rating.

For the PCV end node, start with:

```text
status
battery
pressure
flow
flow total
flow total reset
pcv open
pcv close
pcv state
```

`flow total reset` establishes a persistent local ESP32 baseline; it does not
erase the TUF-2000M internal accumulators. See
[FLOW_TOTALIZER.md](docs/FLOW_TOTALIZER.md).

## Libraries

- `PrintController`: controlled diagnostic output
- `Rs485ModBus`: generic Modbus RTU transport, CRC, retries, raw diagnostics,
  and automatic/manual direction modes
- `DelixiCDIE100`: CDI-E register map, commands, monitoring, fault decoding,
  and configuration validation/application
- `PumpController`: motor limits, start interlocks, no-reverse policy, and
  telemetry
- `CommandProcessor`: pump/VFD Serial command processor
- `PressureNodeCore`: transport- and hardware-independent pressure-node state
  and power-policy types
- `BatteryMonitor`, `PressureSensorXDB401`, `PressureControlValve`: reusable
  Rev A hardware drivers
- `FlowMeter`, `Tuf2000mFlowMeter`: transport-independent flow interface and
  the commissioned TUF-2000M Modbus RTU driver for rate, velocity, errors, and
  accumulated volume
- `PressureControlNode`: shared application API for Serial and LoRaWAN command
  sources, including the persistent local water-total baseline
- `PressureNodeLoRaProtocol`: versioned ChirpStack command/status payloads

## Documentation

- [Architecture](docs/ARCHITECTURE.md)
- [Board and peripheral pinout](docs/PINOUT.md)
- [DELIXI CDI-E100 protocol](docs/DELIXI_CDI_E100.md)
- [Grandfar pump profile](docs/GRANDFAR_2CP50_160B.md)
- [Serial commands](docs/SERIAL_COMMANDS.md)
- [Commissioning](docs/COMMISSIONING.md)
- [RS-485 hardware](docs/RS485_HARDWARE.md)
- [Pressure-control end node](docs/PRESSURE_CONTROL_NODE.md)
- [TUF-2000M/TS-2 reference](docs/TUF_2000M_TS2.md)
- [Accumulated water and reset semantics](docs/FLOW_TOTALIZER.md)
- [PCV LoRaWAN and ChirpStack](docs/PRESSURE_NODE_LORAWAN.md)
- [Preliminary solar soil-node PCB specification](docs/SOIL_NODE_PCB_PRELIMINARY_SPEC.md)
- [Soil-node PCB technical specification review draft](docs/SOIL_NODE_PCB_TECHNICAL_SPEC_DRAFT.md)
- [USB-RS485 diagnostic tool](tools/USB_RS485_Sensor_Tool_v0_9_2/README.md)
- [Essential Uzbek guide](docs/README_UZ.md)

## Remaining integration work

- Add ESP32-H2 GPIO definitions after its pinout is supplied.
- Validate the unresolved PCV pulse/polarity, GPIO27 active level, pressure
  sensor scaling, and complete-node power budget on the assembled hardware.
- Provision unique OTAA credentials and commission the selected LoRaWAN mode.
- After successful bring-up, configure a non-zero CDI-E communication timeout
  as a deployment fail-safe and validate the desired stop action on the real
  installation.
