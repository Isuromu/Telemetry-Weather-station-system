#include "JXCT_WeatherStationSensors.h"
#include <math.h>

JXCT_ModbusSensorBase::JXCT_ModbusSensorBase(RS485Bus& bus,
                                             const char* sensorId,
                                             uint8_t address,
                                             uint16_t scanRegister,
                                             bool debugEnable,
                                             uint8_t powerLineIndex,
                                             uint8_t interfaceIndex,
                                             uint16_t sampleRateMin,
                                             uint32_t warmUpTimeMs,
                                             uint8_t maxConsecutiveErrors,
                                             uint32_t minUsefulPowerOffMs)
    : SensorDriver(sensorId,
                   address,
                   debugEnable,
                   powerLineIndex,
                   interfaceIndex,
                   sampleRateMin,
                   warmUpTimeMs,
                   maxConsecutiveErrors,
                   minUsefulPowerOffMs),
      _bus(bus),
      _scanRegister(scanRegister),
      _lastParsedFrame(false) {}

bool JXCT_ModbusSensorBase::readRegisterBlock(uint16_t startRegister,
                                              uint8_t registerCount,
                                              uint16_t* words,
                                              uint8_t driverRetries,
                                              uint16_t readTimeoutMs,
                                              uint16_t afterReqDelayMs) {
  if (!words || registerCount == 0 || registerCount > MAX_READ_REGISTERS) {
    return false;
  }
  if (driverRetries == 0) driverRetries = 1;
  _lastParsedFrame = false;

  const uint8_t responseSize = (uint8_t)(5 + (2 * registerCount));

  for (uint8_t attempt = 1; attempt <= driverRetries; ++attempt) {
    uint8_t request[8] = {
      _address,
      0x03,
      (uint8_t)(startRegister >> 8),
      (uint8_t)(startRegister & 0xFF),
      0x00,
      registerCount,
      0x00,
      0x00
    };
    uint8_t response[5 + (2 * MAX_READ_REGISTERS)] = {0};
    const uint8_t check[3] = {_address, 0x03, (uint8_t)(2 * registerCount)};

    const bool ok = _bus.SendRequest(request,
                                     sizeof(request),
                                     response,
                                     responseSize,
                                     check,
                                     sizeof(check),
                                     SENSOR_DEFAULT_BUS_RETRIES,
                                     readTimeoutMs,
                                     _debugEnable,
                                     afterReqDelayMs);
    if (!ok) {
      continue;
    }

    _lastParsedFrame = true;
    for (uint8_t i = 0; i < registerCount; ++i) {
      words[i] = ((uint16_t)response[3 + (2 * i)] << 8) | response[4 + (2 * i)];
    }
    return true;
  }

  return false;
}

bool JXCT_ModbusSensorBase::writeSingleRegisterEcho(uint16_t registerAddress,
                                                    uint16_t value,
                                                    bool acceptValueAsAddress,
                                                    uint8_t maxRetries,
                                                    uint16_t readTimeoutMs,
                                                    uint16_t afterReqDelayMs) {
  if (maxRetries == 0) maxRetries = 1;

  uint8_t request[8] = {
    _address,
    0x06,
    (uint8_t)(registerAddress >> 8),
    (uint8_t)(registerAddress & 0xFF),
    (uint8_t)(value >> 8),
    (uint8_t)(value & 0xFF),
    0x00,
    0x00
  };
  const uint8_t oldAddress = _address;
  const uint8_t possibleNewAddress = (uint8_t)value;

  for (uint8_t attempt = 1; attempt <= maxRetries; ++attempt) {
    _bus.CRC_Calc(request, sizeof(request), _debugEnable);
    _bus.Request_RS485(request, sizeof(request), afterReqDelayMs, _debugEnable);

    const size_t bytesRead = _bus.Read_RS485(readTimeoutMs, _debugEnable);
    const uint8_t* raw = _bus.rawData();

    if (bytesRead >= 8 && raw) {
      for (size_t offset = 0; offset <= (bytesRead - 8); ++offset) {
        const uint8_t* frame = &raw[offset];
        const bool addressOk =
            frame[0] == oldAddress ||
            (acceptValueAsAddress && frame[0] == possibleNewAddress);
        const bool echoOk =
            addressOk &&
            frame[1] == 0x06 &&
            frame[2] == (uint8_t)(registerAddress >> 8) &&
            frame[3] == (uint8_t)(registerAddress & 0xFF) &&
            frame[4] == (uint8_t)(value >> 8) &&
            frame[5] == (uint8_t)(value & 0xFF) &&
            RS485Bus::verifyCrc16ModbusFrame(frame, 8);

        if (echoOk) {
          return true;
        }
      }
    }

    delay(100UL * attempt);
  }

  return false;
}

