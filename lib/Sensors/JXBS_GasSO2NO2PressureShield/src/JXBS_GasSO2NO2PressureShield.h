#pragma once
#include <Arduino.h>
#include "JXCTModbusBase.h"

class JXBS_GasSO2NO2PressureShield : public JXCTModbusSensorBase {
public:
  uint16_t no2_raw;
  uint16_t so2_raw;
  uint32_t pressure_raw;
  double no2_ppm;
  double so2_ppm;
  double pressure_mbar;

  JXBS_GasSO2NO2PressureShield(RS485Bus& bus,
                               const char* sensorId,
                               uint8_t address,
                               bool debugEnable = false,
                               // Defaults match the 2000 ppm analog-probe variant
                               // resolution (0.1 ppm). Confirm real Modbus scale.
                               double no2ScaleDivisor = 10.0,
                               double so2ScaleDivisor = 10.0,
                               double maxNO2_ppm = 2000.0,
                               double maxSO2_ppm = 2000.0,
                               uint8_t powerLineIndex = 0,
                               uint8_t interfaceIndex = 0,
                               uint16_t sampleRateMin = 1,
                               uint32_t warmUpTimeMs = 500,
                               uint8_t maxConsecutiveErrors = 10,
                               uint32_t minUsefulPowerOffMs = 60000UL);

  bool readData() override;
  void setFallbackValues() override;
  bool readGasBlock(uint8_t driverRetries = SENSOR_DEFAULT_DRIVER_RETRIES,
                    uint16_t readTimeoutMs = SENSOR_DEFAULT_READ_TIMEOUT_MS,
                    uint16_t afterReqDelayMs = SENSOR_DEFAULT_AFTER_REQ_MS);
  bool readPressure(uint8_t driverRetries = SENSOR_DEFAULT_DRIVER_RETRIES,
                    uint16_t readTimeoutMs = SENSOR_DEFAULT_READ_TIMEOUT_MS,
                    uint16_t afterReqDelayMs = SENSOR_DEFAULT_AFTER_REQ_MS);
  bool readMeasurementBlock(uint8_t driverRetries = SENSOR_DEFAULT_DRIVER_RETRIES,
                            uint16_t readTimeoutMs = SENSOR_DEFAULT_READ_TIMEOUT_MS,
                            uint16_t afterReqDelayMs = SENSOR_DEFAULT_AFTER_REQ_MS);

private:
  double _no2ScaleDivisor;
  double _so2ScaleDivisor;
  double _maxNO2_ppm;
  double _maxSO2_ppm;
  bool validateGases() const;
  bool validatePressure() const;
};
