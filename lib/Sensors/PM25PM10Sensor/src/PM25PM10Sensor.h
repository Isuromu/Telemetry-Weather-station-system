#pragma once
#include "RS485Modbus.h"
#include <Arduino.h>

/*
  PM25PM10Sensor driver (JXBS-3001-PM2.5/PM10, RS485, Shutter Box)
  Register 0x0004: PM2.5, u16, 1 µg/m³ per LSB (0–300 µg/m³)
  Register 0x0009: PM10, u16, 1 µg/m³ per LSB (0–300 µg/m³)
  readAll() performs TWO separate FC03 reads (registers are non-contiguous).
*/

struct PM25PM10Sensor_Data {
  uint16_t pm25_ug_m3; // µg/m³
  uint16_t pm10_ug_m3; // µg/m³
};

class PM25PM10Sensor {
public:
  PM25PM10Sensor(RS485Bus &bus, uint8_t address);

  void setAddress(uint8_t address);
  uint8_t address() const;

  bool readAll(PM25PM10Sensor_Data &out);
  bool changeAddress(uint8_t newAddress);

private:
  RS485Bus &_bus;
  uint8_t _addr;
};
