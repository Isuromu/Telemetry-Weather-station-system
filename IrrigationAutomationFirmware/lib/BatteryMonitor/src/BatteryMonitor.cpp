#include "BatteryMonitor.h"

namespace irrigation::pressure_node {

BatteryMonitor::BatteryMonitor(BatteryMonitorConfiguration configuration)
    : configuration_(configuration) {}

BatteryMonitor::BatteryMonitor(BatteryMonitorConfiguration configuration,
                               BatteryAdcSource &adcSource)
    : configuration_(configuration), adcSource_(&adcSource) {}

bool BatteryMonitor::configurationValid() const {
  const bool hasDivider = configuration_.dividerHighOhm >= 0.0F &&
                          configuration_.dividerLowOhm > 0.0F;
  return (configuration_.voltageMultiplier > 0.0F || hasDivider) &&
         configuration_.calibration > 0.0F && configuration_.sampleCount > 0;
}

bool BatteryMonitor::begin() {
  if (!configurationValid()) {
    initialized_ = false;
    initializationStatus_ = ReadingStatus::ConfigurationMissing;
    return false;
  }

  if (adcSource_ != nullptr) {
    initializationStatus_ = adcSource_->begin();
    initialized_ = initializationStatus_ == ReadingStatus::Valid;
    return initialized_;
  }

  analogReadResolution(12);
  // Arduino-ESP32 3.x registers an ADC pin inside the first analogRead*(), not
  // in pinMode(): pinMode(pin, ANALOG) leaves the pin configured as a disabled
  // GPIO, after which analogSetPinAttenuation() logs "Pin is not configured as
  // analog channel" and returns without applying anything. Set the attenuation
  // before the first conversion, because the per-unit calibration handle is
  // created from the attenuation in force and a later per-pin change does not
  // rebuild it.
  analogSetAttenuation(ADC_11db);
  initialized_ = true;
  initializationStatus_ = ReadingStatus::Valid;
  return true;
}

ReadingStatus BatteryMonitor::readAdcVoltage(float &adcVoltage) {
  if (adcSource_ != nullptr) return adcSource_->readAdcVoltage(adcVoltage);

  adcVoltage = analogReadMilliVolts(configuration_.adcPin) / 1000.0F;
  return ReadingStatus::Valid;
}

BatteryReading BatteryMonitor::read() {
  BatteryReading reading{};
  if (!configurationValid()) {
    reading.status = ReadingStatus::ConfigurationMissing;
    return reading;
  }
  if (!initialized_) {
    reading.status = initializationStatus_;
    return reading;
  }

  // Discard the first conversion, then allow the ADC input and its 100 nF
  // filter capacitor to settle before averaging the on-demand sample.
  float discardedVoltage = 0.0F;
  (void)readAdcVoltage(discardedVoltage);
  if (configuration_.settlingTimeMs > 0)
    delay(configuration_.settlingTimeMs);

  float adcVoltageSum = 0.0F;
  for (uint8_t sample = 0; sample < configuration_.sampleCount; ++sample) {
    float adcVoltage = 0.0F;
    const ReadingStatus status = readAdcVoltage(adcVoltage);
    if (status != ReadingStatus::Valid) {
      reading.status = status;
      return reading;
    }
    adcVoltageSum += adcVoltage;
    delayMicroseconds(200);
  }

  const float adcVoltage =
      adcVoltageSum / static_cast<float>(configuration_.sampleCount);
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
