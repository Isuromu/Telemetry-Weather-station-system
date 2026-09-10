#pragma once

#include <Arduino.h>
#include <PressureNodeTypes.h>

namespace irrigation::pressure_node {

struct BatteryMonitorConfiguration {
  uint8_t adcPin;
  float dividerHighOhm;
  float dividerLowOhm;
  float calibration;
  uint8_t sampleCount;
  uint16_t settlingTimeMs;
};

constexpr float batteryVoltageFromAdc(
    float adcVoltage, const BatteryMonitorConfiguration &configuration) {
  return adcVoltage *
         (configuration.dividerHighOhm + configuration.dividerLowOhm) /
         configuration.dividerLowOhm * configuration.calibration;
}

class BatteryMonitor {
 public:
  explicit BatteryMonitor(BatteryMonitorConfiguration configuration);

  bool begin();
  BatteryReading read();

 private:
  BatteryMonitorConfiguration configuration_;
  bool initialized_{false};

  bool configurationValid() const;
};

}  // namespace irrigation::pressure_node
