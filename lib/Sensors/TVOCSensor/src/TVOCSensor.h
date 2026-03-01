#pragma once
#include "RS485Modbus.h"
#include <Arduino.h>

/*
  TVOCSensor driver (RS485 TVOC multi-parameter air quality board)
  Registers (non-contiguous — readAll() performs 6 separate reads):
    0x0000: humidity u16 /10 → %RH
    0x0001: temperature s16 /10 → °C
    0x0004: PM2.5 u16, µg/m³
    0x0005: CO2 u16, ppm (400–5000)
    0x0006: TVOC u16, ppb (0–1000)
    0x0009: PM10 u16, µg/m³
*/

struct TVOCSensor_Data {
  float humidity_pct;  // %RH
  float temperature_c; // °C
  uint16_t pm25_ug_m3; // µg/m³
  uint16_t co2_ppm;    // ppm
  uint16_t tvoc_ppb;   // ppb
  uint16_t pm10_ug_m3; // µg/m³
};

class TVOCSensor {
public:
  TVOCSensor(RS485Bus &bus, uint8_t address);

  void setAddress(uint8_t address);
  uint8_t address() const;

  bool readAll(TVOCSensor_Data &out);
  bool changeAddress(uint8_t newAddress);

private:
  RS485Bus &_bus;
  uint8_t _addr;

  // Helper: reads one u16 from a single holding register
  bool readU16(uint16_t reg, uint16_t &val);
  // Helper: reads one s16 from a single holding register
  bool readS16(uint16_t reg, int16_t &val);
};
