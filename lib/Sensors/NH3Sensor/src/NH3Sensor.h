#pragma once
#include "RS485Modbus.h"
#include <Arduino.h>

/*
  NH3Sensor driver (JXBS-3001-NH3, RS485)
  Register 0x0006: NH3 concentration u16 /10 → ppm
  Registers 0x0000, 0x0001: humidity + temperature (same family)
  Configurable range max: NH3_MAX_PPM (set in config.h or here)
*/

#ifndef NH3_MAX_PPM
#define NH3_MAX_PPM 5000.0f
#endif

struct NH3Sensor_Data {
  float nh3_ppm;       // ppm
  float humidity_pct;  // %RH
  float temperature_c; // °C
};

class NH3Sensor {
public:
  NH3Sensor(RS485Bus &bus, uint8_t address);

  void setAddress(uint8_t address);
  uint8_t address() const;

  bool readAll(NH3Sensor_Data &out);
  bool changeAddress(uint8_t newAddress);

private:
  RS485Bus &_bus;
  uint8_t _addr;
};
