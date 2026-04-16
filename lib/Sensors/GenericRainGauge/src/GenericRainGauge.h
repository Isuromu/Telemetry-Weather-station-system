#pragma once
#include "RainGaugeCounter.h"

class GenericRainGauge : public RainGaugeCounter {
public:
  static constexpr double MM_PER_PULSE = 0.2794;

  GenericRainGauge(const char* sensorId,
                   bool debugEnable,
                   uint8_t powerLineIndex,
                   uint8_t interfaceIndex,
                   uint16_t sampleRateMin,
                   uint32_t warmUpTimeMs,
                   uint8_t maxConsecutiveErrors,
                   uint32_t minUsefulPowerOffMs,
                   CountingMode countingMode,
                   uint8_t pcf8574Address,
                   int8_t counterResetPin,
                   int8_t interruptPin,
                   uint16_t interruptDebounceMs);
};
