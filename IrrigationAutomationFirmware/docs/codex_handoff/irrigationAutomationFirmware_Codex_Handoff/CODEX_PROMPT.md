# Prompt to Use in Codex

Open the local repository `irrigationAutomationFirmware` and treat this handoff package as engineering context, not as a reason to overwrite working code blindly.

Read `CODEX_START_HERE.md` first, then all files under `docs/`.

The current board is ESP32-WROOM-32D / DevKitC V4 with Arduino framework in VS Code + PioArduino/PlatformIO.

Mandatory naming:
- current hydraulic pressure-reducing/control valve: `PressureControlValve` / `PCV`;
- its 12 V latching solenoid: `PressureControlValveSolenoid`;
- separate FC11C RS485 motorized actuator: `MotorizedValveActuator`;
- pump frequency inverter: `PumpVfd`.

Mandatory current pinout:
- battery ADC GPIO35;
- PCV L298N IN1 GPIO16;
- PCV L298N IN2 GPIO17;
- PCV L298N high-side power enable GPIO27;
- I2C upstream SDA21/SCL22;
- I2C downstream SDA13/SCL4;
- LoRa: NSS5, MOSI23, DIO1 26, RXEN33, NRST14, SCK18, MISO19, BUSY25, TXEN32.

The solenoid is latching. OPEN and CLOSE must be opposite polarity pulses. Never implement CLOSE as merely removing power.

Actuation sequence:
1. IN1/IN2 LOW.
2. Enable L298N power through GPIO27.
3. Wait configurable power-settle time.
4. Apply desired polarity.
5. Hold configurable pulse time.
6. IN1/IN2 LOW.
7. Wait configurable post-pulse time.
8. Disable L298N power.
9. Wait configurable hydraulic-settle time before pressure verification.

After reboot the physical PCV state is `Unknown` unless reliably verified. Never label a RAM boolean as physical valve feedback.

Refactor hardware functions so Serial and LoRa use the same modules:
- BatteryMonitor
- PressureSensorXDB401
- PressureControlValve
- application/status layer

Create/update two examples:
1. Serial control/diagnostics with commands:
   `help`, `status`, `battery`, `pressure`, `pcv open`, `pcv close`, `pcv state`.
2. LoRa control with OPEN/CLOSE/STATUS semantics.

Before implementing the final LoRa transport, inspect the repository and gateway configuration to determine whether this node is raw LoRa or LoRaWAN/ChirpStack. A raw RadioLib example may exist only as a clearly named hardware bring-up example.

The current prototype uses two I2C buses because the two XDB401 sensors may have identical fixed addresses. Do not break the assembled prototype. For the next hardware revision, document/use a single I2C bus with an I2C multiplexer and reserve GPIO13 TX + GPIO34 RX for future RS485, with GPIO4 optional DE/RE.

Inspect and reuse/refactor the existing `RS485ModBus` library when RS485 is added later. Do not implement RS485 in this step unless it is already required for a successful build.

Preserve code/comments in English. Keep the implementation simple, deterministic and hardware-testable. Build the Serial example first and report build errors precisely. Do not invent unverified XDB401 scaling, solenoid pulse duration, valve polarity, or network mode; keep those centralized and explicitly marked as values to validate on hardware.
