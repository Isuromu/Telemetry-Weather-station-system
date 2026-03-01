# 485 Instrument Shelter (Temp/Humi/Luxmeter, RS485 output)

Source: `485 Instrument Shelter User Manual.pdf`

## 1) Overview
- **Purpose / measurements:** Humidity, temperature, illuminance (lux) inside instrument shelter.

## 2) Electrical & RS485 settings
- **Interface / protocol:** RS485 / Modbus RTU (0x03 reads).
- **Serial:** 8N1 (data/parity/stop)
- **Baud:** UNCERTAIN (manual implies standard 9600; confirm)
- **Default Modbus address:** 0x01 (example; typical factory default)
- **Supported address range:** UNCERTAIN (manual does not list address/baud registers)
- **Wiring notes:** RS485 A/B (twisted pair) + GND reference recommended; power V+ and GND per sensor label. If A/B is swapped the sensor will not respond.

**Notes:** Register map includes humidity/temp and lux high/low words.

## 3) Register map
| Register (hex) | Name | Function | Data Type | Endianness | Scale | Units | Range | Notes |
|---:|---|---:|---|---|---:|---|---|---|
| 0x0000 | Humidity | 0x03 | u16 | BE | /10 | %RH | 0–100 | 0.1%RH |
| 0x0001 | Temperature | 0x03 | s16 | BE | /10 | °C | sensor-dependent | 0.1°C |
| 0x0007 | Lux high word | 0x03 | u16 | BE | 1 | Lux | 0..(sensor max) | High 16 bits |
| 0x0008 | Lux low word | 0x03 | u16 | BE | 1 | Lux | 0..(sensor max) | Low 16 bits |


## 4) Read commands (byte-level)
### Read humidity + temperature (2 registers)

**Request (WITHOUT CRC):** `01 03 00 00 00 02`

**Expected response length:** `9 bytes`

**Required response prefix:** `01 03 04`

**Response payload layout:**

```
byte0 addr
byte1 0x03
byte2 0x04
data: humidity u16, temp s16
crc
```

**Parsing formulas:**

humidity = u16/10.0

temperature = int16/10.0

**Worked example (from manual if available):**

Manual includes the standard example mapping FF9B -> -10.1°C and 0292 -> 65.8%RH.

### Read illuminance (2 registers, u32)

**Request (WITHOUT CRC):** `01 03 00 07 00 02`

**Expected response length:** `9 bytes`

**Required response prefix:** `01 03 04`

**Response payload layout:**

```
byte0 addr
byte1 0x03
byte2 0x04
data: highWord u16, lowWord u16
crc
```

**Parsing formulas:**

lux = (u32(high)<<16) | u32(low)

**Worked example (from manual if available):**

Use the same computation as the standalone illuminance sensor.



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
| Temperature | -40 | 80 | °C | Use family-default clamp unless shelter manual specifies otherwise |
| Humidity | 0 | 100 | %RH |  |
| Illuminance | 0 | 200000 | Lux |  |

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
