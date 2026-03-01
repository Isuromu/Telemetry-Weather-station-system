# Leaf Sensor Temperature Diagnosis Report

Based on the protocol spec (`JXBS-3001-YMSD Leaf Surface Humidity Transmitter`), here is the breakdown of the protocol and a checklist to diagnose the `-145` reading bug.

## 1. Register Information
*   **Humidity:** 
    *   **Register:** `0x0020`
    *   **Data Type:** 16-bit unsigned integer (uint16)
    *   **Scaling:** Divide raw value by `10.0`
    *   **Units:** `%RH`
*   **Temperature:** 
    *   **Register:** `0x0021`
    *   **Data Type:** 16-bit signed integer (int16), matching two's complement for negative values.
    *   **Scaling:** Divide raw value by `10.0`
    *   **Units:** `°C`

## 2. Request & Response Structure

### Exact Request Bytes (Reading Humidity + Temperature)
To read both registers (0x0020 and 0x0021) in one request for device ID `0x01` without CRC, you send:
`01 03 00 20 00 02`

*(Note: In your C++ driver using RS485Bus, you will append the CRC bytes to this 6-byte payload before sending).*

### Expected Response
*   **Length:** `9 bytes` (1 byte Addr + 1 byte Func + 1 byte ByteCount + 2 bytes Humidity + 2 bytes Temperature + 2 bytes CRC)
*   **Required Prefix:** `01 03 04` (Address 01, Function 03, ByteCount 04).

### Response Layout
*   `[0]` Addr = 0x01
*   `[1]` Func = 0x03
*   `[2]` ByteCount = 0x04
*   `[3]` Hum High Byte
*   `[4]` Hum Low Byte
*   `[5]` Temp High Byte
*   `[6]` Temp Low Byte
*   `[7]` CRC Low
*   `[8]` CRC High

---

## 3. Checklist for Diagnosing the "-145" Bug

If the temperature is reporting exactly as `-145°C` or `-14.5°C`, here are the likely culprits:

### ❌ 1. Wrong Scaling 
If the actual temperature is `-14.5°C`, but your code is missing the division by `10.0`, the driver will report `-145` directly. Ensure you explicitly cast to `float` and divide by `10.0f` after resolving the signed 16-bit math.

### ❌ 2. Unsigned to Signed Overlap (Math Logic)
A raw byte structure reading `-14.5°C` naturally looks like this in Hexadecimal: 
*   `-14.5` * 10 = `-145`
*   `-145` in 16-bit two's complement: `0xFF6F`
If you read `0xFF` and `0x6F` but process them entirely as unsigned integers, it becomes `65391`. If you then subtract something blindly or cast it wrong, you get bizarre output. *It must be cast tightly to `int16_t`*.

### ❌ 3. Swapped Endianness
If the temperature was actually a positive value, but the bytes were interpreted backwards:
Suppose the temperature was meant to be `28.6°C` (raw `x011E`, bytes `01 1E`). 
If the low and high bytes are swapped in code (e.g. `(Low << 8) | High`), you compute `0x1E01`.
`0x1E01` as signed int16 is `7681`. Divided by 10 = `768.1°C`.
This doesn't land natively on `-145`, but a similar misalignment can cause wildly incorrect negative interpretations. 

### ❌ 4. Frame Misalignment (Garbage Bytes)
If noise inserts a garbage byte before the real frame starts, and your code simply grabs fixed indexes without extracting via `RS485Bus::extractFixedFrameByPrefix()`, your indexes will shift.
For example, if the real frame is `01 03 04 02 92 FF 6F ...`
But you capture `[GARBAGE] 01 03 04 02 92 FF 6F...`
If you read indexes `[5]` and `[6]` assuming they are temperature, you are actually reading `0x92` and `0xFF` instead of `0xFF` and `0x6F`.
*   `0x92FF` as int16_t is `-27905`. 
*   Divided by 10.0 = `-2790.5°C`.

## 4. Recommended C++ Parsing Code Snippet

Here is the safest way to guarantee proper extraction for the Leaf Sensor within your established RS485Modbus framework:

```cpp
bool readLeafSensor(RS485Bus& bus, uint8_t addr, float& humidity, float& temperature) {
  // 1. Build request: [addr, 0x03, 0x00, 0x20, 0x00, 0x02, CRCL, CRCH]
  uint8_t tx[8] = { addr, 0x03, 0x00, 0x20, 0x00, 0x02, 0, 0 };
  uint16_t crcReq = RS485Bus::crc16Modbus(tx, 6);
  tx[6] = crcReq & 0xFF;
  tx[7] = (crcReq >> 8) & 0xFF;

  // 2. Define expected validation bounds
  uint8_t prefix[3] = { addr, 0x03, 0x04 }; // Expect 4 bytes of data
  uint8_t rxFrame[9];                       // 3 (prefix) + 4 (data) + 2 (CRC) = 9
  
  // 3. Dispatch and extract safely
  bool ok = bus.transferAndExtractFixedFrame(
      tx, 8, 
      prefix, 3, 
      9, 
      rxFrame, sizeof(rxFrame), 
      250, 25
  );

  if (!ok) return false;

  // 4. Parse Humidity (uint16_t) - Bytes 3 and 4
  uint16_t rawHum = ((uint16_t)rxFrame[3] << 8) | rxFrame[4];
  humidity = (float)rawHum / 10.0f;

  // 5. Parse Temperature (int16_t) - Bytes 5 and 6
  // CRITICAL: Explicitly cast to signed 16-bit integer!
  int16_t rawTemp = (int16_t)(((uint16_t)rxFrame[5] << 8) | rxFrame[6]);
  temperature = (float)rawTemp / 10.0f;

  return true;
}
```
