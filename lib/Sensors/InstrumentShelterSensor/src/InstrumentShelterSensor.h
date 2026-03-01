#pragma once
#include "RS485Modbus.h"
#include <Arduino.h>

/*
  InstrumentShelterSensor driver (485 Instrument Shelter, Temp/Humi/Lux RS485)
  Registers 0x0000–0x0001: humidity u16/10 + temperature s16/10
  Registers 0x0007–0x0008: lux u32 = (hi<<16)|lo
  readAll() performs TWO FC03 reads.
  Note: address/baud-change registers are UNCERTAIN in doc — not implemented.
*/

struct InstrumentShelterSensor_Data {
  float humidity_pct;  // %RH
  float temperature_c; // °C
  uint32_t lux;        // Lux
};

class InstrumentShelterSensor {
public:
  InstrumentShelterSensor(RS485Bus &bus, uint8_t address);

  void setAddress(uint8_t address);
  uint8_t address() const;

  bool readAll(InstrumentShelterSensor_Data &out);

private:
  RS485Bus &_bus;
  uint8_t _addr;
};
