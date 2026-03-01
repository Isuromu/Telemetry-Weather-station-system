#include "WaterPHSensor.h"

WaterPHSensor::WaterPHSensor(RS485Bus &bus, uint8_t address)
    : _bus(bus), _addr(address) {}

void WaterPHSensor::setAddress(uint8_t address) { _addr = address; }
uint8_t WaterPHSensor::address() const { return _addr; }

bool WaterPHSensor::readAll(WaterPHSensor_Data &out) {
  // --- Read pH: reg 0x0002, qty 1 → 7 bytes; scale /100 → pH ---
  {
    uint8_t tx[8] = {_addr, 0x03, 0x00, 0x02, 0x00, 0x01, 0, 0};
    uint16_t crc = RS485Bus::crc16Modbus(tx, 6);
    tx[6] = crc & 0xFF;
    tx[7] = (crc >> 8) & 0xFF;

    uint8_t prefix[3] = {_addr, 0x03, 0x02};
    uint8_t rx[7];
    if (!_bus.transferAndExtractFixedFrame(tx, 8, prefix, 3, 7, rx, sizeof(rx),
                                           250, 25))
      return false;

    uint16_t raw = ((uint16_t)rx[3] << 8) | rx[4];
    float ph = raw / 100.0f;

    if (ph < 0.0f || ph > 14.0f)
      return false;
    out.ph = ph;
  }

  // --- Read temperature: reg 0x0001, qty 1 → 7 bytes; scale /10 → °C ---
  {
    uint8_t tx[8] = {_addr, 0x03, 0x00, 0x01, 0x00, 0x01, 0, 0};
    uint16_t crc = RS485Bus::crc16Modbus(tx, 6);
    tx[6] = crc & 0xFF;
    tx[7] = (crc >> 8) & 0xFF;

    uint8_t prefix[3] = {_addr, 0x03, 0x02};
    uint8_t rx[7];
    if (!_bus.transferAndExtractFixedFrame(tx, 8, prefix, 3, 7, rx, sizeof(rx),
                                           250, 25))
      return false;

    int16_t tempRaw = (int16_t)(((uint16_t)rx[3] << 8) | rx[4]);
    float temp = tempRaw / 10.0f;

    // Conservative bounds per range_protection.md
    if (temp < -10.0f || temp > 60.0f)
      return false;
    out.temperature_c = temp;
  }

  return true;
}

bool WaterPHSensor::changeAddress(uint8_t newAddress) {
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
