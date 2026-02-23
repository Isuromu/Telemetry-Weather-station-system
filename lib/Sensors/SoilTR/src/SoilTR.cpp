#include "SoilTR.h"

SoilTR::SoilTR(RS485Modbus& bus, uint8_t address)
: _bus(bus), _addr(address) {}

void SoilTR::setAddress(uint8_t address) {
  _addr = address;
}

uint8_t SoilTR::address() const {
  return _addr;
}

int16_t SoilTR::toInt16(uint16_t v) {
  return (int16_t)v;
}

bool SoilTR::readTemperatureHumidity(float& tempC, float& humPct) {
  uint16_t regs[2] = {0};

  // Start 0x0012, length 2 -> humidity + temperature (per your manual example)
  if (!_bus.readHoldingRegisters(_addr, 0x0012, 2, regs, 2)) return false;

  // Humidity: 0.1 %RH
  humPct = (float)regs[0] / 10.0f;

  // Temperature: signed 0.1 C
  int16_t tRaw = toInt16(regs[1]);
  tempC = (float)tRaw / 10.0f;

  return true;
}

bool SoilTR::readConductivity(uint16_t& uScm) {
  uint16_t reg = 0;
  if (!_bus.readHoldingRegisters(_addr, 0x0015, 1, &reg, 1)) return false;
  uScm = reg;
  return true;
}

bool SoilTR::readPH(float& ph) {
  uint16_t reg = 0;
  if (!_bus.readHoldingRegisters(_addr, 0x0006, 1, &reg, 1)) return false;
  ph = (float)reg / 100.0f;
  return true;
}

bool SoilTR::readNPK(uint16_t& n, uint16_t& p, uint16_t& k) {
  uint16_t regs[3] = {0};
  if (!_bus.readHoldingRegisters(_addr, 0x001E, 3, regs, 3)) return false;
  n = regs[0];
  p = regs[1];
  k = regs[2];
  return true;
}

bool SoilTR::readAll(SoilTR_Data& out) {
  if (!readTemperatureHumidity(out.temperatureC, out.humidityPct)) return false;
  if (!readConductivity(out.conductivity_uScm)) return false;
  if (!readPH(out.pH)) return false;
  if (!readNPK(out.nitrogen_mgkg, out.phosphorus_mgkg, out.potassium_mgkg)) return false;
  return true;
}

bool SoilTR::changeAddressSpecial(uint8_t broadcastAddr, uint8_t newAddress) {
  // Pattern: [broadcastAddr] 06 02 00 00 [newAddr]
  // That is Modbus write single register:
  //   reg = 0x0200, value = 0x00xx (xx=newAddress)
  //
  // Many sensors reply with an echo; we treat it as a normal 0x06 transaction.
  return _bus.writeSingleRegister(broadcastAddr, 0x0200, (uint16_t)newAddress);
}
