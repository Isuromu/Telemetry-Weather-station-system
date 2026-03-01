# RS485 Evaporation Sensor (JXCT family)

Source: `RS485 Evaporation Sensor Manual - JXCT Manual.pdf`

## 1) Overview
- **Purpose / measurements:** Evaporation measurement system; register example reads a single value (often water weight or evaporation-related).

## 2) Electrical & RS485 settings
- **Interface / protocol:** RS485 / Modbus RTU (0x03).
- **Serial:** 8N1 (data/parity/stop)
- **Baud:** 2400 / 4800 / 9600 (factory default 9600)
- **Default Modbus address:** 0x01 (factory default)
- **Supported address range:** 0–252
- **Wiring notes:** RS485 A/B (twisted pair) + GND reference recommended; power V+ and GND per sensor label. If A/B is swapped the sensor will not respond.

**Notes:** Manual shows read at register 0x0006 length 1 and a tare/clear write at 0x0102.

## 3) Register map
| Register (hex) | Name | Function | Data Type | Endianness | Scale | Units | Range | Notes |
|---:|---|---:|---|---|---:|---|---|---|
| 0x0006 | Evaporation value / water weight (manual naming varies) | 0x03 | u16 | BE | 1 | (manual-defined) | sensor-dependent | Confirm meaning: some manuals treat this as water weight (g). |
| 0x0102 | Tare / clear command | 0x06 | u16 | BE | 1 | command | 0 or 1 | Write-only command (manual example writes 1). |
| 0x0100 | Device address | 0x06 (write) | u16 | BE | 1 | addr | 0–252 | R/W (confirm) |
| 0x0101 | Baud rate | 0x06 (write) | u16 | BE | enum | bps | 2400/4800/9600 | R/W (confirm) |


## 4) Read commands (byte-level)
### Read evaporation-related value (1 register)

**Request (WITHOUT CRC):** `01 03 00 06 00 01`

**Expected response length:** `7 bytes`

**Required response prefix:** `01 03 02`

**Response payload layout:**

```
byte 0 : addr
byte 1 : 0x03
byte 2 : 0x02
bytes 3..4 : value u16 BE
bytes 5..6 : CRC (Lo, Hi)
```

**Parsing formulas:**

value = u16(raw) (apply scaling per your manual; if not specified treat as integer and confirm with vendor tool)

**Worked example (from manual if available):**

Manual example uses the same family reply pattern.



## 5) Write commands (address change / baud change / calibration, if supported)
**This method is sensor-specific.**

### Tare / clear (write to 0x0102)

**Function code:** `0x06`

**Register:** `0x0102`

**Value format:** u16, command value (manual example uses 0x0001).

**Request (WITHOUT CRC, template):** `[addr] 06 01 02 00 01`

**Expected response length:** 8 bytes on wire (echo + CRC)

**Required response prefix:** `[addr] 06 01 02`

**How to validate ACK:** Echo first 6 bytes exactly + CRC OK.

**Notes:** Used to reset/tare the evaporation barrel weight baseline. Confirm exact semantics on hardware.

## 6) Range protection & fault detection (response sanity)

These checks are applied **after** Modbus RTU validation (prefix match + expected length + CRC16). They help catch:
- wrong signed/unsigned conversion
- wrong scaling (/10 vs /100)
- misalignment (garbage bytes shifting offsets)
- sensor fault outputs that remain within valid Modbus framing but produce nonsense values

### Hard bounds (reject sample if any value is outside)

| Variable | Min | Max | Units | Raw / notes |
|---|---:|---:|---|---|
| Evaporation (height) | 0 | 200 | mm | resolution ≤0.01 mm |

### Rate-of-change / plausibility (optional but recommended)
- **Evaporation (height):** Use a conservative ROC clamp, e.g. > 50 mm/hour is very likely a fault

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
