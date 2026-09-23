#pragma once

#include <RS485ModBus.h>

namespace irrigation::power {

struct EpeverLs1024bConfiguration {
  uint8_t slaveAddress{0x60};
  uint16_t responseTimeoutMs{400};
  bool debug{false};
};

struct EpeverLs1024bReading {
  bool batteryVoltageValid{false};
  bool batteryTemperatureValid{false};
  bool loadCurrentValid{false};
  bool batteryStatusValid{false};
  float batteryVoltage{0.0F};
  float batteryTemperatureC{0.0F};
  float loadCurrentA{0.0F};
  uint16_t batteryStatus{0};

  bool anyValid() const {
    return batteryVoltageValid || batteryTemperatureValid ||
           loadCurrentValid || batteryStatusValid;
  }
};

class EpeverLs1024b {
 public:
  explicit EpeverLs1024b(
      RS485Bus &transport,
      EpeverLs1024bConfiguration configuration = {});

  bool begin();
  EpeverLs1024bReading read();

 private:
  RS485Bus &transport_;
  EpeverLs1024bConfiguration configuration_;
  bool initialized_{false};

  bool readInputRegister(uint16_t address, uint16_t &value);
};

}  // namespace irrigation::power
