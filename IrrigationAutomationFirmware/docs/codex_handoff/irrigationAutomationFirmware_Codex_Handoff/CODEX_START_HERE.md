# Irrigation Automation Firmware — Codex Handoff

## Purpose

This package transfers the full currently known engineering context for the `irrigationAutomationFirmware` project into Codex without mixing several different valves, nodes, or communication layers.

The immediate firmware target is the ESP32 irrigation control node built around:

- 12 V lead-acid battery + solar charge controller
- over-discharge protection between battery and system
- ESP32-WROOM-32D / DevKitC V4
- battery voltage measurement on GPIO35
- two XDB401 I2C pressure sensors, upstream and downstream of the pressure-control valve
- one 12 V latching solenoid
- L298N H-bridge for polarity reversal
- high-side P-channel MOSFET power gate for the L298N, driven through a BJT from GPIO27
- DX-LR30 / SX1262 LoRa radio
- future RS485/UART expansion

All source code and code comments must be in English.

## Canonical naming

Do not use the generic name `Valve` for the current hydraulic device.

Use:

- `PressureControlValve` or `PCV` — the hydraulic pressure-control / pressure-reducing valve as a complete device.
- `PressureControlValveSolenoid` — its 12 V latching solenoid.
- `PressureControlValveDriver` — L298N + high-side power switch used to pulse the solenoid.
- `UpstreamPressureSensor` — pressure sensor before the PCV.
- `DownstreamPressureSensor` — pressure sensor after the PCV.
- `MotorizedValveActuator` — the separate RS485/FC11C motorized valve subsystem discussed elsewhere in this project.
- `PumpVfd` — the Delixi CDI-E series frequency inverter controlling the pump.
- `SoilSensorNode` — the separate low-power LoRa/RS485 field sensor node.
- `IrrigationGateway` — RPi/RAK/ChirpStack-side gateway subsystem.

This naming distinction is mandatory. The old trainee code used `valve`, which is too ambiguous for this project.

## Current hardware revision

Treat the currently assembled board as **Prototype Rev A**:

- two physical I2C buses because the two pressure sensors can have the same fixed address;
- L298N on GPIO16/GPIO17;
- L298N power gate on GPIO27;
- LoRa pinout fixed as documented below;
- future RS485 is not fitted yet.

Treat the preferred next hardware revision as **Target Rev B**:

- one I2C controller, preferably GPIO21/GPIO22;
- an I2C multiplexer such as TCA9548A to isolate the two identical-address pressure sensors;
- GPIO13/GPIO34 reserved for future RS485 UART TX/RX;
- GPIO4 optionally available for DE/RE if a non-auto-direction RS485 transceiver is used.

## Immediate Codex tasks

1. Inspect the existing local repository `irrigationAutomationFirmware`.
2. Preserve any working project configuration that already exists.
3. Add a clear pin/config header instead of scattering pin numbers.
4. Implement reusable modules for:
   - battery voltage monitoring;
   - XDB401 pressure sensor access;
   - pressure-control valve latching-solenoid control.
5. Add two examples:
   - Serial Monitor control and diagnostics;
   - LoRa control.
6. Keep hardware functions independent from the transport:
   - Serial and LoRa must call the same PCV/sensor/battery APIs.
7. Do not implement RS485 yet, but reserve/document the future UART plan.
8. Build the Serial example first.
9. For LoRa, inspect the repository before deciding whether the existing system is raw LoRa or LoRaWAN. Do not silently replace one with the other.
10. Do not treat pressure-derived state as guaranteed physical valve feedback unless hydraulic conditions make the inference valid.

## Critical PCV control rule

The solenoid is now confirmed to be **latching**.

Opening and closing require opposite polarity pulses. Removing power alone is not a close command.

A safe actuation sequence is:

1. Set both L298N inputs LOW.
2. Enable the L298N 12 V power rail through GPIO27.
3. Wait for the switched power rail and H-bridge to settle.
4. Apply the required polarity:
   - OPEN polarity: one IN1/IN2 combination;
   - CLOSE polarity: the opposite combination.
5. Hold for the configured pulse time.
6. Return IN1 and IN2 LOW.
7. Wait a short post-pulse time.
8. Disable L298N power through GPIO27.
9. Allow hydraulic pressure to settle.
10. Read upstream/downstream pressure if verification is meaningful.

The exact pulse duration and which polarity corresponds to OPEN/CLOSE must be validated on the real valve. Keep those values centralized in configuration.

## State model

After ESP32 reset, the actual latching-valve state is not automatically known.

Use at least:

- `Unknown`
- `Open`
- `Closed`

The firmware may remember the last commanded state, but it must not present that as guaranteed physical feedback.

Pressure sensors may help verify the result only when the hydraulic system is in a condition where upstream/downstream pressure can actually distinguish the PCV state. With the pump stopped, no flow, equalized pressure, or an unpressurized line, pressure cannot be trusted as valve-position feedback.

## Source package contents

- `docs/PROJECT_CONTEXT.md` — full consolidated context.
- `docs/HARDWARE_ARCHITECTURE.md` — power, pinout and hardware revisions.
- `docs/PRESSURE_CONTROL_VALVE.md` — latching solenoid state machine.
- `docs/FIRMWARE_ARCHITECTURE.md` — software structure and interfaces.
- `docs/LORA_AND_GATEWAY.md` — current radio/gateway context and unresolved network-mode issue.
- `docs/RS485_FUTURE.md` — future UART allocation and existing RS485 assets.
- `docs/XDB401_NOTES.md` — current pressure-sensor code assumptions and validation points.
- `docs/CODE_REVIEW_TRAINEE.md` — review of the trainee firmware.
- `docs/OPEN_ITEMS.md` — unresolved engineering questions.
- `docs/UZB_QUICK_REFERENCE.md` — short Uzbek Latin quick reference.
- `docs/source_manifest/FILE_LIBRARY_MANIFEST.md` — known source files that exist in ChatGPT File Library and should also be attached to Codex when needed.
- `firmware_scaffold/` — compile-oriented starting code for the common modules and two examples.

## Working rule for Codex

When the repository conflicts with this handoff, inspect the real hardware-oriented source first, then update the documentation together with the implementation. Do not preserve an old name or behavior merely because it already exists.
