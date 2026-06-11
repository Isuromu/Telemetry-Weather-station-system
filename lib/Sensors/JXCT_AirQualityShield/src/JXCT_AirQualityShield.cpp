#include "JXCT_AirQualityShield.h"
#include <math.h>

JXCT_AirQualityShield::JXCT_AirQualityShield(RS485Bus& bus,
                                             const char* sensorId,
                                             uint8_t address,
                                             bool debugEnable,
                                             double maxPM_ug_m3,
                                             double maxTVOC_ppb,
                                             uint8_t powerLineIndex,
                                             uint8_t interfaceIndex,
                                             uint16_t sampleRateMin,
                                             uint32_t warmUpTimeMs,
                                             uint8_t maxConsecutiveErrors,
                                             uint32_t minUsefulPowerOffMs)
    : JXCTModbusSensorBase(bus, sensorId, address, 0x0000, debugEnable, powerLineIndex, interfaceIndex,
                            sampleRateMin, warmUpTimeMs, maxConsecutiveErrors, minUsefulPowerOffMs),
      humidity_percent(0.0),
      air_temperature_C(0.0),
      pm2_5_raw(0),
      pm10_raw(0),
      tvoc_raw_ppb(0),
      pm2_5_ug_m3(0.0),
      pm10_ug_m3(0.0),
      tvoc_ppb(0.0),
      tvoc_ppm(0.0),
      _maxPM_ug_m3(maxPM_ug_m3 > 0.0 ? maxPM_ug_m3 : 300.0),
      _maxTVOC_ppb(maxTVOC_ppb > 0.0 ? maxTVOC_ppb : 1000.0) {}

void JXCT_AirQualityShield::setFallbackValues() {
  humidity_percent = -99.0;
  air_temperature_C = -99.0;
  pm2_5_raw = 0;
  pm10_raw = 0;
  tvoc_raw_ppb = 0;
  pm2_5_ug_m3 = -99.0;
  pm10_ug_m3 = -99.0;
  tvoc_ppb = -99.0;
  tvoc_ppm = -99.0;
}

bool JXCT_AirQualityShield::readAirQualityBlock(uint8_t driverRetries,
                                                uint16_t readTimeoutMs,
                                                uint16_t afterReqDelayMs) {
  uint16_t tempHumidityWords[2] = {0};
  if (!readRegisterBlock(0x0000, 2, tempHumidityWords, driverRetries, readTimeoutMs, afterReqDelayMs)) {
    return false;
  }

  if (!readRegisterBlock(0x0004, 1, &pm2_5_raw, driverRetries, readTimeoutMs, afterReqDelayMs)) {
    return false;
  }
  if (!readRegisterBlock(0x0006, 1, &tvoc_raw_ppb, driverRetries, readTimeoutMs, afterReqDelayMs)) {
    return false;
  }
  if (!readRegisterBlock(0x0009, 1, &pm10_raw, driverRetries, readTimeoutMs, afterReqDelayMs)) {
    return false;
  }

  humidity_percent = scaleU16(tempHumidityWords[0], 10.0);
  air_temperature_C = (double)((int16_t)tempHumidityWords[1]) / 10.0;
  pm2_5_ug_m3 = (double)pm2_5_raw;
  pm10_ug_m3 = (double)pm10_raw;
  tvoc_ppb = (double)tvoc_raw_ppb;
  tvoc_ppm = tvoc_ppb / 1000.0;

  logParsedU16(F("JXCT_AirQualityShield"), F("Humidity"), tempHumidityWords[0], humidity_percent, F("%RH"), 1);
  logParsedU16(F("JXCT_AirQualityShield"), F("Temperature"), tempHumidityWords[1], air_temperature_C, F("C"), 1);
  logParsedU16(F("JXCT_AirQualityShield"), F("PM2.5"), pm2_5_raw, pm2_5_ug_m3, F("ug/m3"), 0);
  logParsedU16(F("JXCT_AirQualityShield"), F("TVOC"), tvoc_raw_ppb, tvoc_ppb, F("ppb"), 0);
  logParsedU16(F("JXCT_AirQualityShield"), F("PM10"), pm10_raw, pm10_ug_m3, F("ug/m3"), 0);

  return validateReadings();
}

bool JXCT_AirQualityShield::validateReadings() const {
  if (isnan(humidity_percent) || humidity_percent < 0.0 || humidity_percent > 100.0) return false;
  if (isnan(air_temperature_C) || air_temperature_C < -40.0 || air_temperature_C > 80.0) return false;
  if (isFaultRaw16(pm2_5_raw) || pm2_5_ug_m3 < 0.0 || pm2_5_ug_m3 > _maxPM_ug_m3) return false;
  if (isFaultRaw16(pm10_raw) || pm10_ug_m3 < 0.0 || pm10_ug_m3 > _maxPM_ug_m3) return false;
  if (isFaultRaw16(tvoc_raw_ppb) || tvoc_ppb < 0.0 || tvoc_ppb > _maxTVOC_ppb) return false;
  return true;
}

bool JXCT_AirQualityShield::readData() {
  markReadTime(millis());
  if (readAirQualityBlock()) {
    markSuccess();
    return true;
  }
  if (!_lastParsedFrame) setFallbackValues();
  markFailure();
  return false;
}
