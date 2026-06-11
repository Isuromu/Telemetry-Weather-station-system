#include "JXCT_PAR.h"
#include <math.h>

JXCT_PAR::JXCT_PAR(RS485Bus& bus,
                   const char* sensorId,
                   uint8_t address,
                   bool debugEnable,
                   double parScaleDivisor,
                   double maxPAR,
                   uint8_t powerLineIndex,
                   uint8_t interfaceIndex,
                   uint16_t sampleRateMin,
                   uint32_t warmUpTimeMs,
                   uint8_t maxConsecutiveErrors,
                   uint32_t minUsefulPowerOffMs)
    : JXCTModbusSensorBase(bus, sensorId, address, 0x0006, debugEnable, powerLineIndex, interfaceIndex,
                            sampleRateMin, warmUpTimeMs, maxConsecutiveErrors, minUsefulPowerOffMs),
      par_raw(0),
      par_value(0.0),
      _parScaleDivisor(parScaleDivisor > 0.0 ? parScaleDivisor : 1.0),
      _maxPAR(maxPAR > 0.0 ? maxPAR : 2000.0) {}

void JXCT_PAR::setFallbackValues() {
  par_raw = 0;
  par_value = -99.0;
}

bool JXCT_PAR::readPAR(uint8_t driverRetries,
                       uint16_t readTimeoutMs,
                       uint16_t afterReqDelayMs) {
  uint16_t word = 0;
  if (!readRegisterBlock(0x0006, 1, &word, driverRetries, readTimeoutMs, afterReqDelayMs)) {
    return false;
  }
  par_raw = word;
  par_value = scaleU16(word, _parScaleDivisor);
  logParsedU16(F("JXCT_PAR"), F("PAR"), word, par_value, F("sensor-unit"), 2);
  return validatePAR();
}

bool JXCT_PAR::validatePAR() const {
  if (isFaultRaw16(par_raw)) return false;
  if (isnan(par_value) || par_value < 0.0 || par_value > _maxPAR) return false;
  return true;
}

bool JXCT_PAR::readData() {
  markReadTime(millis());
  if (readPAR()) {
    markSuccess();
    return true;
  }
  if (!_lastParsedFrame) setFallbackValues();
  markFailure();
  return false;
}