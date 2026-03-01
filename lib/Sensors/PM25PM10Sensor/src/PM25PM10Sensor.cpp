#include "PM25PM10Sensor.h"

PM25PM10Sensor::PM25PM10Sensor(RS485Bus &bus, uint8_t address)
    : _bus(bus), _addr(address) {}

void PM25PM10Sensor::setAddress(uint8_t address) { _addr = address; }
uint8_t PM25PM10Sensor::address() const { return _addr; }

bool PM25PM10Sensor::readAll(PM25PM10Sensor_Data &out) {
  // --- Read PM2.5: reg 0x0004, qty 1 → 7 bytes ---
  {
    uint8_t tx[8] = {_addr, 0x03, 0x00, 0x04, 0x00, 0x01, 0, 0};
    uint16_t crc = RS485Bus::crc16Modbus(tx, 6);
    tx[6] = crc & 0xFF;
    tx[7] = (crc >> 8) & 0xFF;

    uint8_t prefix[3] = {_addr, 0x03, 0x02};
    uint8_t rx[7];
    if (!_bus.transferAndExtractFixedFrame(tx, 8, prefix, 3, 7, rx, sizeof(rx),
                                           250, 25))
      return false;

    uint16_t raw = ((uint16_t)rx[3] << 8) | rx[4];
    if (raw > 300)
      return false; // Hard bound: 0–300 µg/m³
    out.pm25_ug_m3 = raw;
  }

  // --- Read PM10: reg 0x0009, qty 1 → 7 bytes ---
  {
    uint8_t tx[8] = {_addr, 0x03, 0x00, 0x09, 0x00, 0x01, 0, 0};
    uint16_t crc = RS485Bus::crc16Modbus(tx, 6);
    tx[6] = crc & 0xFF;
    tx[7] = (crc >> 8) & 0xFF;

    uint8_t prefix[3] = {_addr, 0x03, 0x02};
    uint8_t rx[7];
    if (!_bus.transferAndExtractFixedFrame(tx, 8, prefix, 3, 7, rx, sizeof(rx),
                                           250, 25))
      return false;

    uint16_t raw = ((uint16_t)rx[3] << 8) | rx[4];
    if (raw > 300)
      return false;
    out.pm10_ug_m3 = raw;
  }

  return true;
}

bool PM25PM10Sensor::changeAddress(uint8_t newAddress) {
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
