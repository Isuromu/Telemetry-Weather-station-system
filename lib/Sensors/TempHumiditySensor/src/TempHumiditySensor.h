#pragma once
#include "RS485Modbus.h"
#include <Arduino.h>

/*
  TempHumiditySensor driver (JXCT family)
  Source: RS485 Temperature & Humidity Sensor manual
  Register 0x0000: humidity u16 /10 → %RH  (0–100 %RH)
  Register 0x0001: temperature s16 /10 → °C (-40–80 °C)
*/

struct TempHumiditySensor_Data {
  float humidity_pct;  // %RH
  float temperature_c; // °C
};

class TempHumiditySensor {
public:
  TempHumiditySensor(RS485Bus &bus, uint8_t address);

  void setAddress(uint8_t address);
  uint8_t address() const;

  bool readAll(TempHumiditySensor_Data &out);
  bool changeAddress(uint8_t newAddress);

private:
  RS485Bus &_bus;
  uint8_t _addr;
};
