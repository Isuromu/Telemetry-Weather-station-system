#include "TVOCSensor.h"

TVOCSensor::TVOCSensor(RS485Bus &bus, uint8_t address)
    : _bus(bus), _addr(address) {}

void TVOCSensor::setAddress(uint8_t address) { _addr = address; }
uint8_t TVOCSensor::address() const { return _addr; }

bool TVOCSensor::readU16(uint16_t reg, uint16_t &val) {
  uint8_t tx[8] = {
      _addr, 0x03, (uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF), 0x00, 0x01,
      0,     0};
  uint16_t crc = RS485Bus::crc16Modbus(tx, 6);
  tx[6] = crc & 0xFF;
  tx[7] = (crc >> 8) & 0xFF;

  uint8_t prefix[3] = {_addr, 0x03, 0x02};
  uint8_t rx[7];
  if (!_bus.transferAndExtractFixedFrame(tx, 8, prefix, 3, 7, rx, sizeof(rx),
                                         250, 25))
    return false;

  val = ((uint16_t)rx[3] << 8) | rx[4];
  return true;
}

bool TVOCSensor::readS16(uint16_t reg, int16_t &val) {
  uint16_t u;
  if (!readU16(reg, u))
    return false;
  val = (int16_t)u;
  return true;
}

bool TVOCSensor::readAll(TVOCSensor_Data &out) {
  uint16_t humRaw, pm25Raw, co2Raw, tvocRaw, pm10Raw;
  int16_t tempRaw;

  // Six separate single-register reads (registers are non-contiguous)
  if (!readU16(0x0000, humRaw))
    return false;
  if (!readS16(0x0001, tempRaw))
    return false;
  if (!readU16(0x0004, pm25Raw))
    return false;
  if (!readU16(0x0005, co2Raw))
    return false;
  if (!readU16(0x0006, tvocRaw))
    return false;
  if (!readU16(0x0009, pm10Raw))
    return false;

  float hum = humRaw / 10.0f;
  float temp = tempRaw / 10.0f;

  // Hard bounds per range_protection.md
  if (hum < 0.0f || hum > 100.0f)
    return false;
  if (temp < -40.0f || temp > 80.0f)
    return false;
  if (co2Raw < 400 || co2Raw > 5000)
    return false;
  if (tvocRaw > 1000)
    return false;
  if (pm25Raw > 300)
    return false;
  if (pm10Raw > 300)
    return false;

  out.humidity_pct = hum;
  out.temperature_c = temp;
  out.pm25_ug_m3 = pm25Raw;
  out.co2_ppm = co2Raw;
  out.tvoc_ppb = tvocRaw;
  out.pm10_ug_m3 = pm10Raw;
  return true;
}

bool TVOCSensor::changeAddress(uint8_t newAddress) {
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
