#pragma once
#include "RS485Modbus.h"
#include <Arduino.h>

/*
  UVSensor driver
  Source: RS485 UV Rays Sensor manual
  Register 0x0008: UV intensity u16 /10 → W/m² (0–150 W/m²)
  Register 0x0000: humidity u16 /10 → %RH
  Register 0x0001: temperature s16 /10 → °C
  readAll() performs TWO transfers: UV-only read + T+H read.
*/

struct UVSensor_Data {
  float uv_w_m2;       // W/m²
  float humidity_pct;  // %RH
  float temperature_c; // °C
};

class UVSensor {
public:
  UVSensor(RS485Bus &bus, uint8_t address);

  void setAddress(uint8_t address);
  uint8_t address() const;

  bool readAll(UVSensor_Data &out);
  bool changeAddress(uint8_t newAddress);

private:
  RS485Bus &_bus;
  uint8_t _addr;
};
