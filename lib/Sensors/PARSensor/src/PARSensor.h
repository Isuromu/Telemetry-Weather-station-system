#pragma once
#include "RS485Modbus.h"
#include <Arduino.h>

/*
  PARSensor driver
  Source: RS485 PAR Sensor manual
  Register 0x0006: PAR value, FC03, u16, scale 1 → W/m²
  Range: 0–2000 W/m²
*/

struct PARSensor_Data {
  uint16_t par_w_m2; // W/m²
};

class PARSensor {
public:
  PARSensor(RS485Bus &bus, uint8_t address);

  void setAddress(uint8_t address);
  uint8_t address() const;

  bool readAll(PARSensor_Data &out);
  bool changeAddress(uint8_t newAddress);

private:
  RS485Bus &_bus;
  uint8_t _addr;
};
