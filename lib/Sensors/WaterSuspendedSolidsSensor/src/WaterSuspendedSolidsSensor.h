#pragma once
#include "RS485Modbus.h"
#include <Arduino.h>

/*
  WaterSuspendedSolidsSensor driver (JXSZ-1001-SS, RS485)
  Register 0x0002: suspended solids u16 /10 → mg/L (per worked example in doc)
    Note: doc text says /100 but example maps 189 → 18.9 (implies /10). Confirm
  on hardware. Register 0x0001: temperature s16 /10 → °C (0–50 °C) readAll()
  performs TWO FC03 reads.
*/

struct WaterSuspendedSolidsSensor_Data {
  float ss_mg_l;       // mg/L
  float temperature_c; // °C
};

class WaterSuspendedSolidsSensor {
public:
  WaterSuspendedSolidsSensor(RS485Bus &bus, uint8_t address);

  void setAddress(uint8_t address);
  uint8_t address() const;

  bool readAll(WaterSuspendedSolidsSensor_Data &out);
  bool changeAddress(uint8_t newAddress);

private:
  RS485Bus &_bus;
  uint8_t _addr;
};
