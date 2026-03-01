# JXBS-3001-YMSD Leaf Surface Humidity Transmitter (RS485)

Source: `JXBS-3001-YMSD 485 Leaf Moisture Temperature.pdf`

## 1) Overview
- **Purpose / measurements:** Leaf surface humidity and leaf surface temperature.

## 2) Electrical & RS485 settings
- **Interface / protocol:** RS485 / Modbus RTU (function 0x03 for reads).
- **Serial:** 8N1 (data/parity/stop)
- **Baud:** 2400 / 4800 / 9600 (factory default 9600)
- **Default Modbus address:** 0x01
- **Supported address range:** 0–252
- **Wiring notes:** RS485 A/B (twisted pair) + GND reference recommended; power V+ and GND per sensor label. If A/B is swapped the sensor will not respond.

**Notes:** Manual tables in this PDF are partially non-extractable by text tools, but read example and register map are known from the manual. Always validate by prefix+length+CRC due to noisy bus.

## 3) Register map
| Register (hex) | Name | Function | Data Type | Endianness | Scale | Units | Range | Notes |
|---:|---|---:|---|---|---:|---|---|---|
| 0x0020 | Humidity | 0x03 | u16 | BE | /10 | %RH | 0–100 | 0.1%RH units |
| 0x0021 | Temperature | 0x03 | s16 | BE | /10 | °C | -20..80 | 0.1°C units; signed two’s complement for negatives |
| 0x0100 | Device address | 0x06 (write) | u16 | BE | 1 | addr | 0–252 | Sensor-specific address change register (confirm on hardware) |
| 0x0101 | Baud rate | 0x06 (write) | u16 | BE | enum | bps | 2400/4800/9600 | Sensor-specific encoding (confirm on hardware) |


## 4) Read commands (byte-level)
### Read humidity + temperature (2 registers)

**Request (WITHOUT CRC):** `01 03 00 20 00 02`

**Expected response length:** `9 bytes`

**Required response prefix:** `01 03 04`

**Response payload layout:**

```
byte 0 : addr (0x01)
byte 1 : func (0x03)
byte 2 : byteCount (0x04)
bytes 3..4 : humidity u16 BE
bytes 5..6 : temperature s16 BE (two’s complement)
bytes 7..8 : CRC16 (Lo, Hi)
```

**Parsing formulas:**

humidity_percent = u16(hum_raw) / 10.0

temperature_c = int16(temp_raw) / 10.0

**Worked example (from manual if available):**

Example from manual family: humidity raw 0x0292 = 658 → 65.8%RH; temperature raw 0xFF9B = -101 → -10.1°C.



## 5) Write commands (address change / baud change / calibration, if supported)
**This method is sensor-specific.**

### Change Modbus address (UNCERTAIN: confirm register/value rules on hardware)

**Function code:** `0x06`

**Register:** `0x0100`

**Value format:** u16, new address in low byte (0–252).

**Request (WITHOUT CRC, template):** `[oldAddr] 06 01 00 00 [newAddr]`

**Expected response length:** 8 bytes (echo + CRC) on wire; without CRC the echo is 6 bytes.

**Required response prefix:** `[oldAddr] 06`

**How to validate ACK:** Response first 6 bytes must exactly echo request first 6 bytes; then CRC must pass.

**Notes:** Some JXCT sensors accept writes to 0x0100 and respond with an echo frame. Power-cycle may be required.

### Change baud rate (UNCERTAIN: encoding must be confirmed)

**Function code:** `0x06`

**Register:** `0x0101`

**Value format:** u16 enum; manual states 2400/4800/9600 are supported.

**Request (WITHOUT CRC, template):** `[addr] 06 01 01 [valHi] [valLo]`

**Expected response length:** 8 bytes (echo + CRC) on wire.

**Required response prefix:** `[addr] 06`

**How to validate ACK:** Echo + CRC.

**Notes:** Because encoding is not reliably extractable from this PDF, you must confirm on hardware or vendor tool before implementing.

## 6) Range protection & fault detection (response sanity)

These checks are applied **after** Modbus RTU validation (prefix match + expected length + CRC16). They help catch:
- wrong signed/unsigned conversion
- wrong scaling (/10 vs /100)
- misalignment (garbage bytes shifting offsets)
- sensor fault outputs that remain within valid Modbus framing but produce nonsense values

### Hard bounds (reject sample if any value is outside)

| Variable | Min | Max | Units | Raw / notes |
|---|---:|---:|---|---|
| Leaf humidity | 0 | 100 | %RH | raw u16 in 0.1%RH → expect 0..1000 |
| Leaf temperature | -20 | 80 | °C | raw s16 in 0.1°C → expect -200..800 |

### Fault patterns to treat as invalid (even if within range)
- Any CRC failure, wrong prefix, or wrong length → **discard** (do not update last-good value).
- Repeated raw patterns such as `0xFFFF`, `0x7FFF`, `0x8000` (common “disconnected / error” placeholders) → treat as **fault** if seen consistently for this sensor.
- “Stuck value” detection: if value never changes across many samples while other sensors are changing (and environment should vary), mark as **suspect**.

### Recommended driver behavior
- Keep `last_good` value per field.
- On invalid sample: increment `invalid_count` and do not overwrite `last_good`.
- After N consecutive invalid samples (e.g. N=3..10 depending on poll rate), raise a **sensor_fault** flag for telemetry.


## 7) Examples & pitfalls
- Temperature must be parsed as int16_t before dividing by 10. Wrong signed conversion can create values like -145.
- If you divide by 1 (instead of 10), -14.5°C appears as -145.
- Garbage bytes can shift offsets; never assume frame starts at index 0. Scan for prefix 01 03 04 then slice 9 bytes and CRC-check.

## 8) Hardware test procedure (real RS485 line)
1. Enable FULL raw logging on your RS485 transport (raw RX buffer + extracted frame + CRC status).
2. Send the baseline read command(s) shown in this file.
3. Capture one raw RX buffer.
4. Scan the buffer for the required prefix bytes (addr + func + byteCount).
5. From that offset, slice the expected frame length and verify Modbus RTU CRC16 (CRC Lo then CRC Hi).
6. Manually compute raw register words from the data bytes and verify scaling and signedness.
7. Only after CRC-valid parsing is stable, integrate into the C++ driver.
