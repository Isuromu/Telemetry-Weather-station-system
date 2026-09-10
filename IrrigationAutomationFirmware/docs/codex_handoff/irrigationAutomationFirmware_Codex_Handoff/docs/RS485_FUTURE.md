# Future RS485 Integration

## Why RS485 is deferred

The current step is PCV + pressure sensors + LoRa.

RS485 is planned next and requires a UART allocation, but it should not complicate the first working firmware.

## Preferred pin allocation after I2C multiplexing

- TX: GPIO13
- RX: GPIO34
- optional DE/RE: GPIO4

GPIO34 is input-only, which is ideal for UART RX.

## Transceiver options

The project previously used an auto-direction RS485 module with:

- 3.3/5 V supply/signal compatibility;
- automatic direction switching;
- no software DE/RE pin required.

If this module is reused:
- GPIO13 TX
- GPIO34 RX
- GPIO4 remains available.

If a MAX3485-style manual-direction transceiver is used:
- GPIO4 can be DE/RE.

## Existing internal library

Existing assets in the user file set:

- `RS485ModBus.h`
- `RS485ModBus.cpp`
- `PrintController.h`
- `PrintController.cpp`
- `library.json`

The library already supports custom ESP32 UART pins and optional direction control.

Before writing new RS485 transport code, inspect and reuse/refactor these assets.

## Pump VFD

The Delixi CDI-E series manual is already available.

Relevant Modbus concepts from the manual:

- function code 06H for command writes;
- A000H command register;
- 0001H forward run;
- 0002H reverse run;
- 0005H free stop;
- 0006H shutdown by speed reduction;
- A001H frequency command;
- monitoring registers around B000H.

For the irrigation pump, reverse operation is not required.

A minimum run frequency around 10 Hz was previously discussed for this project.

## Separate RS485 motorized valve

There is also an FC11C-related motorized-valve subsystem.

Desired API eventually includes:

- open;
- close;
- set position/percentage;
- read feedback;
- calibration;
- fault/status handling.

This device must remain separate in naming and code from `PressureControlValve`, which uses the latching solenoid and L298N.
