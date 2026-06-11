#pragma once
#include <Arduino.h>
#include "JXCTModbusBase.h"

class JXBS_OpticalRainGauge : public JXCTModbusSensorBase {
public:
  uint16_t rainfall_raw;
  double rainfall_mm;

  JXBS_OpticalRainGauge(RS485Bus& bus,
                        const char* sensorId,
                        uint8_t address,
                        bool debugEnable = false,
                        double maxRainfall_mm = 10000.0,
                        uint8_t powerLineIndex = 0,
                        uint8_t interfaceIndex = 0,
                        uint16_t sampleRateMin = 1,
                        uint32_t warmUpTimeMs = 500,
                        uint8_t maxConsecutiveErrors = 10,
                        uint32_t minUsefulPowerOffMs = 60000UL);

  bool readData() override;
  void setFallbackValues() override;
  bool readRainfall(uint8_t driverRetries = SENSOR_DEFAULT_DRIVER_RETRIES,
                    uint16_t readTimeoutMs = SENSOR_DEFAULT_READ_TIMEOUT_MS,
                    uint16_t afterReqDelayMs = SENSOR_DEFAULT_AFTER_REQ_MS);
  bool clearAccumulatedRainfall(uint16_t clearRegister = 0x0105,
                                uint8_t maxRetries = SENSOR_DEFAULT_BUS_RETRIES,
                                uint16_t readTimeoutMs = 500,
                                uint16_t afterReqDelayMs = SENSOR_DEFAULT_AFTER_REQ_MS);

private:
  double _maxRainfall_mm;
  bool validateRainfall() const;
};
