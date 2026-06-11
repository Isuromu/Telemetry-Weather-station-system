#include "JXCT_WindSpeed.h"
#include <math.h>

JXCT_WindSpeed::JXCT_WindSpeed(RS485Bus& bus,
                               const char* sensorId,
                               uint8_t address,
                               bool debugEnable,
                               double maxWindSpeed_m_s,
                               uint8_t powerLineIndex,
                               uint8_t interfaceIndex,
                               uint16_t sampleRateMin,
                               uint32_t warmUpTimeMs,
                               uint8_t maxConsecutiveErrors,
                               uint32_t minUsefulPowerOffMs)
    : JXCTModbusSensorBase(bus, sensorId, address, 0x0016, debugEnable, powerLineIndex, interfaceIndex,
                            sampleRateMin, warmUpTimeMs, maxConsecutiveErrors, minUsefulPowerOffMs),
      wind_speed_raw(0),
      wind_speed_m_s(0.0),
      _maxWindSpeed_m_s(maxWindSpeed_m_s > 0.0 ? maxWindSpeed_m_s : 30.0) {}

void JXCT_WindSpeed::setFallbackValues() {
  wind_speed_raw = 0;
  wind_speed_m_s = -99.0;
}

bool JXCT_WindSpeed::readWindSpeed(uint8_t driverRetries,
                                   uint16_t readTimeoutMs,
                                   uint16_t afterReqDelayMs) {
  uint16_t word = 0;
  if (!readRegisterBlock(0x0016, 1, &word, driverRetries, readTimeoutMs, afterReqDelayMs)) {
    return false;
  }
  wind_speed_raw = word;
  wind_speed_m_s = scaleU16(word, 10.0);
  logParsedU16(F("JXCT_WindSpeed"), F("Wind speed"), word, wind_speed_m_s, F("m/s"), 1);
  return validateWindSpeed();
}

bool JXCT_WindSpeed::validateWindSpeed() const {
  if (isFaultRaw16(wind_speed_raw)) return false;
  if (isnan(wind_speed_m_s) || wind_speed_m_s < 0.0 || wind_speed_m_s > _maxWindSpeed_m_s) return false;
  return true;
}

bool JXCT_WindSpeed::readData() {
  markReadTime(millis());
  if (readWindSpeed()) {
    markSuccess();
    return true;
  }
  if (!_lastParsedFrame) setFallbackValues();
  markFailure();
  return false;
}