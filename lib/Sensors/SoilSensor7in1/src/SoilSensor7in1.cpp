#include "SoilSensor7in1.h"

SoilSensor7in1::SoilSensor7in1(RS485Bus &bus, uint8_t address)
    : _bus(bus), _addr(address) {}

void SoilSensor7in1::setAddress(uint8_t address) { _addr = address; }

uint8_t SoilSensor7in1::address() const { return _addr; }

bool SoilSensor7in1::readHoldingRegisters(uint16_t startReg, uint16_t numRegs,
                                          uint8_t *outPayload) {
  // Construct request: [addr, 0x03, startHi, startLo, numHi, numLo, crcLo,
  // crcHi]
  uint8_t tx[8];
  tx[0] = _addr;
  tx[1] = 0x03;
  tx[2] = (startReg >> 8) & 0xFF;
  tx[3] = startReg & 0xFF;
  tx[4] = (numRegs >> 8) & 0xFF;
  tx[5] = numRegs & 0xFF;

  uint16_t crc = RS485Bus::crc16Modbus(tx, 6);
  tx[6] = crc & 0xFF;
  tx[7] = (crc >> 8) & 0xFF;

  // Expected response prefix: [addr, 0x03, byteCount]
  uint8_t byteCount = numRegs * 2;
  uint8_t prefix[3] = {_addr, 0x03, byteCount};

  // Expected length = 3 (prefix) + byteCount (payload) + 2 (CRC)
  size_t expectedLen = 3 + byteCount + 2;

  // Max payload is reasonable for this sensor (under 50 bytes typically, max
  // 255)
  if (expectedLen > 255)
    return false;
  uint8_t rxFrame[256];

  // Do the transaction
  bool ok =
      _bus.transferAndExtractFixedFrame(tx, 8, prefix, 3, expectedLen, rxFrame,
                                        sizeof(rxFrame), 250, 25 // timeouts
      );

  if (!ok)
    return false;

  // Copy the payload (which starts at index 3 in the Modbus frame)
  memcpy(outPayload, &rxFrame[3], byteCount);
  return true;
}

bool SoilSensor7in1::readAll(SoilSensor7in1_Data &out) {
  // We do 3 reads to avoid non-supported memory regions.

  // 1) read pH (reg 0x0006)
  uint8_t bufPH[2];
  if (!readHoldingRegisters(0x0006, 1, bufPH))
    return false;
  out.pH = (float)((uint16_t)bufPH[0] << 8 | bufPH[1]) / 100.0f;

  // 2) read Moisture, Temp, EC (reg 0x0012 to 0x0015 -> 4 regs)
  uint8_t buf4[8];
  if (!readHoldingRegisters(0x0012, 4, buf4))
    return false;
  out.humidityPct = (float)((uint16_t)buf4[0] << 8 | buf4[1]) / 10.0f;

  int16_t tRaw = (int16_t)((uint16_t)buf4[2] << 8 | buf4[3]);
  out.temperatureC = (float)tRaw / 10.0f;

  // buf4[4..5] is reg 0x0014, unused here based on spec sheet gaps
  out.conductivity_uScm = ((uint16_t)buf4[6] << 8 | buf4[7]);

  // 3) read N,P,K (reg 0x001E to 0x0020 -> 3 regs)
  uint8_t buf3[6];
  if (!readHoldingRegisters(0x001E, 3, buf3))
    return false;
  out.nitrogen_mgkg = ((uint16_t)buf3[0] << 8 | buf3[1]);
  out.phosphorus_mgkg = ((uint16_t)buf3[2] << 8 | buf3[3]);
  out.potassium_mgkg = ((uint16_t)buf3[4] << 8 | buf3[5]);

  // Range Validations per documentation (-40 to 80C, 0 to 100%)
  if (out.temperatureC < -40.0f || out.temperatureC > 80.0f)
    return false;
  if (out.humidityPct < 0.0f || out.humidityPct > 100.0f)
    return false;

  return true;
}
