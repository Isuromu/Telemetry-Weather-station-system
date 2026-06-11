#pragma once
#include <Arduino.h>
#include "JXCTModbusBase.h"

/*
  JXBS_GasSensor

  Small driver for one physical JXBS-3001 style gas sensor.

  Most confirmed JXBS gas RS485 manuals in this project use:
    Function 0x03
    Register 0x0006: gas concentration, u16, scale depends on gas model

  The scale is passed in the constructor so CO, O3, NH3, SO2 and NO2 can share
  the same readable code while keeping their own Modbus address and range.
*/
class JXBS_GasSensor : public JXCTModbusSensorBase {
public:
  uint16_t gas_raw;
  double gas_ppm;

  JXBS_GasSensor(RS485Bus& bus,
                 const char* sensorId,
                 const char* gasName,
                 uint8_t address,
                 double scaleDivisor,
                 double maxGas_ppm,
                 bool debugEnable = false,
                 uint8_t powerLineIndex = 0,
                 uint8_t interfaceIndex = 0,
                 uint16_t sampleRateMin = 1,
                 uint32_t warmUpTimeMs = 500,
                 uint8_t maxConsecutiveErrors = 10,
                 uint32_t minUsefulPowerOffMs = 60000UL);

  bool readData() override;
  void setFallbackValues() override;

  bool readGas(uint8_t driverRetries = SENSOR_DEFAULT_DRIVER_RETRIES,
               uint16_t readTimeoutMs = SENSOR_DEFAULT_READ_TIMEOUT_MS,
               uint16_t afterReqDelayMs = SENSOR_DEFAULT_AFTER_REQ_MS);

private:
  const char* _gasName;
  double _scaleDivisor;
  double _maxGas_ppm;

  bool validateGas() const;
  void logGasReading() const;
};
