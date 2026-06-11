#pragma once
#include <Arduino.h>
#include "JXCTModbusBase.h"

/*
  JXCT_AtmosphericPressure

  The provided pressure manual exposes a 12-register block starting at 0x0000.
  Humidity and temperature are at words 0 and 1. The pressure value is a 32-bit
  value scaled by /100 to mbar, but the copied manual did not preserve a clean
  register offset. To keep the driver usable, it scans adjacent word pairs in
  the valid Modbus frame and accepts the first pressure-like value.
*/
class JXCT_AtmosphericPressure : public JXCTModbusSensorBase {
public:
  double humidity_percent;
  double air_temperature_C;
  uint32_t pressure_raw;
  double pressure_mbar;
  uint8_t pressure_word_index;

  JXCT_AtmosphericPressure(RS485Bus& bus,
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

  bool readPressureBlock(uint8_t driverRetries = SENSOR_DEFAULT_DRIVER_RETRIES,
                         uint16_t readTimeoutMs = SENSOR_DEFAULT_READ_TIMEOUT_MS,
                         uint16_t afterReqDelayMs = SENSOR_DEFAULT_AFTER_REQ_MS);

private:
  static const uint8_t PRESSURE_BLOCK_WORDS = 12;

  bool parsePressureFromWords(const uint16_t* words, uint8_t wordCount);
  bool validateEnvironment() const;
  bool validatePressure() const;
};
