# JXBS-3001-NPK-RS Soil Multi-parameter Sensor (RS485)

Source: `RS485-Soil Multi-parameter Sensor.pdf`

## 1) Overview
- **Purpose / measurements:** Soil pH, moisture, temperature, conductivity, N/P/K.

## 2) Electrical & RS485 settings
- **Interface / protocol:** RS485 / Modbus RTU (0x03 reads).
- **Serial:** 8N1 (data/parity/stop)
- **Baud:** 2400 / 4800 / 9600 (factory default 9600)
- **Default Modbus address:** 0x01 (example; typical factory default)
- **Supported address range:** 0–255 (per manual)
- **Wiring notes:** RS485 A/B (twisted pair) + GND reference recommended; power V+ and GND per sensor label. If A/B is swapped the sensor will not respond.

**Notes:** Register map and multiple communication examples are present.

## 3) Register map
| Register (hex) | Name | Function | Data Type | Endianness | Scale | Units | Range | Notes |
|---:|---|---:|---|---|---:|---|---|---|
| 0x0006 | pH | 0x03 | u16 | BE | /100 | pH | 0..14 | 0.01 pH |
| 0x0012 | Soil moisture | 0x03 | u16 | BE | /10 | % | 0..100 | 0.1% |
| 0x0013 | Soil temperature | 0x03 | s16 | BE | /10 | °C | sensor-dependent | 0.1°C signed |
| 0x0015 | Soil conductivity | 0x03 | u16 | BE | 1 | µS/cm | sensor-dependent |  |
| 0x001E | Nitrogen | 0x03 | u16 | BE | 1 | mg/kg | sensor-dependent |  |
| 0x001F | Phosphorus | 0x03 | u16 | BE | 1 | mg/kg | sensor-dependent |  |
| 0x0020 | Potassium | 0x03 | u16 | BE | 1 | mg/kg | sensor-dependent |  |
| 0x0100 | Equipment address | 0x06 (write) | u16 | BE | 1 | addr | 0–255 | R/W |
| 0x0101 | Baud rate | 0x06 (write) | u16 | BE | enum | bps | 2400/4800/9600 | R/W |


## 4) Read commands (byte-level)
### Read soil moisture + temperature (2 registers)

**Request (WITHOUT CRC):** `01 03 00 12 00 02`

**Expected response length:** `9 bytes`

**Required response prefix:** `01 03 04`

**Response payload layout:**

```
byte 0 : addr
byte 1 : 0x03
byte 2 : 0x04
bytes 3..4 : moisture u16 BE
bytes 5..6 : temperature s16 BE
bytes 7..8 : CRC
```

**Parsing formulas:**

moisture_percent = u16(moisture)/10.0

temperature_c = int16(temp)/10.0

**Worked example (from manual if available):**

Manual example: moisture 65.8%RH, temperature -10.1°C (family example).

### Read pH (1 register)

**Request (WITHOUT CRC):** `01 03 00 06 00 01`

**Expected response length:** `7 bytes`

**Required response prefix:** `01 03 02`

**Response payload layout:**

```
byte0 addr
byte1 0x03
byte2 0x02
bytes3..4 pH u16 BE
bytes5..6 CRC
```

**Parsing formulas:**

pH = u16(raw)/100.0

**Worked example (from manual if available):**

Example: raw 308 → pH 3.08.

### Read NPK (3 registers: N,P,K)

**Request (WITHOUT CRC):** `01 03 00 1E 00 03`

**Expected response length:** `11 bytes`

**Required response prefix:** `01 03 06`

**Response payload layout:**

```
byte0 addr
byte1 0x03
byte2 0x06
bytes3..4 : N u16
bytes5..6 : P u16
bytes7..8 : K u16
bytes9..10 : CRC
```

**Parsing formulas:**

N_mgkg=u16(N)
P_mgkg=u16(P)
K_mgkg=u16(K)

**Worked example (from manual if available):**

Example family: N=32, P=37, K=48.



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
| Soil moisture | 0 | 100 | % | raw u16 in 0.1% → 0..1000 |
| Soil temperature | -40 | 80 | °C | raw s16 in 0.1°C → -400..800 |
| Soil EC | 0 | 10000 | µS/cm | raw u16 |
| Soil pH | 3 | 9 | pH | raw u16 in 0.01 pH → 300..900 |
| N/P/K | 0 | UNCERTAIN | mg/kg | doc excerpt didn’t include explicit max; treat absurd values (e.g. > 5000) as suspicious |

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
