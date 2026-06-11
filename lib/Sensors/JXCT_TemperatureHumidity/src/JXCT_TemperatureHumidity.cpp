#include "JXCT_TemperatureHumidity.h"
#include <math.h>

JXCT_TemperatureHumidity::JXCT_TemperatureHumidity(RS485Bus& bus,
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
      air_temperature_C(0.0) {}

void JXCT_TemperatureHumidity::setFallbackValues() {
  humidity_percent = -99.0;
  air_temperature_C = -99.0;
}

bool JXCT_TemperatureHumidity::readTemperatureHumidity(uint8_t driverRetries,
                                                       uint16_t readTimeoutMs,
                                                       uint16_t afterReqDelayMs) {
  uint16_t words[2] = {0};
  if (!readRegisterBlock(0x0000, 2, words, driverRetries, readTimeoutMs, afterReqDelayMs)) {
    return false;
  }

  humidity_percent = scaleU16(words[0], 10.0);
  air_temperature_C = (double)((int16_t)words[1]) / 10.0;

  logParsedU16(F("JXCT_TemperatureHumidity"), F("Humidity"), words[0], humidity_percent, F("%RH"), 1);
  logParsedU16(F("JXCT_TemperatureHumidity"), F("Temperature"), words[1], air_temperature_C, F("C"), 1);
  return validateReadings();
}

bool JXCT_TemperatureHumidity::validateReadings() const {
  if (isnan(humidity_percent) || humidity_percent < 0.0 || humidity_percent > 100.0) return false;
  if (isnan(air_temperature_C) || air_temperature_C < -40.0 || air_temperature_C > 80.0) return false;
  return true;
}

bool JXCT_TemperatureHumidity::readData() {
  markReadTime(millis());
  if (readTemperatureHumidity()) {
    markSuccess();
    return true;
  }
  if (!_lastParsedFrame) setFallbackValues();
  markFailure();
  return false;
}
