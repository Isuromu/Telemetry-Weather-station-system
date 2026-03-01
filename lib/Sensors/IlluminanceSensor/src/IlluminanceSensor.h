#pragma once
#include "RS485Modbus.h"
#include <Arduino.h>

/*
  IlluminanceSensor driver
  Source: RS485 Illuminance Sensor manual
  Registers 0x0007 (hi) + 0x0008 (lo): illuminance as 32-bit u32 = (hi<<16)|lo,
  Lux Range: 0–200000 Lux
*/

struct IlluminanceSensor_Data {
  uint32_t lux;
};

class IlluminanceSensor {
public:
  IlluminanceSensor(RS485Bus &bus, uint8_t address);

  void setAddress(uint8_t address);
  uint8_t address() const;

  bool readAll(IlluminanceSensor_Data &out);
  bool changeAddress(uint8_t newAddress);

private:
  RS485Bus &_bus;
  uint8_t _addr;
};
