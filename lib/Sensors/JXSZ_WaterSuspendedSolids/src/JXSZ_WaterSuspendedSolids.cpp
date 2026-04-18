#include "JXSZ_WaterSuspendedSolids.h"
#include <math.h>

static bool responseIsJXSZWaterSuspendedSolidsAddressChange(const uint8_t* frame,
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

static void logParsedJXSZWaterSuspendedSolids(PrintController* log,
                                              bool debug,
                                              int16_t rawTemperature,
                                              uint16_t rawSuspendedSolids,
                                              double temperature,
                                              double suspendedSolids) {
  if (!log || !debug) return;

  log->println(F("[DRV][JXSZ_WaterSuspendedSolids] Parsed temperature + suspended solids:"), true);

  log->print(F("  temperature raw = "), true);
  log->print((int)rawTemperature, true, "", DEC);
  log->print(F(" | temperature = "), true);
  log->print(temperature, true, " C", 1);
  log->println("", true);

  log->print(F("  suspended solids raw = "), true);
  log->print((unsigned int)rawSuspendedSolids, true, "", DEC);
  log->print(F(" | suspended solids = "), true);
  log->print(suspendedSolids, true, " mg/L", 2);
  log->println("", true);
}

static void logParsedJXSZWaterTemperature(PrintController* log,
                                          bool debug,
                                          int16_t rawTemperature,
                                          double temperature) {
  if (!log || !debug) return;

  log->println(F("[DRV][JXSZ_WaterSuspendedSolids] Parsed temperature:"), true);
  log->print(F("  temperature raw = "), true);
  log->print((int)rawTemperature, true, "", DEC);
  log->print(F(" | temperature = "), true);
  log->print(temperature, true, " C", 1);
  log->println("", true);
}

static void logParsedJXSZSuspendedSolids(PrintController* log,
                                         bool debug,
                                         uint16_t rawSuspendedSolids,
                                         double suspendedSolids) {
  if (!log || !debug) return;

  log->println(F("[DRV][JXSZ_WaterSuspendedSolids] Parsed suspended solids:"), true);
  log->print(F("  suspended solids raw = "), true);
  log->print((unsigned int)rawSuspendedSolids, true, "", DEC);
  log->print(F(" | suspended solids = "), true);
  log->print(suspendedSolids, true, " mg/L", 2);
  log->println("", true);
}

JXSZ_WaterSuspendedSolids::JXSZ_WaterSuspendedSolids(RS485Bus& bus,
                                                     const char* sensorId,
                                                     uint8_t address,
                                                     bool debugEnable,
                                                     double suspendedSolidsScaleDivisor,
                                                     double maxSuspendedSolids_mg_L,
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
      suspended_solids_raw(0),
      suspended_solids_mg_L(0.0),
      _bus(bus),
      _suspendedSolidsScaleDivisor(suspendedSolidsScaleDivisor > 0.0 ? suspendedSolidsScaleDivisor : 10.0),
      _maxSuspendedSolids_mg_L(maxSuspendedSolids_mg_L > 0.0 ? maxSuspendedSolids_mg_L : 20000.0),
      _lastParsedFrame(false) {}

void JXSZ_WaterSuspendedSolids::setFallbackValues() {
  water_temperature_C = -99.0;
  suspended_solids_raw = 0;
  suspended_solids_mg_L = -99.0;
}

void JXSZ_WaterSuspendedSolids::setSuspendedSolidsScaleDivisor(double divisor) {
  if (divisor > 0.0) {
    _suspendedSolidsScaleDivisor = divisor;
  }
}

void JXSZ_WaterSuspendedSolids::setMaxSuspendedSolids(double maxSuspendedSolids_mg_L) {
  if (maxSuspendedSolids_mg_L > 0.0) {
    _maxSuspendedSolids_mg_L = maxSuspendedSolids_mg_L;
  }
}

double JXSZ_WaterSuspendedSolids::scaledSuspendedSolids(uint16_t raw) const {
  return (double)raw / _suspendedSolidsScaleDivisor;
}

bool JXSZ_WaterSuspendedSolids::validateTemperature() const {
  if (isnan(water_temperature_C)) return false;
  if (water_temperature_C < 0.0 || water_temperature_C > 50.0) return false;
  return true;
}

bool JXSZ_WaterSuspendedSolids::validateSuspendedSolids() const {
  if (isnan(suspended_solids_mg_L)) return false;
  if (suspended_solids_raw == 0xFFFFU || suspended_solids_raw == 0x7FFFU ||
      suspended_solids_raw == 0x8000U) {
    return false;
  }
  if (suspended_solids_mg_L < 0.0 || suspended_solids_mg_L > _maxSuspendedSolids_mg_L) {
    return false;
  }
  return true;
}

bool JXSZ_WaterSuspendedSolids::validateTemperatureSuspendedSolids() const {
  return validateTemperature() && validateSuspendedSolids();
}

bool JXSZ_WaterSuspendedSolids::readTemperatureSuspendedSolids(uint8_t driverRetries,
                                                               uint16_t readTimeoutMs,
                                                               uint16_t afterReqDelayMs) {
  if (driverRetries == 0) driverRetries = 1;
  _lastParsedFrame = false;

  for (uint8_t attempt = 1; attempt <= driverRetries; ++attempt) {
    uint8_t request[READ_TWO_REQUEST_SIZE] = {
      _address, 0x03, 0x00, 0x01, 0x00, 0x02, 0x00, 0x00
    };

    uint8_t response[READ_TWO_RESPONSE_SIZE] = {0};
    const uint8_t check[READ_CHECK_SIZE] = {_address, 0x03, 0x04};

    const bool ok = _bus.SendRequest(request,
                                     READ_TWO_REQUEST_SIZE,
                                     response,
                                     READ_TWO_RESPONSE_SIZE,
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
    const uint16_t rawSuspendedSolids = ((uint16_t)response[5] << 8) | response[6];

    water_temperature_C = (double)rawTemperature / 10.0;
    suspended_solids_raw = rawSuspendedSolids;
    suspended_solids_mg_L = scaledSuspendedSolids(rawSuspendedSolids);

    logParsedJXSZWaterSuspendedSolids(_bus.getLogger(),
                                      _debugEnable,
                                      rawTemperature,
                                      rawSuspendedSolids,
                                      water_temperature_C,
                                      suspended_solids_mg_L);

    if (validateTemperatureSuspendedSolids()) {
      return true;
    }

    if (_bus.getLogger() && _debugEnable) {
      _bus.getLogger()->println(F("[DRV][JXSZ_WaterSuspendedSolids] Range check fail: temperature/suspended solids"), true);
    }
  }

  return false;
}

bool JXSZ_WaterSuspendedSolids::readTemperature(uint8_t driverRetries,
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

    logParsedJXSZWaterTemperature(_bus.getLogger(),
                                  _debugEnable,
                                  rawTemperature,
                                  water_temperature_C);

    if (validateTemperature()) {
      return true;
    }

    if (_bus.getLogger() && _debugEnable) {
      _bus.getLogger()->println(F("[DRV][JXSZ_WaterSuspendedSolids] Range check fail: temperature"), true);
    }
  }

  return false;
}

bool JXSZ_WaterSuspendedSolids::readSuspendedSolids(uint8_t driverRetries,
                                                    uint16_t readTimeoutMs,
                                                    uint16_t afterReqDelayMs) {
  if (driverRetries == 0) driverRetries = 1;
  _lastParsedFrame = false;

  for (uint8_t attempt = 1; attempt <= driverRetries; ++attempt) {
    uint8_t request[READ_ONE_REQUEST_SIZE] = {
      _address, 0x03, 0x00, 0x02, 0x00, 0x01, 0x00, 0x00
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

    const uint16_t rawSuspendedSolids = ((uint16_t)response[3] << 8) | response[4];
    suspended_solids_raw = rawSuspendedSolids;
    suspended_solids_mg_L = scaledSuspendedSolids(rawSuspendedSolids);

    logParsedJXSZSuspendedSolids(_bus.getLogger(),
                                 _debugEnable,
                                 rawSuspendedSolids,
                                 suspended_solids_mg_L);

    if (validateSuspendedSolids()) {
      return true;
    }

    if (_bus.getLogger() && _debugEnable) {
      _bus.getLogger()->println(F("[DRV][JXSZ_WaterSuspendedSolids] Range check fail: suspended solids"), true);
    }
  }

  return false;
}

bool JXSZ_WaterSuspendedSolids::readData() {
  markReadTime(millis());

  const uint8_t driverRetries = SENSOR_DEFAULT_DRIVER_RETRIES;
  bool gotAnyValidFrame = false;

  for (uint8_t attempt = 1; attempt <= driverRetries; ++attempt) {
    if (readTemperatureSuspendedSolids(1, SENSOR_DEFAULT_READ_TIMEOUT_MS, SENSOR_DEFAULT_AFTER_REQ_MS)) {
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

bool JXSZ_WaterSuspendedSolids::changeAddress(uint8_t newAddress,
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
        if (responseIsJXSZWaterSuspendedSolidsAddressChange(&raw[offset], oldAddress, newAddress)) {
          if (_bus.getLogger() && _debugEnable) {
            _bus.getLogger()->print(F("[DRV][JXSZ_WaterSuspendedSolids] Address-change response came from 0x"), true);
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

uint8_t JXSZ_WaterSuspendedSolids::scanForAddress(uint8_t startAddr,
                                                  uint8_t endAddr,
                                                  uint16_t readTimeoutMs,
                                                  uint16_t afterReqDelayMs) {
  if (startAddr == 0) startAddr = 1;
  if (endAddr > 247) endAddr = 247;
  if (startAddr > endAddr) return 0;

  for (uint16_t addr = startAddr; addr <= endAddr; ++addr) {
    uint8_t request[READ_ONE_REQUEST_SIZE] = {
      (uint8_t)addr, 0x03, 0x00, 0x02, 0x00, 0x01, 0x00, 0x00
    };

    uint8_t response[READ_ONE_RESPONSE_SIZE] = {0};
    const uint8_t check[READ_CHECK_SIZE] = {(uint8_t)addr, 0x03, 0x02};

    const bool found = _bus.SendRequest(request,
                                        READ_ONE_REQUEST_SIZE,
                                        response,
                                        READ_ONE_RESPONSE_SIZE,
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
