#pragma once
#include "RS485Modbus.h"
#include <Arduino.h>

/*
  EvaporationSensor driver (JXCT family)
  Source: RS485 Evaporation Sensor manual
  Register 0x0006: evaporation / water-weight value (u16, units confirmed on
  hardware) Range bounds: 0–200 (treated as mm per range_protection.md) Register
  0x0102: tare / clear command (write 0x0001)
*/

struct EvaporationSensor_Data {
  uint16_t evaporation_raw; // raw reading; confirm units with vendor tool
                            // (likely mm or g)
};

class EvaporationSensor {
public:
  EvaporationSensor(RS485Bus &bus, uint8_t address);

  void setAddress(uint8_t address);
  uint8_t address() const;

  bool readAll(EvaporationSensor_Data &out);
  bool tare(); // Resets (tares) the evaporation baseline
  bool changeAddress(uint8_t newAddress);

private:
  RS485Bus &_bus;
  uint8_t _addr;
};
