#pragma once
#include <Arduino.h>
#include "JXCTModbusBase.h"

class JXCT_UVRays : public JXCTModbusSensorBase {
public:
  double humidity_percent;
  double air_temperature_C;
  uint16_t uv_raw;
  double uv_w_m2;

  JXCT_UVRays(RS485Bus& bus,
              const char* sensorId,
              uint8_t address,
              bool debugEnable = false,
              double maxUV_w_m2 = 150.0,
              uint8_t powerLineIndex = 0,
              uint8_t interfaceIndex = 0,
              uint16_t sampleRateMin = 1,
              uint32_t warmUpTimeMs = 500,
              uint8_t maxConsecutiveErrors = 10,
              uint32_t minUsefulPowerOffMs = 60000UL);

  bool readData() override;
  void setFallbackValues() override;
  bool readEnvironment(uint8_t driverRetries = SENSOR_DEFAULT_DRIVER_RETRIES,
                       uint16_t readTimeoutMs = SENSOR_DEFAULT_READ_TIMEOUT_MS,
                       uint16_t afterReqDelayMs = SENSOR_DEFAULT_AFTER_REQ_MS);
  bool readUV(uint8_t driverRetries = SENSOR_DEFAULT_DRIVER_RETRIES,
              uint16_t readTimeoutMs = SENSOR_DEFAULT_READ_TIMEOUT_MS,
              uint16_t afterReqDelayMs = SENSOR_DEFAULT_AFTER_REQ_MS);

private:
  double _maxUV_w_m2;
  bool validateEnvironment() const;
  bool validateUV() const;
};
