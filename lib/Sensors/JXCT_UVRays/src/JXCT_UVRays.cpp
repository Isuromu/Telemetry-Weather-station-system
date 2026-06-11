#include "JXCT_UVRays.h"
#include <math.h>

JXCT_UVRays::JXCT_UVRays(RS485Bus& bus,
                         const char* sensorId,
                         uint8_t address,
                         bool debugEnable,
                         double maxUV_w_m2,
                         uint8_t powerLineIndex,
                         uint8_t interfaceIndex,
                         uint16_t sampleRateMin,
                         uint32_t warmUpTimeMs,
                         uint8_t maxConsecutiveErrors,
                         uint32_t minUsefulPowerOffMs)
    : JXCTModbusSensorBase(bus, sensorId, address, 0x0008, debugEnable, powerLineIndex, interfaceIndex,
                            sampleRateMin, warmUpTimeMs, maxConsecutiveErrors, minUsefulPowerOffMs),
      humidity_percent(0.0),
      air_temperature_C(0.0),
      uv_raw(0),
      uv_w_m2(0.0),
      _maxUV_w_m2(maxUV_w_m2 > 0.0 ? maxUV_w_m2 : 150.0) {}

void JXCT_UVRays::setFallbackValues() {
  humidity_percent = -99.0;
  air_temperature_C = -99.0;
  uv_raw = 0;
  uv_w_m2 = -99.0;
}

bool JXCT_UVRays::readEnvironment(uint8_t driverRetries,
                                  uint16_t readTimeoutMs,
                                  uint16_t afterReqDelayMs) {
  uint16_t words[2] = {0};
  if (!readRegisterBlock(0x0000, 2, words, driverRetries, readTimeoutMs, afterReqDelayMs)) {
    return false;
  }
  humidity_percent = scaleU16(words[0], 10.0);
  air_temperature_C = (double)((int16_t)words[1]) / 10.0;
  logParsedU16(F("JXCT_UVRays"), F("Humidity"), words[0], humidity_percent, F("%RH"), 1);
  logParsedU16(F("JXCT_UVRays"), F("Temperature"), words[1], air_temperature_C, F("C"), 1);
  return validateEnvironment();
}

bool JXCT_UVRays::readUV(uint8_t driverRetries,
                         uint16_t readTimeoutMs,
                         uint16_t afterReqDelayMs) {
  uint16_t word = 0;
  if (!readRegisterBlock(0x0008, 1, &word, driverRetries, readTimeoutMs, afterReqDelayMs)) {
    return false;
  }
  uv_raw = word;
  uv_w_m2 = scaleU16(word, 10.0);
  logParsedU16(F("JXCT_UVRays"), F("UV"), word, uv_w_m2, F("W/m2"), 1);
  return validateUV();
}

bool JXCT_UVRays::validateEnvironment() const {
  if (isnan(humidity_percent) || humidity_percent < 0.0 || humidity_percent > 100.0) return false;
  if (isnan(air_temperature_C) || air_temperature_C < -40.0 || air_temperature_C > 80.0) return false;
  return true;
}

bool JXCT_UVRays::validateUV() const {
  if (isFaultRaw16(uv_raw)) return false;
  if (isnan(uv_w_m2) || uv_w_m2 < 0.0 || uv_w_m2 > _maxUV_w_m2) return false;
  return true;
}

bool JXCT_UVRays::readData() {
  markReadTime(millis());
  bool gotAnyFrame = false;
  const bool envOk = readEnvironment(1, SENSOR_DEFAULT_READ_TIMEOUT_MS, SENSOR_DEFAULT_AFTER_REQ_MS);
  gotAnyFrame = gotAnyFrame || _lastParsedFrame;
  const bool uvOk = readUV(1, SENSOR_DEFAULT_READ_TIMEOUT_MS, SENSOR_DEFAULT_AFTER_REQ_MS);
  gotAnyFrame = gotAnyFrame || _lastParsedFrame;

  if (envOk && uvOk) {
    markSuccess();
    return true;
  }
  if (!gotAnyFrame) setFallbackValues();
  markFailure();
  return false;
}