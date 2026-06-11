#pragma once
#include <Arduino.h>
#include "JXCTModbusBase.h"

/*
  JXCT_TemperatureHumidity

  Reads a common JXCT/JXBS RS485 temperature and humidity transmitter.

  Modbus:
    Function 0x03
    Register 0x0000: humidity, u16, /10 = %RH
    Register 0x0001: temperature, s16, /10 = C
*/
class JXCT_TemperatureHumidity : public JXCTModbusSensorBase {
public:
  double humidity_percent;
  double air_temperature_C;

  JXCT_TemperatureHumidity(RS485Bus& bus,
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

  bool readTemperatureHumidity(uint8_t driverRetries = SENSOR_DEFAULT_DRIVER_RETRIES,
                               uint16_t readTimeoutMs = SENSOR_DEFAULT_READ_TIMEOUT_MS,
                               uint16_t afterReqDelayMs = SENSOR_DEFAULT_AFTER_REQ_MS);

private:
  bool validateReadings() const;
};
