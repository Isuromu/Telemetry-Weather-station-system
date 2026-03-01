#pragma once
#include "RS485Modbus.h"
#include <Arduino.h>

/*
  RainSensor driver (RS485 Optical Rain Sensor)
  Register 0x0003: rainfall accumulation, u16, scale /10 → mm (0.1 mm
  resolution) Register 0x0105: write 0x0000 to clear accumulated rainfall
*/

struct RainSensor_Data {
  float rainfall_mm; // mm, cumulative since last clear
};

class RainSensor {
public:
  RainSensor(RS485Bus &bus, uint8_t address);

  void setAddress(uint8_t address);
  uint8_t address() const;

  bool readAll(RainSensor_Data &out);
  bool clearRainfall(); // Resets accumulated rainfall to zero

private:
  RS485Bus &_bus;
  uint8_t _addr;
};
