# Source Files Known to Exist in ChatGPT File Library

These files were found in the user's File Library during preparation of this handoff. They are not embedded automatically into this ZIP because File Library references are not the same as local sandbox files.

Attach the relevant originals to Codex when working on those subsystems.

## Pump / VFD

### `CDI-E%20frequency%20inverter%20manual.pdf`

Delixi E Series frequency inverter manual, approximately 230 pages.

Relevant areas:
- installation and control wiring;
- PID control for constant-pressure water supply;
- Chapter 8 RS485 communication;
- Modbus command and monitoring registers.

## Existing RS485 library

### `RS485ModBus.h`
### `RS485ModBus.cpp`
### `PrintController.h`
### `PrintController.cpp`
### `library.json`

Internal Arduino/ESP32 RS485/Modbus transport code.

Important features:
- custom ESP32 RX/TX pins;
- optional direction-control pin;
- Modbus CRC16;
- buffered receive;
- retry behavior;
- debug logging.

## FC11C / motorized valve

### `1000103154.jpg`

Photo of the FC11C controller/module used in the separate motorized-valve discussion.

## Other irrigation-related source material

### `Soil Temperature and moisture and EC salinity 4 in 1 sensor-RD-SMTES-4 wires.pdf`

Manual for the RS485 soil sensor used in the broader field-node context.

## Note

A prior `soil_sensor_node.ino` was discussed in the project, but it was not located by the File Library search used for this handoff. Do not assume it is absent from the user's local repository.
