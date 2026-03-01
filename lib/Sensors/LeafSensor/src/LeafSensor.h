#pragma once
#include "RS485Modbus.h"
#include <Arduino.h>


/*
  LeafSensor driver (JXBS-3001-YMSD 485 "Leaf Moisture Temperature")
  Reads Temperature and Humidity from the leaf surface.

  Registers used:
    - 0x0020: Humidity (uint16, 0.1 %RH, range 0-100%)
    - 0x0021: Temperature (int16, 0.1 °C, range -20 to 80°C)
*/

struct LeafSensor_Data {
  float temperatureC;
  float humidityPct;
};

class LeafSensor {
public:
  LeafSensor(RS485Bus &bus, uint8_t address);

  void setAddress(uint8_t address);
  uint8_t address() const;

  // Reads Humidity and Temperature
  // Returns true if valid frame is received AND values fall within the
  // documented protocol range.
  bool readAll(LeafSensor_Data &out);

private:
  RS485Bus &_bus;
  uint8_t _addr;
};