bool JXCT_ModbusSensorBase::changeAddress(uint8_t newAddress,
                                          uint8_t maxRetries,
                                          uint16_t readTimeoutMs,
                                          uint16_t afterReqDelayMs) {
  if (newAddress == 0 || newAddress > 247) {
    return false;
  }

  if (writeSingleRegisterEcho(0x0100, newAddress, true, maxRetries, readTimeoutMs, afterReqDelayMs)) {
    _address = newAddress;
    return true;
  }

  return false;
}

uint8_t JXCT_ModbusSensorBase::scanForAddress(uint8_t startAddr,
                                              uint8_t endAddr,
                                              uint16_t readTimeoutMs,
                                              uint16_t afterReqDelayMs) {
  if (startAddr == 0) startAddr = 1;
  if (endAddr > 247) endAddr = 247;
  if (startAddr > endAddr) return 0;

  for (uint16_t addr = startAddr; addr <= endAddr; ++addr) {
    uint8_t request[8] = {
      (uint8_t)addr,
      0x03,
      (uint8_t)(_scanRegister >> 8),
      (uint8_t)(_scanRegister & 0xFF),
      0x00,
      0x01,
      0x00,
      0x00
    };
    uint8_t response[7] = {0};
    const uint8_t check[3] = {(uint8_t)addr, 0x03, 0x02};

    const bool found = _bus.SendRequest(request,
                                        sizeof(request),
                                        response,
                                        sizeof(response),
                                        check,
                                        sizeof(check),
                                        1,
                                        readTimeoutMs,
                                        _debugEnable,
                                        afterReqDelayMs);
    if (found) {
      _address = (uint8_t)addr;
      return (uint8_t)addr;
    }

    delay(5);
  }

  return 0;
}

void JXCT_ModbusSensorBase::logParsedU16(const __FlashStringHelper* driverLabel,
                                         const __FlashStringHelper* valueLabel,
                                         uint16_t raw,
                                         double value,
                                         const __FlashStringHelper* unit,
                                         uint8_t decimals) const {
  if (!_bus.getLogger() || !_debugEnable) return;

  _bus.getLogger()->print(F("[DRV]["), true);
  _bus.getLogger()->print(driverLabel, true);
  _bus.getLogger()->print(F("] "), true);
  _bus.getLogger()->print(valueLabel, true);
  _bus.getLogger()->print(F(" raw = "), true);
  _bus.getLogger()->print((unsigned int)raw, true, "", DEC);
  _bus.getLogger()->print(F(" | value = "), true);
  _bus.getLogger()->print(value, true, "", decimals);
  if (unit) {
    _bus.getLogger()->print(F(" "), true);
    _bus.getLogger()->print(unit, true);
  }
  _bus.getLogger()->println("", true);
}

bool JXCT_ModbusSensorBase::isFaultRaw16(uint16_t raw) {
  return raw == 0xFFFFU || raw == 0x7FFFU || raw == 0x8000U;
}

double JXCT_ModbusSensorBase::scaleU16(uint16_t raw, double divisor) {
  return (double)raw / (divisor > 0.0 ? divisor : 1.0);
}

