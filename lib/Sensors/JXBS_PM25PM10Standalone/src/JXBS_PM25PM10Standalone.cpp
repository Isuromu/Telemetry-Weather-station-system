#include "JXBS_PM25PM10Standalone.h"
#include <math.h>

JXBS_PM25PM10Standalone::JXBS_PM25PM10Standalone(RS485Bus& bus,
                                                 const char* sensorId,
                                                 uint8_t address,
                                                 bool debugEnable,
                                                 double maxPM_ug_m3,
                                                 uint8_t powerLineIndex,
                                                 uint8_t interfaceIndex,
                                                 uint16_t sampleRateMin,
                                                 uint32_t warmUpTimeMs,
                                                 uint8_t maxConsecutiveErrors,
                                                 uint32_t minUsefulPowerOffMs)
    : JXCTModbusSensorBase(bus, sensorId, address, 0x0004, debugEnable, powerLineIndex, interfaceIndex,
                            sampleRateMin, warmUpTimeMs, maxConsecutiveErrors, minUsefulPowerOffMs),
      pm2_5_raw(0),
      pm10_raw(0),
      pm2_5_ug_m3(0.0),
      pm10_ug_m3(0.0),
      _maxPM_ug_m3(maxPM_ug_m3 > 0.0 ? maxPM_ug_m3 : 300.0) {}

void JXBS_PM25PM10Standalone::setFallbackValues() {
  pm2_5_raw = 0;
  pm10_raw = 0;
  pm2_5_ug_m3 = -99.0;
  pm10_ug_m3 = -99.0;
}

bool JXBS_PM25PM10Standalone::readPMBlock(uint8_t driverRetries,
                                          uint16_t readTimeoutMs,
                                          uint16_t afterReqDelayMs) {
  const bool pm25Ok = readPM25(driverRetries, readTimeoutMs, afterReqDelayMs);
  const bool pm10Ok = readPM10(driverRetries, readTimeoutMs, afterReqDelayMs);
  return pm25Ok && pm10Ok && validatePM();
}

bool JXBS_PM25PM10Standalone::readPM25(uint8_t driverRetries,
                                       uint16_t readTimeoutMs,
                                       uint16_t afterReqDelayMs) {
  uint16_t word = 0;
  if (!readRegisterBlock(0x0004, 1, &word, driverRetries, readTimeoutMs, afterReqDelayMs)) {
    return false;
  }

  pm2_5_raw = word;
  pm2_5_ug_m3 = (double)word;
  logParsedU16(F("JXBS_PM25PM10Standalone"), F("PM2.5"), pm2_5_raw, pm2_5_ug_m3, F("ug/m3"), 0);
  return !isFaultRaw16(pm2_5_raw) && pm2_5_ug_m3 >= 0.0 && pm2_5_ug_m3 <= _maxPM_ug_m3;
}

bool JXBS_PM25PM10Standalone::readPM10(uint8_t driverRetries,
                                       uint16_t readTimeoutMs,
                                       uint16_t afterReqDelayMs) {
  uint16_t word = 0;
  if (!readRegisterBlock(0x0009, 1, &word, driverRetries, readTimeoutMs, afterReqDelayMs)) {
    return false;
  }

  pm10_raw = word;
  pm10_ug_m3 = (double)word;
  logParsedU16(F("JXBS_PM25PM10Standalone"), F("PM10"), pm10_raw, pm10_ug_m3, F("ug/m3"), 0);
  return !isFaultRaw16(pm10_raw) && pm10_ug_m3 >= 0.0 && pm10_ug_m3 <= _maxPM_ug_m3;
}

bool JXBS_PM25PM10Standalone::validatePM() const {
  if (isFaultRaw16(pm2_5_raw) || pm2_5_ug_m3 < 0.0 || pm2_5_ug_m3 > _maxPM_ug_m3) return false;
  if (isFaultRaw16(pm10_raw) || pm10_ug_m3 < 0.0 || pm10_ug_m3 > _maxPM_ug_m3) return false;
  return true;
}

bool JXBS_PM25PM10Standalone::readData() {
  markReadTime(millis());
  if (readPMBlock()) {
    markSuccess();
    return true;
  }
  if (!_lastParsedFrame) setFallbackValues();
  markFailure();
  return false;
}
