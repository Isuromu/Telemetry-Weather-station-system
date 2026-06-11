#pragma once
#include <Arduino.h>
#include "JXCTModbusBase.h"

class JXBS_GasO3CONH3Shield : public JXCTModbusSensorBase {
public:
  uint16_t co_raw;
  uint16_t o3_raw;
  uint16_t nh3_raw;
  double co_ppm;
  double o3_ppm;
  double nh3_ppm;

  JXBS_GasO3CONH3Shield(RS485Bus& bus,
                        const char* sensorId,
                        uint8_t address,
                        bool debugEnable = false,
                        double maxCO_ppm = 2000.0,
                        double maxO3_ppm = 100.0,
                        double maxNH3_ppm = 5000.0,
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

private:
  double _maxCO_ppm;
  double _maxO3_ppm;
  double _maxNH3_ppm;
  bool validateGases() const;
};
