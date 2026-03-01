#pragma once
#include "RS485Modbus.h"
#include <Arduino.h>

/*
  AtmosphericPressureSensor driver
  Source: RS485 Atmospheric Pressure Sensor manual (doc also contains T/H)
  Reads 12 registers from 0x0000:
    Reg 0x0000: humidity u16 /10 → %RH
    Reg 0x0001: temperature s16 /10 → °C
    Regs 0x0002–0x0003: pressure u32 /100 → mbar
      (TODO: confirm exact word offsets on hardware — doc does not cleanly state
  position)

  Hard bounds: humidity 0–100 %RH, temp -40–80 °C, pressure 10–1200 mbar
*/

struct AtmosphericPressureSensor_Data {
  float humidity_pct;  // %RH
  float temperature_c; // °C
  float pressure_mbar; // mbar (TODO: verify register offset on hardware)
};

class AtmosphericPressureSensor {
public:
  AtmosphericPressureSensor(RS485Bus &bus, uint8_t address);

  void setAddress(uint8_t address);
  uint8_t address() const;

  bool readAll(AtmosphericPressureSensor_Data &out);

private:
  RS485Bus &_bus;
  uint8_t _addr;
};
