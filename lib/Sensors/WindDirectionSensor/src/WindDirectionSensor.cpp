#include "WindDirectionSensor.h"

WindDirectionSensor::WindDirectionSensor(RS485Bus &bus, uint8_t address)
    : _bus(bus), _addr(address) {}

void WindDirectionSensor::setAddress(uint8_t address) { _addr = address; }
uint8_t WindDirectionSensor::address() const { return _addr; }

bool WindDirectionSensor::readAll(WindDirectionSensor_Data &out) {
  // FC03, reg 0x0000, qty 1 → 7-byte response
  uint8_t tx[8] = {_addr, 0x03, 0x00, 0x00, 0x00, 0x01, 0, 0};
  uint16_t crc = RS485Bus::crc16Modbus(tx, 6);
  tx[6] = crc & 0xFF;
  tx[7] = (crc >> 8) & 0xFF;

  uint8_t prefix[3] = {_addr, 0x03, 0x02};
  uint8_t rx[7];
  if (!_bus.transferAndExtractFixedFrame(tx, 8, prefix, 3, 7, rx, sizeof(rx),
                                         250, 25))
    return false;

  uint16_t raw = ((uint16_t)rx[3] << 8) | rx[4];

  // Hard bounds: 0–360 degrees
  if (raw > 360)
    return false;

  out.direction_deg = raw;
  return true;
}

bool WindDirectionSensor::changeAddress(uint8_t newAddress) {
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
