#pragma once

#include <Arduino.h>

class BatteryMonitor {
public:
    BatteryMonitor(uint8_t adcPin,
                   float dividerHighOhm,
                   float dividerLowOhm,
                   float calibration,
                   uint8_t sampleCount);

    void begin();
    float readVoltage() const;

private:
    uint8_t _adcPin;
    float _dividerHighOhm;
    float _dividerLowOhm;
    float _calibration;
    uint8_t _sampleCount;
};
