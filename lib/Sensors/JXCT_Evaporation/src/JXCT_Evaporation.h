#pragma once
#include <Arduino.h>
#include "JXCTModbusBase.h"

class JXCT_Evaporation : public JXCTModbusSensorBase {
public:
  uint16_t evaporation_raw;
  double evaporation_value;

  JXCT_Evaporation(RS485Bus& bus,
                   const char* sensorId,
                   uint8_t address,
                   bool debugEnable = false,
                   double evaporationScaleDivisor = 1.0,
                   double maxEvaporationValue = 200.0,
                   uint8_t powerLineIndex = 0,
                   uint8_t interfaceIndex = 0,
                   uint16_t sampleRateMin = 1,
                   uint32_t warmUpTimeMs = 500,
                   uint8_t maxConsecutiveErrors = 10,
                   uint32_t minUsefulPowerOffMs = 60000UL);

  bool readData() override;
  void setFallbackValues() override;
  bool readEvaporation(uint8_t driverRetries = SENSOR_DEFAULT_DRIVER_RETRIES,
                       uint16_t readTimeoutMs = SENSOR_DEFAULT_READ_TIMEOUT_MS,
                       uint16_t afterReqDelayMs = SENSOR_DEFAULT_AFTER_REQ_MS);
  bool tare(uint16_t commandValue = 0x0001,
            uint8_t maxRetries = SENSOR_DEFAULT_BUS_RETRIES,
            uint16_t readTimeoutMs = 500,
            uint16_t afterReqDelayMs = SENSOR_DEFAULT_AFTER_REQ_MS);

private:
  double _evaporationScaleDivisor;
  double _maxEvaporationValue;
  bool validateEvaporation() const;
};
