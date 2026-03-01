#pragma once
#include "RS485Modbus.h"
#include <Arduino.h>

/*
  WindDirectionSensor driver
  Source: RS485 Wind Direction Sensor manual
  Register 0x0000: wind direction, FC03, u16, scale 1 → degrees
  Range: 0–360°
*/

struct WindDirectionSensor_Data {
  uint16_t direction_deg; // degrees, 0–360
};

class WindDirectionSensor {
public:
  WindDirectionSensor(RS485Bus &bus, uint8_t address);

  void setAddress(uint8_t address);
  uint8_t address() const;

  bool readAll(WindDirectionSensor_Data &out);
  bool changeAddress(uint8_t newAddress);

private:
  RS485Bus &_bus;
  uint8_t _addr;
};
