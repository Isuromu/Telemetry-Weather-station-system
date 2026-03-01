#include "RainSensor.h"

RainSensor::RainSensor(RS485Bus &bus, uint8_t address)
    : _bus(bus), _addr(address) {}

void RainSensor::setAddress(uint8_t address) { _addr = address; }
uint8_t RainSensor::address() const { return _addr; }

bool RainSensor::readAll(RainSensor_Data &out) {
  // FC03, reg 0x0003, qty 1 → 7-byte response
  uint8_t tx[8] = {_addr, 0x03, 0x00, 0x03, 0x00, 0x01, 0, 0};
  uint16_t crc = RS485Bus::crc16Modbus(tx, 6);
  tx[6] = crc & 0xFF;
  tx[7] = (crc >> 8) & 0xFF;

  uint8_t prefix[3] = {_addr, 0x03, 0x02};
  uint8_t rx[7];
  if (!_bus.transferAndExtractFixedFrame(tx, 8, prefix, 3, 7, rx, sizeof(rx),
                                         250, 25))
    return false;

  uint16_t raw = ((uint16_t)rx[3] << 8) | rx[4];
  // Fault patterns
  if (raw == 0xFFFF || raw == 0x7FFF)
    return false;

  out.rainfall_mm = raw / 10.0f;
  return true;
}

bool RainSensor::clearRainfall() {
  // FC06, write 0x0000 to register 0x0105
  uint8_t tx[8] = {_addr, 0x06, 0x01, 0x05, 0x00, 0x00, 0, 0};
  uint16_t crc = RS485Bus::crc16Modbus(tx, 6);
  tx[6] = crc & 0xFF;
  tx[7] = (crc >> 8) & 0xFF;

  uint8_t prefix[4] = {_addr, 0x06, 0x01, 0x05};
  uint8_t rx[8];
  return _bus.transferAndExtractFixedFrame(tx, 8, prefix, 4, 8, rx, sizeof(rx),
                                           500, 25);
}
