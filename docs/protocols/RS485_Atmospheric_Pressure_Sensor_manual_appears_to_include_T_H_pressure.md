# RS485 Atmospheric Pressure Sensor (manual appears to include T/H + pressure)

Source: `RS485-Atmospheric pressure.doc (converted to txt)`

## 1) Overview
- **Purpose / measurements:** Atmospheric pressure (and in this manual, also humidity/temperature registers appear).

## 2) Electrical & RS485 settings
- **Interface / protocol:** RS485 / Modbus RTU (0x03 reads).
- **Serial:** 8N1 (data/parity/stop)
- **Baud:** 2400/4800/9600 (factory default 9600) per manual template
- **Default Modbus address:** 0x01 (factory default)
- **Supported address range:** UNCERTAIN (not extracted; likely 0–252)
- **Wiring notes:** RS485 A/B (twisted pair) + GND reference recommended; power V+ and GND per sensor label. If A/B is swapped the sensor will not respond.

**Notes:** The provided doc is a WPS Word export. It includes a combined read example: start 0x0000 length 0x000C (12 registers). Pressure appears as a 32-bit value with scale /100 to get mbar (example: 100549 -> 1005.49 mbar), but exact offsets for the pressure word(s) are not cleanly extractable.

## 3) Register map
| Register (hex) | Name | Function | Data Type | Endianness | Scale | Units | Range | Notes |
|---:|---|---:|---|---|---:|---|---|---|
| 0x0000 | Humidity | 0x03 | u16 | BE | /10 | %RH | 0–100 | Present in manual example tables |
| 0x0001 | Temperature | 0x03 | s16 | BE | /10 | °C | sensor-dependent | Present in manual example tables |
| 0x000? | Atmospheric pressure (32-bit) | 0x03 | u32 | BE words | /100 | mbar | sensor-dependent | UNCERTAIN exact register address; manual shows 32-bit pressure data and scale. |


## 4) Read commands (byte-level)
### Read humidity+temperature+pressure block (12 registers, from manual example)

**Request (WITHOUT CRC):** `01 03 00 00 00 0C`

**Expected response length:** `29 bytes`

**Required response prefix:** `01 03 18`

**Response payload layout:**

```
byte 0 : addr
byte 1 : 0x03
byte 2 : 0x18 (24 data bytes)
bytes 3..26 : data (12 registers = 24 bytes), order u16 BE per register
bytes 27..28 : CRC (Lo, Hi)

UNCERTAIN: exact register-to-field mapping beyond humidity/temp is not cleanly extractable from the provided DOC. Use logging + manual computation to locate the pressure words.
```

**Parsing formulas:**

humidity_percent = u16(data[0])/10.0

temperature_c = int16(data[1])/10.0

pressure_mbar = u32(pressure_words)/100.0  (find correct word indices by matching manual example magnitude ~1000 mbar)

**Worked example (from manual if available):**

Manual mentions an example pressure raw value 100549 → 1005.49 mbar (scale /100).



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
| Atmospheric pressure | 10 | 1200 | mbar | Typical ambient is ~800..1100 mbar; treat outside that as suspicious but not invalid |

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
