#pragma once
#include "RS485Modbus.h"
#include <Arduino.h>


/*
  SoilSensor7in1 driver (JXBS-3001-TR 485 "comprehensive soil sensor")
  Reads Temperature, Humidity, Conductivity, pH, N, P, K.

  Uses transport-only RS485Bus:
  - Driver constructs the exact Modbus request bytes.
  - Driver calls transferAndExtractFixedFrame().
  - Driver parses the raw byte response.
*/

struct SoilSensor7in1_Data {
  float temperatureC;
  float humidityPct;
  uint16_t conductivity_uScm;
  float pH;
  uint16_t nitrogen_mgkg;
  uint16_t phosphorus_mgkg;
  uint16_t potassium_mgkg;
};

class SoilSensor7in1 {
public:
  SoilSensor7in1(RS485Bus &bus, uint8_t address);

  void setAddress(uint8_t address);
  uint8_t address() const;

  // Reads all 7 parameters using a single Modbus read (from 0x0006 for pH, etc.
  // or specific blocks) Actually, the parameters are spread out: 0x0006: pH
  // 0x0012: moisture, 0x0013: temp, 0x0014: ?, 0x0015: cond
  // 0x001E: N, 0x001F: P, 0x0020: K
  // We can read them in 3 blocks, or read 0x0006 through 0x0020 (27 registers)
  // at once. We'll provide a single readAll() method that queries the required
  // blocks.

  bool readAll(SoilSensor7in1_Data &out);

private:
  RS485Bus &_bus;
  uint8_t _addr;

  bool readHoldingRegisters(uint16_t startReg, uint16_t numRegs,
                            uint8_t *outPayload);
};
