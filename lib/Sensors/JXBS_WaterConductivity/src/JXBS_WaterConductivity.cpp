#include "JXBS_WaterConductivity.h"
#include <math.h>

static bool responseIsJXBSWaterConductivityAddressChange(const uint8_t* frame,
                                                         uint8_t oldAddress,
                                                         uint8_t newAddress) {
  if (!frame) return false;

  const bool responseAddressOk = (frame[0] == oldAddress || frame[0] == newAddress);
  return responseAddressOk &&
         frame[1] == 0x06 &&
         frame[2] == 0x01 &&
         frame[3] == 0x00 &&
         frame[4] == 0x00 &&
         frame[5] == newAddress &&
         RS485Bus::verifyCrc16ModbusFrame(frame, 8);
}

static void logParsedJXBSWaterConductivity(PrintController* log,
                                           bool debug,
                                           int16_t rawTemperature,
                                           uint32_t rawConductivity,
                                           double temperature,
                                           double conductivity) {
  if (!log || !debug) return;

  log->println(F("[DRV][JXBS_WaterConductivity] Parsed temperature + conductivity:"), true);

  log->print(F("  temperature raw = "), true);
  log->print((int)rawTemperature, true, "", DEC);
  log->print(F(" | temperature = "), true);
  log->print(temperature, true, " C", 1);
  log->println("", true);

  log->print(F("  conductivity raw = "), true);
  log->print((unsigned long)rawConductivity, true, "", DEC);
  log->print(F(" | conductivity = "), true);
  log->print(conductivity, true, " uS/cm", 2);
  log->println("", true);
}

static void logParsedWaterTemperature(PrintController* log,
                                      bool debug,
                                      int16_t rawTemperature,
                                      double temperature) {
  if (!log || !debug) return;

  log->println(F("[DRV][JXBS_WaterConductivity] Parsed temperature:"), true);
  log->print(F("  temperature raw = "), true);
  log->print((int)rawTemperature, true, "", DEC);
  log->print(F(" | temperature = "), true);
  log->print(temperature, true, " C", 1);
  log->println("", true);
}

static void logParsedConductivity(PrintController* log,
                                  bool debug,
                                  uint32_t rawConductivity,
                                  double conductivity) {
  if (!log || !debug) return;

  log->println(F("[DRV][JXBS_WaterConductivity] Parsed conductivity:"), true);
  log->print(F("  conductivity raw = "), true);
  log->print((unsigned long)rawConductivity, true, "", DEC);
  log->print(F(" | conductivity = "), true);
  log->print(conductivity, true, " uS/cm", 2);
  log->println("", true);
}

