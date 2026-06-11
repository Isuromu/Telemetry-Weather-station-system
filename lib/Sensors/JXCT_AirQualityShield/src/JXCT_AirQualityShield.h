#pragma once
#include <Arduino.h>
#include "JXCTModbusBase.h"

class JXCT_AirQualityShield : public JXCTModbusSensorBase {
public:
  double humidity_percent;
  double air_temperature_C;
  uint16_t pm2_5_raw;
  uint16_t pm10_raw;
  uint16_t tvoc_raw_ppb;
  double pm2_5_ug_m3;
  double pm10_ug_m3;
  double tvoc_ppb;
  double tvoc_ppm;

  JXCT_AirQualityShield(RS485Bus& bus,
                        const char* sensorId,
                        uint8_t address,
                        bool debugEnable = false,
                        double maxPM_ug_m3 = 300.0,
                        double maxTVOC_ppb = 1000.0,
                        uint8_t powerLineIndex = 0,
                        uint8_t interfaceIndex = 0,
                        uint16_t sampleRateMin = 1,
                        uint32_t warmUpTimeMs = 500,
                        uint8_t maxConsecutiveErrors = 10,
                        uint32_t minUsefulPowerOffMs = 60000UL);

  bool readData() override;
  void setFallbackValues() override;
  bool readAirQualityBlock(uint8_t driverRetries = SENSOR_DEFAULT_DRIVER_RETRIES,
                           uint16_t readTimeoutMs = SENSOR_DEFAULT_READ_TIMEOUT_MS,
                           uint16_t afterReqDelayMs = SENSOR_DEFAULT_AFTER_REQ_MS);

private:
  double _maxPM_ug_m3;
  double _maxTVOC_ppb;

  bool validateReadings() const;
};
