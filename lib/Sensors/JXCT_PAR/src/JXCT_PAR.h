#pragma once
#include <Arduino.h>
#include "JXCTModbusBase.h"

class JXCT_PAR : public JXCTModbusSensorBase {
public:
  uint16_t par_raw;
  double par_value;

  JXCT_PAR(RS485Bus& bus,
           const char* sensorId,
           uint8_t address,
           bool debugEnable = false,
           double parScaleDivisor = 1.0,
           double maxPAR = 2000.0,
           uint8_t powerLineIndex = 0,
           uint8_t interfaceIndex = 0,
           uint16_t sampleRateMin = 1,
           uint32_t warmUpTimeMs = 500,
           uint8_t maxConsecutiveErrors = 10,
           uint32_t minUsefulPowerOffMs = 60000UL);

  bool readData() override;
  void setFallbackValues() override;
  bool readPAR(uint8_t driverRetries = SENSOR_DEFAULT_DRIVER_RETRIES,
               uint16_t readTimeoutMs = SENSOR_DEFAULT_READ_TIMEOUT_MS,
               uint16_t afterReqDelayMs = SENSOR_DEFAULT_AFTER_REQ_MS);

private:
  double _parScaleDivisor;
  double _maxPAR;
  bool validatePAR() const;
};
