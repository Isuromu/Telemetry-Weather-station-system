#include "LeafSensor.h"

LeafSensor::LeafSensor(RS485Bus &bus, uint8_t address)
    : _bus(bus), _addr(address) {}

void LeafSensor::setAddress(uint8_t address) { _addr = address; }

uint8_t LeafSensor::address() const { return _addr; }

bool LeafSensor::readAll(LeafSensor_Data &out) {
  // 1. Build request: [addr, 0x03, 0x00, 0x20, 0x00, 0x02] + CRC
  uint8_t tx[8] = {_addr, 0x03, 0x00, 0x20, 0x00, 0x02, 0, 0};
  uint16_t crcReq = RS485Bus::crc16Modbus(tx, 6);
  tx[6] = crcReq & 0xFF;
  tx[7] = (crcReq >> 8) & 0xFF;

  // 2. Define expected validation bounds
  uint8_t prefix[3] = {_addr, 0x03, 0x04}; // Expect 4 bytes of data
  uint8_t rxFrame[9]; // 3 (prefix) + 4 (data) + 2 (CRC) = 9

  // 3. Dispatch and extract safely
  bool ok = _bus.transferAndExtractFixedFrame(tx, 8, prefix, 3, 9, rxFrame,
                                              sizeof(rxFrame), 250, 25);

  if (!ok)
    return false;

  // 4. Parse Humidity (uint16_t) - Bytes 3 and 4
  uint16_t rawHum = ((uint16_t)rxFrame[3] << 8) | rxFrame[4];
  float hum = (float)rawHum / 10.0f;

  // 5. Parse Temperature (int16_t) - Bytes 5 and 6
  // CRITICAL: Explicitly cast to signed 16-bit integer!
  int16_t rawTemp = (int16_t)(((uint16_t)rxFrame[5] << 8) | rxFrame[6]);
  float temp = (float)rawTemp / 10.0f;

  // 6. Range Validations
  if (temp < -20.0f || temp > 80.0f)
    return false;
  if (hum < 0.0f || hum > 100.0f)
    return false;

  out.humidityPct = hum;
  out.temperatureC = temp;

  return true;
}