JXBS_WaterConductivity::JXBS_WaterConductivity(RS485Bus& bus,
                                               const char* sensorId,
                                               uint8_t address,
                                               bool debugEnable,
                                               double conductivityScaleDivisor,
                                               double maxConductivity_uS_cm,
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
      water_temperature_C(0.0),
      conductivity_raw(0),
      conductivity_uS_cm(0.0),
      _bus(bus),
      _conductivityScaleDivisor(conductivityScaleDivisor > 0.0 ? conductivityScaleDivisor : 100.0),
      _maxConductivity_uS_cm(maxConductivity_uS_cm > 0.0 ? maxConductivity_uS_cm : 200000.0),
      _lastParsedFrame(false) {}

void JXBS_WaterConductivity::setFallbackValues() {
  water_temperature_C = -99.0;
  conductivity_raw = 0;
  conductivity_uS_cm = -99.0;
}

void JXBS_WaterConductivity::setConductivityScaleDivisor(double divisor) {
  if (divisor > 0.0) {
    _conductivityScaleDivisor = divisor;
  }
}

void JXBS_WaterConductivity::setMaxConductivity(double maxConductivity_uS_cm) {
  if (maxConductivity_uS_cm > 0.0) {
    _maxConductivity_uS_cm = maxConductivity_uS_cm;
  }
}

double JXBS_WaterConductivity::scaledConductivity(uint32_t raw) const {
  return (double)raw / _conductivityScaleDivisor;
}

bool JXBS_WaterConductivity::validateTemperature() const {
  if (isnan(water_temperature_C)) return false;
  if (water_temperature_C < -10.0 || water_temperature_C > 80.0) return false;
  return true;
}

bool JXBS_WaterConductivity::validateConductivity() const {
  if (isnan(conductivity_uS_cm)) return false;
  if (conductivity_raw == 0xFFFFFFFFUL || conductivity_raw == 0x7FFFFFFFUL ||
      conductivity_raw == 0x80000000UL) {
    return false;
  }
  if (conductivity_uS_cm < 0.0 || conductivity_uS_cm > _maxConductivity_uS_cm) {
    return false;
  }
  return true;
}

bool JXBS_WaterConductivity::validateTemperatureConductivity() const {
  return validateTemperature() && validateConductivity();
}

bool JXBS_WaterConductivity::readTemperatureConductivity(uint8_t driverRetries,
                                                         uint16_t readTimeoutMs,
                                                         uint16_t afterReqDelayMs) {
  if (driverRetries == 0) driverRetries = 1;
  _lastParsedFrame = false;

  for (uint8_t attempt = 1; attempt <= driverRetries; ++attempt) {
    uint8_t request[READ_ALL_REQUEST_SIZE] = {
      _address, 0x03, 0x00, 0x01, 0x00, 0x03, 0x00, 0x00
    };

    uint8_t response[READ_ALL_RESPONSE_SIZE] = {0};
    const uint8_t check[READ_CHECK_SIZE] = {_address, 0x03, 0x06};

    const bool ok = _bus.SendRequest(request,
                                     READ_ALL_REQUEST_SIZE,
                                     response,
                                     READ_ALL_RESPONSE_SIZE,
                                     check,
                                     READ_CHECK_SIZE,
                                     SENSOR_DEFAULT_BUS_RETRIES,
                                     readTimeoutMs,
                                     _debugEnable,
                                     afterReqDelayMs);
    if (!ok) {
      continue;
    }

    _lastParsedFrame = true;

    const int16_t rawTemperature = (int16_t)(((uint16_t)response[3] << 8) | response[4]);
    const uint32_t rawConductivity =
        ((uint32_t)response[5] << 24) |
        ((uint32_t)response[6] << 16) |
        ((uint32_t)response[7] << 8) |
        (uint32_t)response[8];

    water_temperature_C = (double)rawTemperature / 10.0;
    conductivity_raw = rawConductivity;
    conductivity_uS_cm = scaledConductivity(rawConductivity);

    logParsedJXBSWaterConductivity(_bus.getLogger(),
                                   _debugEnable,
                                   rawTemperature,
                                   rawConductivity,
                                   water_temperature_C,
                                   conductivity_uS_cm);

    if (validateTemperatureConductivity()) {
      return true;
    }

    if (_bus.getLogger() && _debugEnable) {
      _bus.getLogger()->println(F("[DRV][JXBS_WaterConductivity] Range check fail: temperature/conductivity"), true);
    }
  }

  return false;
}

bool JXBS_WaterConductivity::readTemperature(uint8_t driverRetries,
                                             uint16_t readTimeoutMs,
                                             uint16_t afterReqDelayMs) {
  if (driverRetries == 0) driverRetries = 1;
  _lastParsedFrame = false;

  for (uint8_t attempt = 1; attempt <= driverRetries; ++attempt) {
    uint8_t request[READ_ONE_REQUEST_SIZE] = {
      _address, 0x03, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00
    };

    uint8_t response[READ_ONE_RESPONSE_SIZE] = {0};
    const uint8_t check[READ_CHECK_SIZE] = {_address, 0x03, 0x02};

    const bool ok = _bus.SendRequest(request,
                                     READ_ONE_REQUEST_SIZE,
                                     response,
                                     READ_ONE_RESPONSE_SIZE,
                                     check,
                                     READ_CHECK_SIZE,
                                     SENSOR_DEFAULT_BUS_RETRIES,
                                     readTimeoutMs,
                                     _debugEnable,
                                     afterReqDelayMs);
    if (!ok) {
      continue;
    }

    _lastParsedFrame = true;

    const int16_t rawTemperature = (int16_t)(((uint16_t)response[3] << 8) | response[4]);
    water_temperature_C = (double)rawTemperature / 10.0;

    logParsedWaterTemperature(_bus.getLogger(),
                              _debugEnable,
                              rawTemperature,
                              water_temperature_C);

    if (validateTemperature()) {
      return true;
    }

    if (_bus.getLogger() && _debugEnable) {
      _bus.getLogger()->println(F("[DRV][JXBS_WaterConductivity] Range check fail: temperature"), true);
    }
  }

  return false;
}

bool JXBS_WaterConductivity::readConductivity(uint8_t driverRetries,
                                              uint16_t readTimeoutMs,
                                              uint16_t afterReqDelayMs) {
  if (driverRetries == 0) driverRetries = 1;
  _lastParsedFrame = false;

  for (uint8_t attempt = 1; attempt <= driverRetries; ++attempt) {
    uint8_t request[READ_EC_REQUEST_SIZE] = {
      _address, 0x03, 0x00, 0x02, 0x00, 0x02, 0x00, 0x00
    };

    uint8_t response[READ_EC_RESPONSE_SIZE] = {0};
    const uint8_t check[READ_CHECK_SIZE] = {_address, 0x03, 0x04};

    const bool ok = _bus.SendRequest(request,
                                     READ_EC_REQUEST_SIZE,
                                     response,
                                     READ_EC_RESPONSE_SIZE,
                                     check,
                                     READ_CHECK_SIZE,
                                     SENSOR_DEFAULT_BUS_RETRIES,
                                     readTimeoutMs,
                                     _debugEnable,
                                     afterReqDelayMs);
    if (!ok) {
      continue;
    }

    _lastParsedFrame = true;

    const uint32_t rawConductivity =
        ((uint32_t)response[3] << 24) |
        ((uint32_t)response[4] << 16) |
        ((uint32_t)response[5] << 8) |
        (uint32_t)response[6];

    conductivity_raw = rawConductivity;
    conductivity_uS_cm = scaledConductivity(rawConductivity);

    logParsedConductivity(_bus.getLogger(),
                          _debugEnable,
                          rawConductivity,
                          conductivity_uS_cm);

    if (validateConductivity()) {
      return true;
    }

    if (_bus.getLogger() && _debugEnable) {
      _bus.getLogger()->println(F("[DRV][JXBS_WaterConductivity] Range check fail: conductivity"), true);
    }
  }

  return false;
}

bool JXBS_WaterConductivity::readData() {
  markReadTime(millis());

  const uint8_t driverRetries = SENSOR_DEFAULT_DRIVER_RETRIES;
  bool gotAnyValidFrame = false;

  for (uint8_t attempt = 1; attempt <= driverRetries; ++attempt) {
    if (readTemperatureConductivity(1, SENSOR_DEFAULT_READ_TIMEOUT_MS, SENSOR_DEFAULT_AFTER_REQ_MS)) {
      markSuccess();
      return true;
    }

    if (_lastParsedFrame) {
      gotAnyValidFrame = true;
    }
  }

  if (!gotAnyValidFrame) {
    setFallbackValues();
  }

  markFailure();
  return false;
}

bool JXBS_WaterConductivity::changeAddress(uint8_t newAddress,
                                           uint8_t maxRetries,
                                           uint16_t readTimeoutMs,
                                           uint16_t afterReqDelayMs) {
  if (newAddress == 0 || newAddress > 247) {
    return false;
  }
  if (maxRetries == 0) maxRetries = 1;

  uint8_t request[8] = {_address, 0x06, 0x01, 0x00, 0x00, newAddress, 0x00, 0x00};
  const uint8_t oldAddress = _address;

  for (uint8_t attempt = 1; attempt <= maxRetries; ++attempt) {
    _bus.CRC_Calc(request, sizeof(request), _debugEnable);
    _bus.Request_RS485(request, sizeof(request), afterReqDelayMs, _debugEnable);

    const size_t bytesRead = _bus.Read_RS485(readTimeoutMs, _debugEnable);
    const uint8_t* raw = _bus.rawData();

    if (bytesRead >= 8 && raw) {
      for (size_t offset = 0; offset <= (bytesRead - 8); ++offset) {
        if (responseIsJXBSWaterConductivityAddressChange(&raw[offset], oldAddress, newAddress)) {
          if (_bus.getLogger() && _debugEnable) {
            _bus.getLogger()->print(F("[DRV][JXBS_WaterConductivity] Address-change response came from 0x"), true);
            _bus.getLogger()->print((unsigned int)raw[offset], true, "", HEX);
            _bus.getLogger()->println(raw[offset] == newAddress ? F(" (new address)") : F(" (old address)"), true);
          }

          _address = newAddress;
          return true;
        }
      }
    }

    delay(100UL * attempt);
  }

  return false;
}

uint8_t JXBS_WaterConductivity::scanForAddress(uint8_t startAddr,
                                               uint8_t endAddr,
                                               uint16_t readTimeoutMs,
                                               uint16_t afterReqDelayMs) {
  if (startAddr == 0) startAddr = 1;
  if (endAddr > 247) endAddr = 247;
  if (startAddr > endAddr) return 0;

  for (uint16_t addr = startAddr; addr <= endAddr; ++addr) {
    uint8_t request[READ_ALL_REQUEST_SIZE] = {
      (uint8_t)addr, 0x03, 0x00, 0x01, 0x00, 0x03, 0x00, 0x00
    };

    uint8_t response[READ_ALL_RESPONSE_SIZE] = {0};
    const uint8_t check[READ_CHECK_SIZE] = {(uint8_t)addr, 0x03, 0x06};

    const bool found = _bus.SendRequest(request,
                                        READ_ALL_REQUEST_SIZE,
                                        response,
                                        READ_ALL_RESPONSE_SIZE,
                                        check,
                                        READ_CHECK_SIZE,
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
