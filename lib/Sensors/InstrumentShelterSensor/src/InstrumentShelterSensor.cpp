#include "InstrumentShelterSensor.h"

InstrumentShelterSensor::InstrumentShelterSensor(RS485Bus &bus, uint8_t address)
    : _bus(bus), _addr(address) {}

void InstrumentShelterSensor::setAddress(uint8_t address) { _addr = address; }
uint8_t InstrumentShelterSensor::address() const { return _addr; }

bool InstrumentShelterSensor::readAll(InstrumentShelterSensor_Data &out) {
  // --- Read T+H: regs 0x0000–0x0001, qty 2 → 9 bytes ---
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

  // --- Read Illuminance: regs 0x0007–0x0008, qty 2 → 9 bytes ---
  {
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

    if (lux > 300000UL)
      return false; // >300k Lux is suspicious

    out.lux = lux;
  }

  return true;
}
