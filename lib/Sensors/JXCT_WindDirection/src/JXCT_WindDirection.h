#pragma once
#include <Arduino.h>
#include "JXCTModbusBase.h"

class JXCT_WindDirection : public JXCTModbusSensorBase {
public:
  uint16_t wind_direction_raw;
  double wind_direction_deg;

  JXCT_WindDirection(RS485Bus& bus,
                     const char* sensorId,
                     uint8_t address,
                     bool debugEnable = false,
                     uint8_t powerLineIndex = 0,
                     uint8_t interfaceIndex = 0,
                     uint16_t sampleRateMin = 1,
                     uint32_t warmUpTimeMs = 500,
                     uint8_t maxConsecutiveErrors = 10,
                     uint32_t minUsefulPowerOffMs = 60000UL);

  bool readData() override;
  void setFallbackValues() override;
  bool readWindDirection(uint8_t driverRetries = SENSOR_DEFAULT_DRIVER_RETRIES,
                         uint16_t readTimeoutMs = SENSOR_DEFAULT_READ_TIMEOUT_MS,
                         uint16_t afterReqDelayMs = SENSOR_DEFAULT_AFTER_REQ_MS);

private:
  bool validateWindDirection() const;
};
