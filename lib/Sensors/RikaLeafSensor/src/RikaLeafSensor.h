#pragma once
#include "RS485Modbus.h"
#include <Arduino.h>


// Rika Leaf Moisture / Temperature Sensor Data Structure
// Explicitly requested exact structure variables.
struct RikaLeafSensor_Data {
  double rika_leaf_temp;
  double rika_leaf_humid;
};

class RikaLeafSensor {
public:
  RikaLeafSensor(RS485Bus &bus, uint8_t address);

  void setAddress(uint8_t address);
  uint8_t address() const;

  // Reads Temperature and Humidity
  bool readAll(RikaLeafSensor_Data &out);

  // Broadcast command to change address (0xFF)
  // WARNING: Sensor's white cable must be connected to positive power supply
  // for this to work. ONLY ONE sensor should be on the bus when sending this
  // broadcast.
  bool changeAddress(uint8_t newAddress);

private:
  RS485Bus &_bus;
  uint8_t _addr;
};
