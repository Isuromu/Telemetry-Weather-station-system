# RS485 Wind Speed Sensor

Source: `RS485 Wind Speed Sensor.pdf`

## 1) Overview
- **Purpose / measurements:** Wind speed.

## 2) Electrical & RS485 settings
- **Interface / protocol:** RS485 / Modbus RTU (0x03).
- **Serial:** 8N1 (data/parity/stop)
- **Baud:** 2400 / 4800 / 9600 (factory default 9600)
- **Default Modbus address:** 0x01 (factory default)
- **Supported address range:** 0–252
- **Wiring notes:** RS485 A/B (twisted pair) + GND reference recommended; power V+ and GND per sensor label. If A/B is swapped the sensor will not respond.

**Notes:** Manual includes read example at register 0x0016.

## 3) Register map
| Register (hex) | Name | Function | Data Type | Endianness | Scale | Units | Range | Notes |
|---:|---|---:|---|---|---:|---|---|---|
| 0x0016 | Wind speed | 0x03 | u16 | BE | /10 | m/s | 0..(sensor max) | 0.1 m/s units |
| 0x0100 | Device address | 0x06 (write) | u16 | BE | 1 | addr | 0–252 | R/W |
| 0x0101 | Baud rate | 0x06 (write) | u16 | BE | enum | bps | 2400/4800/9600 | R/W |


## 4) Read commands (byte-level)
### Read wind speed (1 register)

**Request (WITHOUT CRC):** `01 03 00 16 00 01`

**Expected response length:** `7 bytes`

**Required response prefix:** `01 03 02`

**Response payload layout:**

```
byte 0 : addr
byte 1 : 0x03
byte 2 : 0x02
bytes 3..4 : windSpeed u16 BE
bytes 5..6 : CRC (Lo, Hi)
```

**Parsing formulas:**

wind_speed_ms = u16(raw)/10.0

**Worked example (from manual if available):**

Example: raw 0x0047=71 → 7.1 m/s.



## 5) Write commands (address change / baud change / calibration, if supported)
**This method is sensor-specific.**

### Change Modbus address

**Function code:** `0x06`

**Register:** `0x0100`

**Value format:** u16

**Request (WITHOUT CRC, template):** `[addr] 06 01 00 00 [newAddr]`

**Expected response length:** 8 bytes on wire

**Required response prefix:** `[addr] 06`

**How to validate ACK:** Echo first 6 bytes + CRC

**Notes:** Echo + CRC. Confirm reboot requirement on hardware.

### Change baud rate

**Function code:** `0x06`

**Register:** `0x0101`

**Value format:** u16 enum

**Request (WITHOUT CRC, template):** `[addr] 06 01 01 [valHi] [valLo]`

**Expected response length:** 8 bytes on wire

**Required response prefix:** `[addr] 06`

**How to validate ACK:** Echo first 6 bytes + CRC

**Notes:** Confirm encoding on hardware.

## 6) Range protection & fault detection (response sanity)

These checks are applied **after** Modbus RTU validation (prefix match + expected length + CRC16). They help catch:
- wrong signed/unsigned conversion
- wrong scaling (/10 vs /100)
- misalignment (garbage bytes shifting offsets)
- sensor fault outputs that remain within valid Modbus framing but produce nonsense values

### Hard bounds (reject sample if any value is outside)

| Variable | Min | Max | Units | Raw / notes |
|---|---:|---:|---|---|
| Wind speed | 0 | 30 | m/s | raw u16 in 0.1m/s → 0..300 |

### Rate-of-change / plausibility (optional but recommended)
- **Wind speed:** Reject single-sample jumps > 15 m/s unless verified by multiple consecutive reads

### Fault patterns to treat as invalid (even if within range)
- Any CRC failure, wrong prefix, or wrong length → **discard** (do not update last-good value).
- Repeated raw patterns such as `0xFFFF`, `0x7FFF`, `0x8000` (common “disconnected / error” placeholders) → treat as **fault** if seen consistently for this sensor.
- “Stuck value” detection: if value never changes across many samples while other sensors are changing (and environment should vary), mark as **suspect**.

### Recommended driver behavior
- Keep `last_good` value per field.
- On invalid sample: increment `invalid_count` and do not overwrite `last_good`.
- After N consecutive invalid samples (e.g. N=3..10 depending on poll rate), raise a **sensor_fault** flag for telemetry.


## 7) Examples & pitfalls
- Signed vs unsigned conversion (especially temperature)
- Wrong scaling (/10 vs /100 vs /1000)
- Wrong byte order when sensors use multi-register values
- Reading wrong offsets due to garbage bytes (always prefix-scan + length + CRC)
- Confusing Modbus function 0x03 (holding) vs 0x04 (input)


## 8) Hardware test procedure (real RS485 line)
1. Enable FULL raw logging on your RS485 transport (raw RX buffer + extracted frame + CRC status).
2. Send the baseline read command(s) shown in this file.
3. Capture one raw RX buffer.
4. Scan the buffer for the required prefix bytes (addr + func + byteCount).
5. From that offset, slice the expected frame length and verify Modbus RTU CRC16 (CRC Lo then CRC Hi).
6. Manually compute raw register words from the data bytes and verify scaling and signedness.
7. Only after CRC-valid parsing is stable, integrate into the C++ driver.
