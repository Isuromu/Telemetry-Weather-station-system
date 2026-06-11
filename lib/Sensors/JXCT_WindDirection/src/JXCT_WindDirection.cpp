#include "JXCT_WindDirection.h"
#include <math.h>

JXCT_WindDirection::JXCT_WindDirection(RS485Bus& bus,
                                       const char* sensorId,
                                       uint8_t address,
                                       bool debugEnable,
                                       uint8_t powerLineIndex,
                                       uint8_t interfaceIndex,
                                       uint16_t sampleRateMin,
                                       uint32_t warmUpTimeMs,
                                       uint8_t maxConsecutiveErrors,
                                       uint32_t minUsefulPowerOffMs)
    : JXCTModbusSensorBase(bus, sensorId, address, 0x0000, debugEnable, powerLineIndex, interfaceIndex,
                            sampleRateMin, warmUpTimeMs, maxConsecutiveErrors, minUsefulPowerOffMs),
      wind_direction_raw(0),
      wind_direction_deg(0.0) {}

void JXCT_WindDirection::setFallbackValues() {
  wind_direction_raw = 0;
  wind_direction_deg = -99.0;
}

bool JXCT_WindDirection::readWindDirection(uint8_t driverRetries,
                                           uint16_t readTimeoutMs,
                                           uint16_t afterReqDelayMs) {
  uint16_t word = 0;
  if (!readRegisterBlock(0x0000, 1, &word, driverRetries, readTimeoutMs, afterReqDelayMs)) {
    return false;
  }
  wind_direction_raw = word;
  wind_direction_deg = (double)word;
  logParsedU16(F("JXCT_WindDirection"), F("Wind direction"), word, wind_direction_deg, F("deg"), 0);
  return validateWindDirection();
}

bool JXCT_WindDirection::validateWindDirection() const {
  if (isFaultRaw16(wind_direction_raw)) return false;
  if (isnan(wind_direction_deg) || wind_direction_deg < 0.0 || wind_direction_deg > 360.0) return false;
  return true;
}

bool JXCT_WindDirection::readData() {
  markReadTime(millis());
  if (readWindDirection()) {
    markSuccess();
    return true;
  }
  if (!_lastParsedFrame) setFallbackValues();
  markFailure();
  return false;
}