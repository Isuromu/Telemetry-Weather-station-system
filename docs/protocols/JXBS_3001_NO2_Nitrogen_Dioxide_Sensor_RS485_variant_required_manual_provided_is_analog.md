# JXBS-3001-NO2 Nitrogen Dioxide Sensor (RS485 variant required, manual provided is analog)

Source: `JXBS-3001-NO2 20240628.pdf`

## 1) Overview
- **Purpose / measurements:** Nitrogen dioxide concentration (NO2).

## 2) Electrical & RS485 settings
- **Interface / protocol:** RS485 / Modbus RTU (PROJECT REQUIREMENT), but provided document describes analog outputs only.
- **Serial:** 8N1 (data/parity/stop)
- **Baud:** UNCERTAIN
- **Default Modbus address:** UNCERTAIN
- **Supported address range:** UNCERTAIN
- **Wiring notes:** RS485 A/B (twisted pair) + GND reference recommended; power V+ and GND per sensor label. If A/B is swapped the sensor will not respond.

**Notes:** The uploaded manual content is for analog outputs (4–20mA, 0–10V, 0–5V) and does not contain Modbus RTU registers/frames. Do not invent Modbus registers. Obtain the RS485 Modbus manual for the NO2 RS485 model or reverse-engineer via safe probing.

## 3) Register map
_Register map is **UNCERTAIN** from the provided documents (no Modbus register table extracted). Use the hardware test procedure below to discover registers safely._


## 4) Read commands (byte-level)
_No read command examples were extractable from the provided file. Use the hardware test procedure to discover a working read command._

## 5) Write commands (address change / baud change / calibration, if supported)
**This method is sensor-specific.**

_Write operations are **UNCERTAIN** or not documented in the provided files. Many JXCT/JXBS RS485 sensors support address/baud changes via registers like 0x0100/0x0101, but you must confirm per-sensor on real hardware before implementing._

## 6) Range protection & fault detection (response sanity)

These checks are applied **after** Modbus RTU validation (prefix match + expected length + CRC16). They help catch:
- wrong signed/unsigned conversion
- wrong scaling (/10 vs /100)
- misalignment (garbage bytes shifting offsets)
- sensor fault outputs that remain within valid Modbus framing but produce nonsense values

### Hard bounds (reject sample if any value is outside)

| Variable | Min | Max | Units | Raw / notes |
|---|---:|---:|---|---|
| NO2 | 0 | 2000 | ppm | manual lists 0–20 ppm or 0–2000 ppm variants (analog doc); configure expected max for your RS485 model |

### Fault patterns to treat as invalid (even if within range)
- Any CRC failure, wrong prefix, or wrong length → **discard** (do not update last-good value).
- Repeated raw patterns such as `0xFFFF`, `0x7FFF`, `0x8000` (common “disconnected / error” placeholders) → treat as **fault** if seen consistently for this sensor.
- “Stuck value” detection: if value never changes across many samples while other sensors are changing (and environment should vary), mark as **suspect**.

### Recommended driver behavior
- Keep `last_good` value per field.
- On invalid sample: increment `invalid_count` and do not overwrite `last_good`.
- After N consecutive invalid samples (e.g. N=3..10 depending on poll rate), raise a **sensor_fault** flag for telemetry.


## 7) Examples & pitfalls
- Manual mismatch: analog protocol info is useless for Modbus driver implementation.
- Do not guess that reg 0x0006 is gas concentration just because other gas sensors use it. Confirm on your exact unit.

## 8) Hardware test procedure (real RS485 line)
1. Enable FULL raw logging on your RS485 transport (raw RX buffer + extracted frame + CRC status).
2. Send the baseline read command(s) shown in this file.
3. Capture one raw RX buffer.
4. Scan the buffer for the required prefix bytes (addr + func + byteCount).
5. From that offset, slice the expected frame length and verify Modbus RTU CRC16 (CRC Lo then CRC Hi).
6. Manually compute raw register words from the data bytes and verify scaling and signedness.
7. Only after CRC-valid parsing is stable, integrate into the C++ driver.
