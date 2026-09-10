#include "BatteryMonitor.hpp"

BatteryMonitor::BatteryMonitor(uint8_t adcPin,
                               float dividerHighOhm,
                               float dividerLowOhm,
                               float calibration,
                               uint8_t sampleCount)
    : _adcPin(adcPin),
      _dividerHighOhm(dividerHighOhm),
      _dividerLowOhm(dividerLowOhm),
      _calibration(calibration),
      _sampleCount(sampleCount == 0 ? 1 : sampleCount) {}

void BatteryMonitor::begin() {
    analogReadResolution(12);
    analogSetPinAttenuation(_adcPin, ADC_11db);
}

float BatteryMonitor::readVoltage() const {
    uint32_t millivoltSum = 0;

    for (uint8_t i = 0; i < _sampleCount; ++i) {
        millivoltSum += analogReadMilliVolts(_adcPin);
        delayMicroseconds(200);
    }

    const float adcVoltage =
        (millivoltSum / static_cast<float>(_sampleCount)) / 1000.0F;

    const float batteryVoltage =
        adcVoltage *
        (_dividerHighOhm + _dividerLowOhm) /
        _dividerLowOhm;

    return batteryVoltage * _calibration;
}
