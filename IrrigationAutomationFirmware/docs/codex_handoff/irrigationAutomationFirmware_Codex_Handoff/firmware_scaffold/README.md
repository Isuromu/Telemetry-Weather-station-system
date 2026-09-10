# Firmware Scaffold

This scaffold is a transfer aid for the existing `irrigationAutomationFirmware` repository.

Do not blindly replace the real repository with this folder.

## Files

- `include/BoardPins.hpp`
- `include/SystemConfig.hpp`
- `lib/BatteryMonitor`
- `lib/PressureSensorXDB401`
- `lib/PressureControlValve`
- `examples/SerialControl/main.cpp`
- `examples/RawLoRaControl/main.cpp`

## Integration order

1. Integrate `BoardPins.hpp` and `SystemConfig.hpp`.
2. Integrate and build `BatteryMonitor`.
3. Integrate and build `PressureSensorXDB401`.
4. Integrate and bench-test `PressureControlValve`.
5. Build/run `SerialControl`.
6. Confirm actual OPEN/CLOSE polarity and pulse timing.
7. Confirm pressure-sensor conversion.
8. Only then integrate the LoRa transport.
9. Inspect the deployed network before deciding raw LoRa versus LoRaWAN.

## RadioLib

The raw LoRa example expects RadioLib and an SX1262-compatible API.

The exact library version should be selected according to the existing repository and PioArduino/PlatformIO environment rather than pinned here without inspecting that repository.