uint32_t JXCT_ModbusSensorBase::joinU32(uint16_t highWord, uint16_t lowWord) {
  return ((uint32_t)highWord << 16) | (uint32_t)lowWord;
}

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
    : JXCT_ModbusSensorBase(bus, sensorId, address, 0x0000, debugEnable, powerLineIndex, interfaceIndex,
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
  uint16_t words[10] = {0};
  if (!readRegisterBlock(0x0000, 10, words, driverRetries, readTimeoutMs, afterReqDelayMs)) {
    return false;
  }

  humidity_percent = scaleU16(words[0], 10.0);
  air_temperature_C = (double)((int16_t)words[1]) / 10.0;
  pm2_5_raw = words[4];
  tvoc_raw_ppb = words[6];
  pm10_raw = words[9];
  pm2_5_ug_m3 = (double)pm2_5_raw;
  pm10_ug_m3 = (double)pm10_raw;
  tvoc_ppb = (double)tvoc_raw_ppb;
  tvoc_ppm = tvoc_ppb / 1000.0;

  logParsedU16(F("JXCT_AirQualityShield"), F("Humidity"), words[0], humidity_percent, F("%RH"), 1);
  logParsedU16(F("JXCT_AirQualityShield"), F("Temperature"), words[1], air_temperature_C, F("C"), 1);
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
    : JXCT_ModbusSensorBase(bus, sensorId, address, 0x0016, debugEnable, powerLineIndex, interfaceIndex,
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
    : JXCT_ModbusSensorBase(bus, sensorId, address, 0x0000, debugEnable, powerLineIndex, interfaceIndex,
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
    : JXCT_ModbusSensorBase(bus, sensorId, address, 0x0008, debugEnable, powerLineIndex, interfaceIndex,
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
    : JXCT_ModbusSensorBase(bus, sensorId, address, 0x0006, debugEnable, powerLineIndex, interfaceIndex,
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
    : JXCT_ModbusSensorBase(bus, sensorId, address, 0x0000, debugEnable, powerLineIndex, interfaceIndex,
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
    : JXCT_ModbusSensorBase(bus, sensorId, address, 0x0006, debugEnable, powerLineIndex, interfaceIndex,
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

JXBS_GasO3CONH3Shield::JXBS_GasO3CONH3Shield(RS485Bus& bus,
                                             const char* sensorId,
                                             uint8_t address,
                                             bool debugEnable,
                                             double maxCO_ppm,
                                             double maxO3_ppm,
                                             double maxNH3_ppm,
                                             uint8_t powerLineIndex,
                                             uint8_t interfaceIndex,
                                             uint16_t sampleRateMin,
                                             uint32_t warmUpTimeMs,
                                             uint8_t maxConsecutiveErrors,
                                             uint32_t minUsefulPowerOffMs)
    : JXCT_ModbusSensorBase(bus, sensorId, address, 0x0006, debugEnable, powerLineIndex, interfaceIndex,
                            sampleRateMin, warmUpTimeMs, maxConsecutiveErrors, minUsefulPowerOffMs),
      co_raw(0),
      o3_raw(0),
      nh3_raw(0),
      co_ppm(0.0),
      o3_ppm(0.0),
      nh3_ppm(0.0),
      _maxCO_ppm(maxCO_ppm > 0.0 ? maxCO_ppm : 2000.0),
      _maxO3_ppm(maxO3_ppm > 0.0 ? maxO3_ppm : 100.0),
      _maxNH3_ppm(maxNH3_ppm > 0.0 ? maxNH3_ppm : 5000.0) {}

void JXBS_GasO3CONH3Shield::setFallbackValues() {
  co_raw = 0;
  o3_raw = 0;
  nh3_raw = 0;
  co_ppm = -99.0;
  o3_ppm = -99.0;
  nh3_ppm = -99.0;
}

bool JXBS_GasO3CONH3Shield::readGasBlock(uint8_t driverRetries,
                                         uint16_t readTimeoutMs,
                                         uint16_t afterReqDelayMs) {
  uint16_t words[3] = {0};
  if (!readRegisterBlock(0x0006, 3, words, driverRetries, readTimeoutMs, afterReqDelayMs)) {
    return false;
  }

  co_raw = words[0];
  o3_raw = words[1];
  nh3_raw = words[2];
  co_ppm = scaleU16(co_raw, 10.0);
  o3_ppm = scaleU16(o3_raw, 100.0);
  nh3_ppm = scaleU16(nh3_raw, 10.0);

  logParsedU16(F("JXBS_GasO3CONH3Shield"), F("CO"), co_raw, co_ppm, F("ppm"), 1);
  logParsedU16(F("JXBS_GasO3CONH3Shield"), F("O3"), o3_raw, o3_ppm, F("ppm"), 2);
  logParsedU16(F("JXBS_GasO3CONH3Shield"), F("NH3"), nh3_raw, nh3_ppm, F("ppm"), 1);

  return validateGases();
}

bool JXBS_GasO3CONH3Shield::validateGases() const {
  if (isFaultRaw16(co_raw) || isnan(co_ppm) || co_ppm < 0.0 || co_ppm > _maxCO_ppm) return false;
  if (isFaultRaw16(o3_raw) || isnan(o3_ppm) || o3_ppm < 0.0 || o3_ppm > _maxO3_ppm) return false;
  if (isFaultRaw16(nh3_raw) || isnan(nh3_ppm) || nh3_ppm < 0.0 || nh3_ppm > _maxNH3_ppm) return false;
  return true;
}

bool JXBS_GasO3CONH3Shield::readData() {
  markReadTime(millis());
  if (readGasBlock()) {
    markSuccess();
    return true;
  }
  if (!_lastParsedFrame) setFallbackValues();
  markFailure();
  return false;
}

JXBS_GasSO2NO2PressureShield::JXBS_GasSO2NO2PressureShield(RS485Bus& bus,
                                                           const char* sensorId,
                                                           uint8_t address,
                                                           bool debugEnable,
                                                           double no2ScaleDivisor,
                                                           double so2ScaleDivisor,
                                                           double maxNO2_ppm,
                                                           double maxSO2_ppm,
                                                           uint8_t powerLineIndex,
                                                           uint8_t interfaceIndex,
                                                           uint16_t sampleRateMin,
                                                           uint32_t warmUpTimeMs,
                                                           uint8_t maxConsecutiveErrors,
                                                           uint32_t minUsefulPowerOffMs)
    : JXCT_ModbusSensorBase(bus, sensorId, address, 0x0006, debugEnable, powerLineIndex, interfaceIndex,
                            sampleRateMin, warmUpTimeMs, maxConsecutiveErrors, minUsefulPowerOffMs),
      no2_raw(0),
      so2_raw(0),
      pressure_raw(0),
      no2_ppm(0.0),
      so2_ppm(0.0),
      pressure_mbar(0.0),
      _no2ScaleDivisor(no2ScaleDivisor > 0.0 ? no2ScaleDivisor : 10.0),
      _so2ScaleDivisor(so2ScaleDivisor > 0.0 ? so2ScaleDivisor : 10.0),
      _maxNO2_ppm(maxNO2_ppm > 0.0 ? maxNO2_ppm : 2000.0),
      _maxSO2_ppm(maxSO2_ppm > 0.0 ? maxSO2_ppm : 2000.0) {}

void JXBS_GasSO2NO2PressureShield::setFallbackValues() {
  no2_raw = 0;
  so2_raw = 0;
  pressure_raw = 0;
  no2_ppm = -99.0;
  so2_ppm = -99.0;
  pressure_mbar = -99.0;
}

bool JXBS_GasSO2NO2PressureShield::readGasBlock(uint8_t driverRetries,
                                                uint16_t readTimeoutMs,
                                                uint16_t afterReqDelayMs) {
  uint16_t words[2] = {0};
  if (!readRegisterBlock(0x0006, 2, words, driverRetries, readTimeoutMs, afterReqDelayMs)) {
    return false;
  }

  no2_raw = words[0];
  so2_raw = words[1];
  no2_ppm = scaleU16(no2_raw, _no2ScaleDivisor);
  so2_ppm = scaleU16(so2_raw, _so2ScaleDivisor);

  logParsedU16(F("JXBS_GasSO2NO2PressureShield"), F("NO2"), no2_raw, no2_ppm, F("ppm"), 2);
  logParsedU16(F("JXBS_GasSO2NO2PressureShield"), F("SO2"), so2_raw, so2_ppm, F("ppm"), 2);

  return validateGases();
}

bool JXBS_GasSO2NO2PressureShield::readPressure(uint8_t driverRetries,
                                                uint16_t readTimeoutMs,
                                                uint16_t afterReqDelayMs) {
  uint16_t words[2] = {0};
  if (!readRegisterBlock(0x0012, 2, words, driverRetries, readTimeoutMs, afterReqDelayMs)) {
    return false;
  }

  pressure_raw = joinU32(words[0], words[1]);
  pressure_mbar = (double)pressure_raw / 100.0;

  if (_bus.getLogger() && _debugEnable) {
    _bus.getLogger()->print(F("[DRV][JXBS_GasSO2NO2PressureShield] Pressure raw = "), true);
    _bus.getLogger()->print((unsigned long)pressure_raw, true, " | value = ", DEC);
    _bus.getLogger()->print(pressure_mbar, true, " mbar", 2);
    _bus.getLogger()->println("", true);
  }

  return validatePressure();
}

bool JXBS_GasSO2NO2PressureShield::validateGases() const {
  if (isFaultRaw16(no2_raw) || isnan(no2_ppm) || no2_ppm < 0.0 || no2_ppm > _maxNO2_ppm) return false;
  if (isFaultRaw16(so2_raw) || isnan(so2_ppm) || so2_ppm < 0.0 || so2_ppm > _maxSO2_ppm) return false;
  return true;
}

bool JXBS_GasSO2NO2PressureShield::validatePressure() const {
  if (pressure_raw == 0xFFFFFFFFUL || pressure_raw == 0x7FFFFFFFUL || pressure_raw == 0x80000000UL) return false;
  if (isnan(pressure_mbar) || pressure_mbar < 10.0 || pressure_mbar > 1200.0) return false;
  return true;
}

bool JXBS_GasSO2NO2PressureShield::readData() {
  markReadTime(millis());
  bool gotAnyFrame = false;
  const bool gasOk = readGasBlock(1, SENSOR_DEFAULT_READ_TIMEOUT_MS, SENSOR_DEFAULT_AFTER_REQ_MS);
  gotAnyFrame = gotAnyFrame || _lastParsedFrame;
  const bool pressureOk = readPressure(1, SENSOR_DEFAULT_READ_TIMEOUT_MS, SENSOR_DEFAULT_AFTER_REQ_MS);
  gotAnyFrame = gotAnyFrame || _lastParsedFrame;

  if (gasOk && pressureOk) {
    markSuccess();
    return true;
  }
  if (!gotAnyFrame) setFallbackValues();
  markFailure();
  return false;
}
