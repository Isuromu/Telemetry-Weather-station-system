#include "IlluminanceSensor.h"

IlluminanceSensor::IlluminanceSensor(RS485Bus &bus, uint8_t address)
    : _bus(bus), _addr(address) {}

void IlluminanceSensor::setAddress(uint8_t address) { _addr = address; }
uint8_t IlluminanceSensor::address() const { return _addr; }

bool IlluminanceSensor::readAll(IlluminanceSensor_Data &out) {
  // FC03, regs 0x0007–0x0008, qty 2 → 9-byte response (u32 as two u16 words)
  uint8_t tx[8] = {_addr, 0x03, 0x00, 0x07, 0x00, 0x02, 0, 0};
  uint16_t crc = RS485Bus::crc16Modbus(tx, 6);
  tx[6] = crc & 0xFF;
  tx[7] = (crc >> 8) & 0xFF;

  uint8_t prefix[3] = {_addr, 0x03, 0x04};
  uint8_t rx[9];
  if (!_bus.transferAndExtractFixedFrame(tx, 8, prefix, 3, 9, rx, sizeof(rx),
                                         250, 25))
    return false;

  uint16_t hi = ((uint16_t)rx[3] << 8) | rx[4];
  uint16_t lo = ((uint16_t)rx[5] << 8) | rx[6];
  uint32_t lux = ((uint32_t)hi << 16) | (uint32_t)lo;

  // Hard bounds: 0–200000 Lux (>300000 treated as fault per
  // range_protection.md)
  if (lux > 300000UL)
    return false;

  out.lux = lux;
  return true;
}

bool IlluminanceSensor::changeAddress(uint8_t newAddress) {
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
