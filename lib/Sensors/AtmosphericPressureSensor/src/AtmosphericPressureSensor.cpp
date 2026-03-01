#include "AtmosphericPressureSensor.h"

AtmosphericPressureSensor::AtmosphericPressureSensor(RS485Bus &bus,
                                                     uint8_t address)
    : _bus(bus), _addr(address) {}

void AtmosphericPressureSensor::setAddress(uint8_t address) { _addr = address; }
uint8_t AtmosphericPressureSensor::address() const { return _addr; }

bool AtmosphericPressureSensor::readAll(AtmosphericPressureSensor_Data &out) {
  // FC03, read 12 registers from 0x0000 → 29-byte response (3+24+2)
  // Frame: addr 0x03 0x18 [24 data bytes] [CRC lo] [CRC hi]
  uint8_t tx[8] = {_addr, 0x03, 0x00, 0x00, 0x00, 0x0C, 0, 0};
  uint16_t crc = RS485Bus::crc16Modbus(tx, 6);
  tx[6] = crc & 0xFF;
  tx[7] = (crc >> 8) & 0xFF;

  uint8_t prefix[3] = {_addr, 0x03, 0x18}; // 0x18 = 24 data bytes
  uint8_t rx[29];
  if (!_bus.transferAndExtractFixedFrame(tx, 8, prefix, 3, 29, rx, sizeof(rx),
                                         400, 25))
    return false;

  // reg[0] = bytes 3..4  → humidity
  uint16_t humRaw = ((uint16_t)rx[3] << 8) | rx[4];
  // reg[1] = bytes 5..6  → temperature
  int16_t tempRaw = (int16_t)(((uint16_t)rx[5] << 8) | rx[6]);
  // reg[2]+reg[3] = bytes 7..10 → pressure u32 /100
  // TODO: confirm exact register offset on hardware.
  // Doc states example raw 100549 → 1005.49 mbar. Candidate: regs 2+3.
  uint32_t presRaw = ((uint32_t)rx[7] << 24) | ((uint32_t)rx[8] << 16) |
                     ((uint32_t)rx[9] << 8) | (uint32_t)rx[10];

  float hum = humRaw / 10.0f;
  float temp = tempRaw / 10.0f;
  float pres = presRaw / 100.0f;

  // Hard bounds
  if (hum < 0.0f || hum > 100.0f)
    return false;
  if (temp < -40.0f || temp > 80.0f)
    return false;
  if (pres < 10.0f || pres > 1200.0f)
    return false;

  out.humidity_pct = hum;
  out.temperature_c = temp;
  out.pressure_mbar = pres;
  return true;
}
