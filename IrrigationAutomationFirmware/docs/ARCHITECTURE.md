# Architecture

## Multi-node project boundary

The repository is a multi-node irrigation project. A build target selects one
end-node application; shared transports and hardware drivers remain reusable.
The pump/VFD end node below is not the pressure-control end node. The latter is
documented in [PRESSURE_CONTROL_NODE.md](PRESSURE_CONTROL_NODE.md).

## Control layers

```text
SerialCommandSource        Future LoRaCommandSource
          |                           |
          +------------+--------------+
                       |
               CommandProcessor
                       |
                PumpController
                       |
                DelixiCDIE100
                       |
                  Rs485ModBus
                       |
        HardwareSerial / RS-485 transceiver
                       |
              DELIXI CDI-E100 VFD
                       |
             Grandfar 2CP50/160B
```

`Rs485ModBus` contains no DELIXI or pump knowledge. `DelixiCDIE100` contains the
manual-derived VFD protocol but no hardcoded pump limits. `PumpController`
receives a motor profile and applies its allowed frequency range and
no-reverse policy.

`CommandProcessor` has no dependency on Serial input. `SerialCommandSource`
only assembles lines and passes them to the processor. A future LoRa source can
submit the same command strings without changing the pump or VFD layers.

## Configuration selection

The initial milestone uses compile-time selection:

```text
ACTIVE_BOARD
ACTIVE_INVERTER
ACTIVE_MOTOR
```

Profiles remain separate because one VFD may be used with different motors.
Runtime selection and NVS storage can be added later without changing the
driver interfaces.

## Boot state

1. Initialize debug Serial.
2. Initialize UART2 and the auto-direction RS-485 transport.
3. Initialize the CDI-E driver and pump controller.
4. Test VFD communication.
5. Send a deceleration-stop command only.
6. Read and compare the expected configuration without changing it.
7. Remain stopped and process commands.

There is no boot path that sends a run command.

## Pressure-control end node

```text
SerialPressureNodeCommandSource     LoRaWAN Class-A downlink
                  |                          |
                  +-------------+------------+
                                  |
                         PressureControlNode
                                  |
              +-------------------+-------------------+
              |                   |                   |
    PressureControlValve   PressureSensorXDB401  BatteryMonitor
              |              upstream/downstream      |
       L298N + power gate       dual I2C Rev A       GPIO35 ADC
                                  |
                           FlowMeter interface
                                  |
                  Tuf2000mFlowMeter Modbus RTU driver
                                  |
                         shared Rs485ModBus
                                  |
                         UART2 GPIO16/GPIO17
```

`PressureControlNode` owns application status and command semantics, not
Serial parsing. It deliberately performs no hydraulic state inference yet;
pressure-derived PCV verification remains `NOT_AVAILABLE` until its validity
conditions are defined and tested.

The application has three compile-time runtime adapters over these shared
components. `pcv_serial_only` omits radio operation. `pcv_hybrid_class_c` keeps
Serial and Class C receive active concurrently and reports periodically.
`pcv_low_power_class_a` disables Serial command parsing, receives a queued
FPort 30 command in RX1/RX2 after its FPort 31 status, sends an immediate result
status, and enters timer deep sleep. The two radio policies deliberately trade
command latency against energy consumption.
