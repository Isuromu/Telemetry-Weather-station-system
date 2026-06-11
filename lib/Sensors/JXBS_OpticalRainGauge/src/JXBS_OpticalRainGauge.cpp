#include "JXBS_OpticalRainGauge.h"
#include <math.h>

JXBS_OpticalRainGauge::JXBS_OpticalRainGauge(RS485Bus& bus,
                                             const char* sensorId,
                                             uint8_t address,
                                             bool debugEnable,
                                             double maxRainfall_mm,
                                             uint8_t powerLineIndex,
                                             uint8_t interfaceIndex,
                                             uint16_t sampleRateMin,
                                             uint32_t warmUpTimeMs,
                                             uint8_t maxConsecutiveErrors,
                                             uint32_t minUsefulPowerOffMs)
    : JXCTModbusSensorBase(bus, sensorId, address, 0x0003, debugEnable, powerLineIndex, interfaceIndex,
                            sampleRateMin, warmUpTimeMs, maxConsecutiveErrors, minUsefulPowerOffMs),
      rainfall_raw(0),
      rainfall_mm(0.0),
      _maxRainfall_mm(maxRainfall_mm > 0.0 ? maxRainfall_mm : 10000.0) {}

void JXBS_OpticalRainGauge::setFallbackValues() {
  rainfall_raw = 0;
  rainfall_mm = -99.0;
}

bool JXBS_OpticalRainGauge::readRainfall(uint8_t driverRetries,
                                         uint16_t readTimeoutMs,
                                         uint16_t afterReqDelayMs) {
  uint16_t word = 0;
  if (!readRegisterBlock(0x0003, 1, &word, driverRetries, readTimeoutMs, afterReqDelayMs)) {
    return false;
  }

  rainfall_raw = word;
  rainfall_mm = scaleU16(rainfall_raw, 10.0);
  logParsedU16(F("JXBS_OpticalRainGauge"), F("Rainfall"), rainfall_raw, rainfall_mm, F("mm"), 1);
  return validateRainfall();
}

bool JXBS_OpticalRainGauge::clearAccumulatedRainfall(uint16_t clearRegister,
                                                     uint8_t maxRetries,
                                                     uint16_t readTimeoutMs,
                                                     uint16_t afterReqDelayMs) {
  return writeSingleRegisterEcho(clearRegister, 0x0000, false, maxRetries, readTimeoutMs, afterReqDelayMs);
}

bool JXBS_OpticalRainGauge::validateRainfall() const {
  if (isFaultRaw16(rainfall_raw)) return false;
  if (isnan(rainfall_mm) || rainfall_mm < 0.0 || rainfall_mm > _maxRainfall_mm) return false;
  return true;
}

bool JXBS_OpticalRainGauge::readData() {
  markReadTime(millis());
  if (readRainfall()) {
    markSuccess();
    return true;
  }
  if (!_lastParsedFrame) setFallbackValues();
  markFailure();
  return false;
}