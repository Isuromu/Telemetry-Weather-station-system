#include "EvaporationSensor.h"

EvaporationSensor::EvaporationSensor(RS485Bus &bus, uint8_t address)
    : _bus(bus), _addr(address) {}

void EvaporationSensor::setAddress(uint8_t address) { _addr = address; }
uint8_t EvaporationSensor::address() const { return _addr; }

bool EvaporationSensor::readAll(EvaporationSensor_Data &out) {
  // FC03, reg 0x0006, qty 1 → 7-byte response
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

  // Fault pattern: 0xFFFF = disconnected/error placeholder
  if (raw == 0xFFFF)
    return false;

  // Hard bounds: 0–200 (mm interpretation per range_protection.md)
  if (raw > 200)
    return false;

  out.evaporation_raw = raw;
  return true;
}

bool EvaporationSensor::tare() {
  // Write 0x0001 to register 0x0102 to reset the tare baseline
  uint8_t tx[8] = {_addr, 0x06, 0x01, 0x02, 0x00, 0x01, 0, 0};
  uint16_t crc = RS485Bus::crc16Modbus(tx, 6);
  tx[6] = crc & 0xFF;
  tx[7] = (crc >> 8) & 0xFF;

  uint8_t prefix[4] = {_addr, 0x06, 0x01, 0x02};
  uint8_t rx[8];
  return _bus.transferAndExtractFixedFrame(tx, 8, prefix, 4, 8, rx, sizeof(rx),
                                           500, 25);
}

bool EvaporationSensor::changeAddress(uint8_t newAddress) {
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
