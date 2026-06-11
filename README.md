# Telemetry Weather Station System (PlatformIO / ESP32-S3 / RS485 Modbus)

This repository contains an embedded telemetry firmware and a growing set of reusable libraries and sensor drivers for weather/soil monitoring stations.

The project is built with **PlatformIO** using the **Arduino framework** and targets:
- **ESP32-S3 R8N16** (4D Systems GEN4-ESP32 or compatible clone) on **PCB_TELEMETRY_ESP32S3_V2**

Primary field requirement:
- Reliable RS485/Modbus communication with multiple sensors on real hardware (noisy lines, occasional garbage bytes, timing issues).

---

## Project Goals

1) **Stabilize core libraries** (baseline / вЂњdo not breakвЂќ):
   - `PrintController` (3 debug modes)
   - `RS485ModBus` (transport only: send/receive/CRC/frame extraction)

2) **Add sensor drivers** as separate libraries under `lib/Sensors/<SensorName>/`
   - Each driver must:
     - Build request bytes (without CRC)
     - Define expected response length
     - Define response prefix bytes to validate (usually first 3 bytes)
     - Call RS485 transport to perform transaction and CRC validation
     - Parse payload and store values in member variables
     - Provide a high-level `readAll()`/`update()` method + getters

3) **Provide per-sensor PlatformIO examples** under `examples/<SensorName>_Example/src/main.cpp`
   - Each example should be buildable and flashable independently.

4) Later (after drivers are stable): **station profiles**
   - Different station firmware variants share the same libraries but enable different sensor sets and features.

---

## Repository Layout

```

boards/                      # Custom PlatformIO board definitions (.json)
docs/
datasheets/                # PDFs and hardware docs (keep canonical copies here)
protocols/                 # Markdown protocol notes per sensor (register maps, frames)
notes/                     # Troubleshooting and engineering notes
lib/
PrintController/           # Stable library (do not modify unless explicitly allowed)
RS485ModBus/               # Stable library (transport only)
Sensors/ <SensorName>/            # Sensor driver libs (one sensor = one folder)
examples/ <SensorName>_Example/      # Standalone test firmware for a sensor
src/
main.cpp                   # Production firmware (combined station logic)
platformio.ini               # Stable configuration file (append-only policy for lib_deps)

````

**Important:** file and folder names must be consistent (case-sensitive on CI/Linux).

---

## Hardware / Wiring Notes

- RS485 UART pins for ESP32-S3 are configured in `pcb/PCB_TELEMETRY_ESP32S3_V2.h`.
- Telemetry PCB pinout is opposite to Amudario Firmware: Telemetry uses RX=18, TX=17; Amudario uses RX=17, TX=18.
- RS485 direction control (DE/RE) may be:
  - Disabled (`DE=-1`) for auto-direction modules
  - Enabled with a GPIO pin for manual DE/RE control

Always document actual wiring per PCB revision in `docs/notes/`.

---

## Debugging: PrintController Modes

`PrintController` provides 3 verbosity modes:

- **Mode 0 (BASIC)**  
  Always prints short human-readable results (never silent when enabled).  
  Example: decoded humidity/temperature values.

- **Mode 1 (IO)**  
  Prints TX request frame + extracted RX response frame + decoded values.

- **Mode 2 (FULL)**  
  Prints:
  - TX bytes
  - Full raw RX buffer (including garbage/noise)
  - Match index where a valid frame was found
  - Extracted frame bytes
  - CRC details
  - Parsing steps

**Policy:** sensor debugging must be reproducible using Mode 2 logs.

---

## RS485ModBus Library Contract (Transport Only)

`RS485ModBus` is transport-only. It must NOT contain sensor-specific logic.

Transport responsibilities:
- Append/validate **Modbus RTU CRC16**
- Capture **raw RX bytes** into a buffer (large enough for вЂњgarbage + frameвЂќ cases)
- Search for a valid frame by:
  - matching a caller-provided **prefix** (typically first 3 bytes)
  - enforcing caller-provided **expected frame length**
  - validating **CRC**
- Return extracted frame bytes + metadata (index, CRC status)

Sensor drivers are responsible for:
- Choosing function code, register ranges, expected lengths
- Prefix definition and semantic validation
- Payload decoding and scaling

---

## Branch / AI Agent Workflow

This repo uses a strict branch policy to prevent accidental breakage:

- **`main`**: manual-only stable branch  
  - Only the human maintainer merges into `main` after verification on hardware.

- **`agent-work`** (or another dedicated branch): AI/agent working branch  
  - Cursor / IntelliGravity / other agents work here.
  - All changes must be reviewed before promotion to `main`.

Recommended: enable branch protection on `main`.

### AI Agent Rules (Mandatory)

Agents must follow these rules:

1) Work ONLY in the `agent-work` branch (never push to `main`).
2) Do NOT modify stable libs unless explicitly requested:
   - `lib/PrintController/**`
   - `lib/RS485ModBus/**`
   - `platformio.ini` (append-only `lib_deps` policy)
3) No placeholders. All produced code must be complete and compile-ready.
4) All comments in code must be in English.
5) Minimal diffs. Do not refactor unrelated code.
6) Every change must include:
   - list of changed/added files
   - rationale
   - exact PlatformIO build commands to verify

If an agent has improvement ideas for stable libs, write them into:
- `docs/notes/improvements.md`  
Do NOT edit the stable libraries directly.

---

## Building and Flashing (PlatformIO)

### Install PlatformIO
- Use VS Code + PlatformIO extension.

### Build (examples or firmware)
From the project root:

```bash
pio run -e <environment_name>
````

### Upload / Flash

```bash
pio run -e <environment_name> -t upload
```

### Serial Monitor

```bash
pio device monitor -b 115200
```

Environments are defined in `platformio.ini`.

---

## How to Add a New Sensor Driver

1. Create a new driver library:

   * `lib/Sensors/<SensorName>/src/<SensorName>.h`
   * `lib/Sensors/<SensorName>/src/<SensorName>.cpp`

2. Add a standalone example:

   * `examples/<SensorName>_Example/src/main.cpp`

3. Document the protocol:

   * `docs/protocols/<SensorName>.md`
     Include:
   * register map
   * request/response frames (bytes)
   * scaling (signed/unsigned, /10, /100, float packing)
   * CRC notes
   * hardware test checklist
   * typical value ranges

4. Verify:

   * build + flash example
   * capture Mode 2 logs
   * confirm correct decoding on real hardware

---

## Current Priority / Known Issue

* **Priority:** stabilize `PrintController` and `RS485ModBus` baseline.
* **Known issue under test:** Leaf sensor temperature decoding anomaly (example: `-145`).

  * Must be diagnosed using Mode 2 raw frame logs:
    verify byte order, signed conversion, scaling, and frame extraction alignment.

---

## License

Internal project. License may be defined later.

