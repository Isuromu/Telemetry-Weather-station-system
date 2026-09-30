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
  // A positive value replaces the resistor-derived multiplier. This supports
  // direct external ADC measurements (1.0) and external divider scaling.
  float voltageMultiplier{0.0F};
};

constexpr float batteryVoltageFromAdc(
    float adcVoltage, const BatteryMonitorConfiguration &configuration) {
  const float dividerMultiplier =
      configuration.voltageMultiplier > 0.0F
          ? configuration.voltageMultiplier
          : (configuration.dividerHighOhm + configuration.dividerLowOhm) /
                configuration.dividerLowOhm;
  return adcVoltage * dividerMultiplier * configuration.calibration;
}

// Keeps source-specific drivers, such as ADS1115, outside this library while
// sharing battery conversion, averaging, calibration and reading status.
class BatteryAdcSource {
 public:
  virtual ~BatteryAdcSource() = default;

  virtual ReadingStatus begin() { return ReadingStatus::Valid; }
  virtual ReadingStatus readAdcVoltage(float &adcVoltage) = 0;
};

class BatteryMonitor {
 public:
  explicit BatteryMonitor(BatteryMonitorConfiguration configuration);
  BatteryMonitor(BatteryMonitorConfiguration configuration,
                 BatteryAdcSource &adcSource);

  bool begin();
  BatteryReading read();

 private:
  BatteryMonitorConfiguration configuration_;
  BatteryAdcSource *adcSource_{nullptr};
  bool initialized_{false};
  ReadingStatus initializationStatus_{ReadingStatus::NotInitialized};

  bool configurationValid() const;
  ReadingStatus readAdcVoltage(float &adcVoltage);
};

}  // namespace irrigation::pressure_node
