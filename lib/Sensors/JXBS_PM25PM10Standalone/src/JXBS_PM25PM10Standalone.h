#pragma once
#include <Arduino.h>
#include "JXCTModbusBase.h"

class JXBS_PM25PM10Standalone : public JXCTModbusSensorBase {
public:
  uint16_t pm2_5_raw;
  uint16_t pm10_raw;
  double pm2_5_ug_m3;
  double pm10_ug_m3;

  JXBS_PM25PM10Standalone(RS485Bus& bus,
                          const char* sensorId,
                          uint8_t address,
                          bool debugEnable = false,
                          double maxPM_ug_m3 = 300.0,
                          uint8_t powerLineIndex = 0,
                          uint8_t interfaceIndex = 0,
                          uint16_t sampleRateMin = 1,
                          uint32_t warmUpTimeMs = 500,
                          uint8_t maxConsecutiveErrors = 10,
                          uint32_t minUsefulPowerOffMs = 60000UL);

  bool readData() override;
  void setFallbackValues() override;
  bool readPMBlock(uint8_t driverRetries = SENSOR_DEFAULT_DRIVER_RETRIES,
                   uint16_t readTimeoutMs = SENSOR_DEFAULT_READ_TIMEOUT_MS,
                   uint16_t afterReqDelayMs = SENSOR_DEFAULT_AFTER_REQ_MS);
  bool readPM25(uint8_t driverRetries = SENSOR_DEFAULT_DRIVER_RETRIES,
                uint16_t readTimeoutMs = SENSOR_DEFAULT_READ_TIMEOUT_MS,
                uint16_t afterReqDelayMs = SENSOR_DEFAULT_AFTER_REQ_MS);
  bool readPM10(uint8_t driverRetries = SENSOR_DEFAULT_DRIVER_RETRIES,
                uint16_t readTimeoutMs = SENSOR_DEFAULT_READ_TIMEOUT_MS,
                uint16_t afterReqDelayMs = SENSOR_DEFAULT_AFTER_REQ_MS);

private:
  double _maxPM_ug_m3;
  bool validatePM() const;
};
