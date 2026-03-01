#pragma once
#include "RS485Modbus.h"
#include <Arduino.h>

/*
  OzoneSensor driver (JXBS-3001-O3, RS485)
  Register 0x0006: O3 concentration u16 /100 → ppm  (note: /100, not /10)
  Registers 0x0000, 0x0001: humidity + temperature
  Configurable range max: O3_MAX_PPM (set in config.h)
*/

#ifndef O3_MAX_PPM
#define O3_MAX_PPM 100.0f
#endif

struct OzoneSensor_Data {
  float ozone_ppm;     // ppm
  float humidity_pct;  // %RH
  float temperature_c; // °C
};

class OzoneSensor {
public:
  OzoneSensor(RS485Bus &bus, uint8_t address);

  void setAddress(uint8_t address);
  uint8_t address() const;

  bool readAll(OzoneSensor_Data &out);
  bool changeAddress(uint8_t newAddress);

private:
  RS485Bus &_bus;
  uint8_t _addr;
};
