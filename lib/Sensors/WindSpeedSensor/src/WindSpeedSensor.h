#pragma once
#include "RS485Modbus.h"
#include <Arduino.h>

/*
  WindSpeedSensor driver
  Source: RS485 Wind Speed Sensor manual
  Register 0x0016: wind speed, FC03, u16, scale /10 → m/s
  Range: 0–30 m/s
*/

struct WindSpeedSensor_Data {
  float windSpeed_ms; // m/s
};

class WindSpeedSensor {
public:
  WindSpeedSensor(RS485Bus &bus, uint8_t address);

  void setAddress(uint8_t address);
  uint8_t address() const;

  bool readAll(WindSpeedSensor_Data &out);
  bool changeAddress(uint8_t newAddress);

private:
  RS485Bus &_bus;
  uint8_t _addr;
};
