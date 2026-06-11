#include "JXCT_Evaporation.h"
#include <math.h>

JXCT_Evaporation::JXCT_Evaporation(RS485Bus& bus,
                                   const char* sensorId,
                                   uint8_t address,
                                   bool debugEnable,
                                   double evaporationScaleDivisor,
                                   double maxEvaporationValue,
                                   uint8_t powerLineIndex,
                                   uint8_t interfaceIndex,
                                   uint16_t sampleRateMin,
                                   uint32_t warmUpTimeMs,
                                   uint8_t maxConsecutiveErrors,
                                   uint32_t minUsefulPowerOffMs)
    : JXCTModbusSensorBase(bus, sensorId, address, 0x0006, debugEnable, powerLineIndex, interfaceIndex,
                            sampleRateMin, warmUpTimeMs, maxConsecutiveErrors, minUsefulPowerOffMs),
      evaporation_raw(0),
      evaporation_value(0.0),
      _evaporationScaleDivisor(evaporationScaleDivisor > 0.0 ? evaporationScaleDivisor : 1.0),
      _maxEvaporationValue(maxEvaporationValue > 0.0 ? maxEvaporationValue : 200.0) {}

void JXCT_Evaporation::setFallbackValues() {
  evaporation_raw = 0;
  evaporation_value = -99.0;
}

bool JXCT_Evaporation::readEvaporation(uint8_t driverRetries,
                                       uint16_t readTimeoutMs,
                                       uint16_t afterReqDelayMs) {
  uint16_t word = 0;
  if (!readRegisterBlock(0x0006, 1, &word, driverRetries, readTimeoutMs, afterReqDelayMs)) {
    return false;
  }
  evaporation_raw = word;
  evaporation_value = scaleU16(word, _evaporationScaleDivisor);
  logParsedU16(F("JXCT_Evaporation"), F("Evaporation"), word, evaporation_value, F("sensor-unit"), 2);
  return validateEvaporation();
}

bool JXCT_Evaporation::tare(uint16_t commandValue,
                            uint8_t maxRetries,
                            uint16_t readTimeoutMs,
                            uint16_t afterReqDelayMs) {
  return writeSingleRegisterEcho(0x0102, commandValue, false, maxRetries, readTimeoutMs, afterReqDelayMs);
}

bool JXCT_Evaporation::validateEvaporation() const {
  if (isFaultRaw16(evaporation_raw)) return false;
  if (isnan(evaporation_value) || evaporation_value < 0.0 || evaporation_value > _maxEvaporationValue) return false;
  return true;
}

bool JXCT_Evaporation::readData() {
  markReadTime(millis());
  if (readEvaporation()) {
    markSuccess();
    return true;
  }
  if (!_lastParsedFrame) setFallbackValues();
  markFailure();
  return false;
}