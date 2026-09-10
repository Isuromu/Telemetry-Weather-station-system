#pragma once

#include <Arduino.h>
#include <PressureNodeTypes.h>
#include <Wire.h>

namespace irrigation::pressure_node {

struct Xdb401Configuration {
  uint8_t primaryAddress;
  uint8_t alternateAddress;
  uint8_t pressureRegister;
  uint8_t temperatureRegister;
  uint8_t measurementRegister;
  uint8_t startMeasurementCommand;
  uint8_t busyMask;
  float fullScaleBar;
  bool engineeringScaleValidated;
  uint8_t readyPollAttempts;
  uint16_t readyPollIntervalMs;
};

class PressureSensorXDB401 {
 public:
  PressureSensorXDB401(TwoWire &bus, Xdb401Configuration configuration);

  ReadingStatus begin();
  bool isPresent() const { return address_ != 0; }
  uint8_t address() const { return address_; }
  PressureReading read();

 private:
  TwoWire &bus_;
  Xdb401Configuration configuration_;
  uint8_t address_{0};
  bool initialized_{false};

  bool configurationValid() const;
  bool devicePresent(uint8_t address);
  bool writeRegister(uint8_t reg, uint8_t value);
  bool readRegister(uint8_t reg, uint8_t *data, size_t length);
};

}  // namespace irrigation::pressure_node
