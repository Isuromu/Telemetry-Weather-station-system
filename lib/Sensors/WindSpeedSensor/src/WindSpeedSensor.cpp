#include "WindSpeedSensor.h"

WindSpeedSensor::WindSpeedSensor(RS485Bus &bus, uint8_t address)
    : _bus(bus), _addr(address) {}

void WindSpeedSensor::setAddress(uint8_t address) { _addr = address; }
uint8_t WindSpeedSensor::address() const { return _addr; }

bool WindSpeedSensor::readAll(WindSpeedSensor_Data &out) {
  // FC03, reg 0x0016, qty 1 → 7-byte response
  uint8_t tx[8] = {_addr, 0x03, 0x00, 0x16, 0x00, 0x01, 0, 0};
  uint16_t crc = RS485Bus::crc16Modbus(tx, 6);
  tx[6] = crc & 0xFF;
  tx[7] = (crc >> 8) & 0xFF;

  uint8_t prefix[3] = {_addr, 0x03, 0x02};
  uint8_t rx[7];
  if (!_bus.transferAndExtractFixedFrame(tx, 8, prefix, 3, 7, rx, sizeof(rx),
                                         250, 25))
    return false;

  uint16_t raw = ((uint16_t)rx[3] << 8) | rx[4];

  // Hard bounds: 0–30 m/s (raw 0–300 in 0.1 m/s units)
  if (raw > 300)
    return false;

  out.windSpeed_ms = raw / 10.0f;
  return true;
}

bool WindSpeedSensor::changeAddress(uint8_t newAddress) {
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
