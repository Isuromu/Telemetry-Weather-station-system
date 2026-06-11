#pragma once
#include <Arduino.h>
#include "JXCTModbusBase.h"

class JXCT_WindSpeed : public JXCTModbusSensorBase {
public:
  uint16_t wind_speed_raw;
  double wind_speed_m_s;

  JXCT_WindSpeed(RS485Bus& bus,
                 const char* sensorId,
                 uint8_t address,
                 bool debugEnable = false,
                 double maxWindSpeed_m_s = 30.0,
                 uint8_t powerLineIndex = 0,
                 uint8_t interfaceIndex = 0,
                 uint16_t sampleRateMin = 1,
                 uint32_t warmUpTimeMs = 500,
                 uint8_t maxConsecutiveErrors = 10,
                 uint32_t minUsefulPowerOffMs = 60000UL);

  bool readData() override;
  void setFallbackValues() override;
  bool readWindSpeed(uint8_t driverRetries = SENSOR_DEFAULT_DRIVER_RETRIES,
                     uint16_t readTimeoutMs = SENSOR_DEFAULT_READ_TIMEOUT_MS,
                     uint16_t afterReqDelayMs = SENSOR_DEFAULT_AFTER_REQ_MS);

private:
  double _maxWindSpeed_m_s;
  bool validateWindSpeed() const;
};
