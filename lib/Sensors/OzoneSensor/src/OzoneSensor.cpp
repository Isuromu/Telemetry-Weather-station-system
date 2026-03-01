#include "OzoneSensor.h"

OzoneSensor::OzoneSensor(RS485Bus &bus, uint8_t address)
    : _bus(bus), _addr(address) {}

void OzoneSensor::setAddress(uint8_t address) { _addr = address; }
uint8_t OzoneSensor::address() const { return _addr; }

bool OzoneSensor::readAll(OzoneSensor_Data &out) {
  // --- Read O3: reg 0x0006, qty 1 → 7 bytes; scale /100 → ppm ---
  {
    uint8_t tx[8] = {_addr, 0x03, 0x00, 0x06, 0x00, 0x01, 0, 0};
    uint16_t crc = RS485Bus::crc16Modbus(tx, 6);
    tx[6] = crc & 0xFF;
    tx[7] = (crc >> 8) & 0xFF;

    uint8_t prefix[3] = {_addr, 0x03, 0x02};
    uint8_t rx[7];
    if (!_bus.transferAndExtractFixedFrame(tx, 8, prefix, 3, 7, rx, sizeof(rx),
                                           250, 25))
      return false;

    uint16_t raw = ((uint16_t)rx[3] << 8) | rx[4];
    float ozone = raw / 100.0f; // NOTE: /100 unlike NH3/CO which use /10
    if (ozone < 0.0f || ozone > O3_MAX_PPM)
      return false;
    out.ozone_ppm = ozone;
  }

  // --- Read T+H: regs 0x0000–0x0001 → 9 bytes ---
  {
    uint8_t tx[8] = {_addr, 0x03, 0x00, 0x00, 0x00, 0x02, 0, 0};
    uint16_t crc = RS485Bus::crc16Modbus(tx, 6);
    tx[6] = crc & 0xFF;
    tx[7] = (crc >> 8) & 0xFF;

    uint8_t prefix[3] = {_addr, 0x03, 0x04};
    uint8_t rx[9];
    if (!_bus.transferAndExtractFixedFrame(tx, 8, prefix, 3, 9, rx, sizeof(rx),
                                           250, 25))
      return false;

    uint16_t humRaw = ((uint16_t)rx[3] << 8) | rx[4];
    int16_t tempRaw = (int16_t)(((uint16_t)rx[5] << 8) | rx[6]);
    float hum = humRaw / 10.0f;
    float temp = tempRaw / 10.0f;
    if (hum < 0.0f || hum > 100.0f)
      return false;
    if (temp < -40.0f || temp > 80.0f)
      return false;
    out.humidity_pct = hum;
    out.temperature_c = temp;
  }

  return true;
}

bool OzoneSensor::changeAddress(uint8_t newAddress) {
  uint8_t tx[8] = {_addr, 0x06, 0x01, 0x00, 0x00, newAddress, 0, 0};
  uint16_t crc = RS485Bus::crc16Modbus(tx, 6);
  tx[6] = crc & 0xFF;
  tx[7] = (crc >> 8) & 0xFF;

  uint8_t prefix[2] = {_addr, 0x06};
  uint8_t rx[8];
  if (!_bus.transferAndExtractFixedFrame(tx, 8, prefix, 2, 8, rx, sizeof(rx),
                                         500, 25))
    return false;

  _addr = newAddress;
  return true;
}
