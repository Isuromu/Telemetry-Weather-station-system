#include "WaterSuspendedSolidsSensor.h"

WaterSuspendedSolidsSensor::WaterSuspendedSolidsSensor(RS485Bus &bus,
                                                       uint8_t address)
    : _bus(bus), _addr(address) {}

void WaterSuspendedSolidsSensor::setAddress(uint8_t address) {
  _addr = address;
}
uint8_t WaterSuspendedSolidsSensor::address() const { return _addr; }

bool WaterSuspendedSolidsSensor::readAll(WaterSuspendedSolidsSensor_Data &out) {
  // --- Read suspended solids: reg 0x0002, qty 1 → 7 bytes ---
  // Scale /10 per worked example in doc (0x00BD=189 → 18.9 mg/L)
  // TODO: confirm doc table says /100 vs example implies /10. Verify on
  // hardware.
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
    float ss = raw / 10.0f;

    if (ss < 0.0f || ss > 20000.0f)
      return false;
    out.ss_mg_l = ss;
  }

  // --- Read temperature: reg 0x0001, qty 1 → 7 bytes ---
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

    if (temp < 0.0f || temp > 50.0f)
      return false;
    out.temperature_c = temp;
  }

  return true;
}

bool WaterSuspendedSolidsSensor::changeAddress(uint8_t newAddress) {
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
