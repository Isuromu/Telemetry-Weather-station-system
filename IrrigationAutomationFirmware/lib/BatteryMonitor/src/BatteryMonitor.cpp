#include "BatteryMonitor.h"

namespace irrigation::pressure_node {

BatteryMonitor::BatteryMonitor(BatteryMonitorConfiguration configuration)
    : configuration_(configuration) {}

bool BatteryMonitor::configurationValid() const {
  return configuration_.dividerHighOhm > 0.0F &&
         configuration_.dividerLowOhm > 0.0F &&
         configuration_.calibration > 0.0F && configuration_.sampleCount > 0;
}

bool BatteryMonitor::begin() {
  if (!configurationValid()) {
    initialized_ = false;
    return false;
  }

  analogReadResolution(12);
  // The pin must be configured as an analog channel before the per-pin
  // attenuation is applied: the Arduino-ESP32 core rejects the attenuation
  // call on a pin that is not yet an ADC channel, silently leaving the ADC
  // at its default attenuation.
  pinMode(configuration_.adcPin, ANALOG);
  analogSetPinAttenuation(configuration_.adcPin, ADC_11db);
  initialized_ = true;
  return true;
}

BatteryReading BatteryMonitor::read() {
  BatteryReading reading{};
  if (!configurationValid()) {
    reading.status = ReadingStatus::ConfigurationMissing;
    return reading;
  }
  if (!initialized_) {
    reading.status = ReadingStatus::NotInitialized;
    return reading;
  }

  // Discard the first conversion, then allow the ADC input and its 100 nF
  // filter capacitor to settle before averaging the on-demand sample.
  (void)analogReadMilliVolts(configuration_.adcPin);
  if (configuration_.settlingTimeMs > 0)
    delay(configuration_.settlingTimeMs);

  uint32_t millivoltSum = 0;
  for (uint8_t sample = 0; sample < configuration_.sampleCount; ++sample) {
    millivoltSum += analogReadMilliVolts(configuration_.adcPin);
    delayMicroseconds(200);
  }

  const float adcVoltage =
      (millivoltSum / static_cast<float>(configuration_.sampleCount)) /
      1000.0F;
  reading.adcVoltageV = adcVoltage;
  reading.voltageV = batteryVoltageFromAdc(adcVoltage, configuration_);
  reading.status = ReadingStatus::Valid;

  // Voltage is measured, but state of charge is deliberately unknown until a
  // battery-specific, load-aware model is validated.
  reading.socStatus = BatterySocStatus::Unknown;
  reading.socPercent = 0.0F;
  return reading;
}

}  // namespace irrigation::pressure_node
