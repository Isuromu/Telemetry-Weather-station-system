#pragma once
#include "RainGaugeCounter.h"

class RikaRainGauge : public RainGaugeCounter {
public:
  static constexpr double MM_PER_PULSE = 0.2;

  RikaRainGauge(RS485Bus& bus,
                const char* sensorId,
                uint8_t address,
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

  bool changeAddress(uint8_t newAddress,
                     uint8_t maxRetries = SENSOR_DEFAULT_BUS_RETRIES,
                     uint16_t readTimeoutMs = 500,
                     uint16_t afterReqDelayMs = SENSOR_DEFAULT_AFTER_REQ_MS);

protected:
  bool supportsRs485SensorMode() const override { return true; }
  bool readRs485Rainfall(double& rainfallMm) override;
  bool resetRs485Rainfall() override;
};
