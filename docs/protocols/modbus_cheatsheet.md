# Modbus RTU / RS485 Cheat Sheet (Telemetry Weather Station System)

## CRC16 Modbus RTU (CRC Lo then CRC Hi inside the frame)
- CRC polynomial: **0xA001** (LSB-first algorithm), init **0xFFFF**
- CRC bytes are appended as: **CRC_L (low byte) then CRC_H (high byte)**

### Minimal reference implementation (algorithm description)
1. Start `crc = 0xFFFF`
2. For each byte `b`:
   - `crc ^= b`
   - repeat 8 times:
     - if `(crc & 0x0001) != 0`: `crc = (crc >> 1) ^ 0xA001`
     - else: `crc >>= 1`
3. Append `crc & 0xFF` then `(crc >> 8) & 0xFF`

## Common function codes used by these sensors
### 0x03 Read Holding Registers
**Request (WITHOUT CRC):**
`[addr] [03] [regHi] [regLo] [countHi] [countLo]`

**Response:**
`[addr] [03] [byteCount] [data...] [crcLo] [crcHi]`

Where `byteCount = 2 * count`, and total response length is:
`5 + 2*count`

### 0x06 Write Single Register
**Request (WITHOUT CRC):**
`[addr] [06] [regHi] [regLo] [valHi] [valLo]`

**Response (ACK):**
Echo of the first 6 bytes:
`[addr] [06] [regHi] [regLo] [valHi] [valLo] [crcLo] [crcHi]`

Total length: 8 bytes.

## Validating “response belongs to request” on a noisy RS485 bus
Because garbage bytes may appear before/after the real Modbus frame, never assume the response starts at index 0.

### Validation steps (must all pass)
1. **Prefix match**
   - For read (0x03): prefix is usually `[addr] [03] [expectedByteCount]`
   - For write (0x06): prefix is `[addr] [06] [regHi] [regLo] [valHi] [valLo]`
2. **Length match**
   - Slice exactly the expected frame length starting at the candidate prefix.
3. **CRC match**
   - Compute CRC16 over the slice excluding the last two bytes.
   - Compare with CRC_L/CRC_H stored in the last two bytes.

### Practical frame alignment algorithm (recommended)
Given an RX buffer `rx[]` of length `rxLen` and an expected response length `L`:
- For each offset `i` from `0` to `rxLen - L`:
  - Check prefix bytes at `rx[i...]`
  - If prefix matches, CRC-check the `L` bytes from `i`
  - If CRC passes, accept this as the extracted frame
- If no offset yields a valid frame, treat response as invalid and retry (or mark sensor offline)

## Parsing rules that bite people
- **Signed vs unsigned:** many temperature registers are signed 16-bit (two’s complement).
- **Scaling:** common scales: `/10`, `/100`, `/1000`. Use exactly what the manual says.
- **Endianness:** Modbus register words are big-endian within each 16-bit register.
  - Multi-register values (u32) are typically high-word then low-word.

## Logging requirements for real debugging
When debugging:
- Log the **raw RX buffer** (full dump)
- Log the **extracted frame** (start offset, length)
- Log **CRC status**
- Log the parsed raw register words and final scaled values
