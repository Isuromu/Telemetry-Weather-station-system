#pragma once
#include "RS485Modbus.h"
#include <Arduino.h>

/*
  COSensor driver (JXBS-3001-CO, RS485)
  Register 0x0006: CO concentration u16 /10 → ppm
  Configurable range max: CO_MAX_PPM (set in config.h or here)
*/

#ifndef CO_MAX_PPM
#define CO_MAX_PPM 2000.0f
#endif

struct COSensor_Data {
  float co_ppm; // ppm
};

class COSensor {
public:
  COSensor(RS485Bus &bus, uint8_t address);

  void setAddress(uint8_t address);
  uint8_t address() const;

  bool readAll(COSensor_Data &out);
  bool changeAddress(uint8_t newAddress);

private:
  RS485Bus &_bus;
  uint8_t _addr;
};
