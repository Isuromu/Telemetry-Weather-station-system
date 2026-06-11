#include "JXCT_AtmosphericPressure.h"
#include <math.h>

JXCT_AtmosphericPressure::JXCT_AtmosphericPressure(RS485Bus& bus,
                                                   const char* sensorId,
                                                   uint8_t address,
                                                   bool debugEnable,
                                                   uint8_t powerLineIndex,
                                                   uint8_t interfaceIndex,
                                                   uint16_t sampleRateMin,
                                                   uint32_t warmUpTimeMs,
                                                   uint8_t maxConsecutiveErrors,
                                                   uint32_t minUsefulPowerOffMs)
    : JXCTModbusSensorBase(bus,
                           sensorId,
                           address,
                           0x0000,
                           debugEnable,
                           powerLineIndex,
                           interfaceIndex,
                           sampleRateMin,
                           warmUpTimeMs,
                           maxConsecutiveErrors,
                           minUsefulPowerOffMs),
      humidity_percent(0.0),
      air_temperature_C(0.0),
      pressure_raw(0),
      pressure_mbar(0.0),
      pressure_word_index(0xFF) {}

void JXCT_AtmosphericPressure::setFallbackValues() {
  humidity_percent = -99.0;
  air_temperature_C = -99.0;
  pressure_raw = 0;
  pressure_mbar = -99.0;
  pressure_word_index = 0xFF;
}

bool JXCT_AtmosphericPressure::readPressureBlock(uint8_t driverRetries,
                                                 uint16_t readTimeoutMs,
                                                 uint16_t afterReqDelayMs) {
  uint16_t words[PRESSURE_BLOCK_WORDS] = {0};
  if (!readRegisterBlock(0x0000, PRESSURE_BLOCK_WORDS, words, driverRetries, readTimeoutMs, afterReqDelayMs)) {
    return false;
  }

  humidity_percent = scaleU16(words[0], 10.0);
  air_temperature_C = (double)((int16_t)words[1]) / 10.0;

  logParsedU16(F("JXCT_AtmosphericPressure"), F("Humidity"), words[0], humidity_percent, F("%RH"), 1);
  logParsedU16(F("JXCT_AtmosphericPressure"), F("Temperature"), words[1], air_temperature_C, F("C"), 1);

  return validateEnvironment() && parsePressureFromWords(words, PRESSURE_BLOCK_WORDS) && validatePressure();
}

bool JXCT_AtmosphericPressure::parsePressureFromWords(const uint16_t* words, uint8_t wordCount) {
  if (!words || wordCount < 2) return false;

  for (uint8_t i = 0; i + 1 < wordCount; ++i) {
    const uint32_t candidateRaw = joinU32(words[i], words[i + 1]);
    const double candidateMbar = (double)candidateRaw / 100.0;

    if (candidateMbar >= 300.0 && candidateMbar <= 1200.0) {
      pressure_raw = candidateRaw;
      pressure_mbar = candidateMbar;
      pressure_word_index = i;

      if (_bus.getLogger() && _debugEnable) {
        _bus.getLogger()->print(F("[DRV][JXCT_AtmosphericPressure] Pressure raw = "), true);
        _bus.getLogger()->print((unsigned long)pressure_raw, true, "", DEC);
        _bus.getLogger()->print(F(" | value = "), true);
        _bus.getLogger()->print(pressure_mbar, true, " mbar | word index = ", 2);
        _bus.getLogger()->println((unsigned int)pressure_word_index, true);
      }

      return true;
    }
  }

  return false;
}

bool JXCT_AtmosphericPressure::validateEnvironment() const {
  if (isnan(humidity_percent) || humidity_percent < 0.0 || humidity_percent > 100.0) return false;
  if (isnan(air_temperature_C) || air_temperature_C < -40.0 || air_temperature_C > 80.0) return false;
  return true;
}

bool JXCT_AtmosphericPressure::validatePressure() const {
  if (pressure_raw == 0xFFFFFFFFUL || pressure_raw == 0x7FFFFFFFUL || pressure_raw == 0x80000000UL) return false;
  if (isnan(pressure_mbar) || pressure_mbar < 300.0 || pressure_mbar > 1200.0) return false;
  return true;
}

bool JXCT_AtmosphericPressure::readData() {
  markReadTime(millis());
  if (readPressureBlock()) {
    markSuccess();
    return true;
  }
  if (!_lastParsedFrame) setFallbackValues();
  markFailure();
  return false;
}
