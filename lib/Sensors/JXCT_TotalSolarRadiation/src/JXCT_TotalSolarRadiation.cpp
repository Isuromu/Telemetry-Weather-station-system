#include "JXCT_TotalSolarRadiation.h"
#include <math.h>

JXCT_TotalSolarRadiation::JXCT_TotalSolarRadiation(RS485Bus& bus,
                                                   const char* sensorId,
                                                   uint8_t address,
                                                   bool debugEnable,
                                                   double maxSolar_w_m2,
                                                   uint8_t powerLineIndex,
                                                   uint8_t interfaceIndex,
                                                   uint16_t sampleRateMin,
                                                   uint32_t warmUpTimeMs,
                                                   uint8_t maxConsecutiveErrors,
                                                   uint32_t minUsefulPowerOffMs)
    : JXCTModbusSensorBase(bus, sensorId, address, 0x0000, debugEnable, powerLineIndex, interfaceIndex,
                            sampleRateMin, warmUpTimeMs, maxConsecutiveErrors, minUsefulPowerOffMs),
      solar_raw(0),
      total_solar_w_m2(0.0),
      _maxSolar_w_m2(maxSolar_w_m2 > 0.0 ? maxSolar_w_m2 : 1500.0) {}

void JXCT_TotalSolarRadiation::setFallbackValues() {
  solar_raw = 0;
  total_solar_w_m2 = -99.0;
}

bool JXCT_TotalSolarRadiation::readSolarRadiation(uint8_t driverRetries,
                                                  uint16_t readTimeoutMs,
                                                  uint16_t afterReqDelayMs) {
  uint16_t word = 0;
  if (!readRegisterBlock(0x0000, 1, &word, driverRetries, readTimeoutMs, afterReqDelayMs)) {
    return false;
  }
  solar_raw = word;
  total_solar_w_m2 = (double)word;
  logParsedU16(F("JXCT_TotalSolarRadiation"), F("Solar radiation"), word, total_solar_w_m2, F("W/m2"), 0);
  return validateSolar();
}

bool JXCT_TotalSolarRadiation::validateSolar() const {
  if (isFaultRaw16(solar_raw)) return false;
  if (isnan(total_solar_w_m2) || total_solar_w_m2 < 0.0 || total_solar_w_m2 > _maxSolar_w_m2) return false;
  return true;
}

bool JXCT_TotalSolarRadiation::readData() {
  markReadTime(millis());
  if (readSolarRadiation()) {
    markSuccess();
    return true;
  }
  if (!_lastParsedFrame) setFallbackValues();
  markFailure();
  return false;
}