#pragma once
#include "RS485Modbus.h"
#include <Arduino.h>

/*
  SolarRadiationSensor driver
  Source: RS485 Total Solar Radiation Sensor manual
  Register 0x0000: total solar radiation, FC03, u16, scale 1 → W/m²
  Range: 0–1500 W/m²
*/

struct SolarRadiationSensor_Data {
  uint16_t radiation_w_m2; // W/m²
};

class SolarRadiationSensor {
public:
  SolarRadiationSensor(RS485Bus &bus, uint8_t address);

  void setAddress(uint8_t address);
  uint8_t address() const;

  bool readAll(SolarRadiationSensor_Data &out);
  bool changeAddress(uint8_t newAddress);

private:
  RS485Bus &_bus;
  uint8_t _addr;
};
