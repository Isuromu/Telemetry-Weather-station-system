#pragma once
#include "RS485Modbus.h"
#include <Arduino.h>

/*
  WaterPHSensor driver (JXBS-3001-PH-RS, RS485)
  Register 0x0002: pH, u16 /100 (0–14 pH)
  Register 0x0001: temperature, s16 /10 °C (conservative bounds -10–60 °C)
  readAll() performs TWO FC03 reads.
*/

struct WaterPHSensor_Data {
  float ph;            // pH (0–14)
  float temperature_c; // °C
};

class WaterPHSensor {
public:
  WaterPHSensor(RS485Bus &bus, uint8_t address);

  void setAddress(uint8_t address);
  uint8_t address() const;

  bool readAll(WaterPHSensor_Data &out);
  bool changeAddress(uint8_t newAddress);

private:
  RS485Bus &_bus;
  uint8_t _addr;
};
