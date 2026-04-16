#include "RikaSoilSensor3in1.h"

static void logParsedRikaSoil3in1(PrintController* log,
                                  bool debug,
                                  uint8_t tempHi,
                                  uint8_t tempLo,
                                  uint8_t vwcHi,
                                  uint8_t vwcLo,
                                  uint8_t ecHi,
                                  uint8_t ecLo,
                                  int16_t rawTemp,
                                  uint16_t rawVwc,
                                  uint16_t rawEc,
                                  double temp,
                                  double vwc,
                                  double ec) {
  if (!log || !debug) return;

  log->println(F("[DRV][RikaSoil3in1] Parsed response:"), true);

  log->print(F("  bytes [3],[4] -> temperature raw = 0x"), true);
  log->print((unsigned int)tempHi, true, "", HEX);
  log->print((unsigned int)tempLo, true, " ", HEX);
  log->print(F("| combined = "), true);
  log->print((int)rawTemp, true, "", DEC);
  log->print(F(" | temperature = "), true);
  log->print(temp, true, " C", 1);
  log->println("", true);

  log->print(F("  bytes [5],[6] -> VWC raw = 0x"), true);
  log->print((unsigned int)vwcHi, true, "", HEX);
  log->print((unsigned int)vwcLo, true, " ", HEX);
  log->print(F("| combined = "), true);
  log->print((unsigned int)rawVwc, true, "", DEC);
  log->print(F(" | VWC = "), true);
  log->print(vwc, true, " %", 1);
  log->println("", true);

  log->print(F("  bytes [7],[8] -> EC raw = 0x"), true);
  log->print((unsigned int)ecHi, true, "", HEX);
  log->print((unsigned int)ecLo, true, " ", HEX);
  log->print(F("| combined = "), true);
  log->print((unsigned int)rawEc, true, "", DEC);
  log->print(F(" | EC = "), true);
  log->print(ec, true, " mS/cm", 4);
  log->println("", true);
}

RikaSoilSensor3in1::RikaSoilSensor3in1(RS485Bus& bus,
                                       const char* sensorId,
                                       uint8_t address,
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
      soil_temp(0.0),
      soil_vwc(0.0),
      soil_ec(0.0),
      epsilon(0.0),
      currentSoilType(SOIL_UNKNOWN) {}

void RikaSoilSensor3in1::setFallbackValues() {
  soil_temp = -99.0;
  soil_vwc  = -99.0;
  soil_ec   = -99.0;
  epsilon   = -99.0;
  currentSoilType = SOIL_UNKNOWN;
}

bool RikaSoilSensor3in1::readData() {
  markReadTime(millis());

  const uint8_t DRIVER_RETRIES = SENSOR_DEFAULT_DRIVER_RETRIES;
  bool gotAnyValidFrame = false;

  for (uint8_t driverAttempt = 1; driverAttempt <= DRIVER_RETRIES; ++driverAttempt) {
    uint8_t request[MAIN_REQUEST_SIZE] = {
      _address, 0x03, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00
    };

    uint8_t response[MAIN_RESPONSE_SIZE] = {0};
    const uint8_t check[MAIN_CHECK_SIZE] = {_address, 0x03, 0x06};

    const bool ok = _bus.SendRequest(request,
                                     MAIN_REQUEST_SIZE,
                                     response,
                                     MAIN_RESPONSE_SIZE,
                                     check,
                                     MAIN_CHECK_SIZE,
                                     SENSOR_DEFAULT_BUS_RETRIES,
                                     SENSOR_DEFAULT_READ_TIMEOUT_MS,
                                     _debugEnable,
                                     SENSOR_DEFAULT_AFTER_REQ_MS);

    if (!ok) {
      continue;
    }

    gotAnyValidFrame = true;

    const uint8_t tempHi = response[3];
    const uint8_t tempLo = response[4];
    const uint8_t vwcHi = response[5];
    const uint8_t vwcLo = response[6];
    const uint8_t ecHi = response[7];
    const uint8_t ecLo = response[8];

    const int16_t rawTemp = (int16_t)(((uint16_t)tempHi << 8) | tempLo);
    const uint16_t rawVwc = ((uint16_t)vwcHi << 8) | vwcLo;
    const uint16_t rawEc  = ((uint16_t)ecHi << 8) | ecLo;

    soil_temp = (double)rawTemp / 10.0;
    soil_vwc  = (double)rawVwc / 10.0;
    soil_ec   = (double)rawEc / 1000.0;

    logParsedRikaSoil3in1(_bus.getLogger(),
                          _debugEnable,
                          tempHi,
                          tempLo,
                          vwcHi,
                          vwcLo,
                          ecHi,
                          ecLo,
                          rawTemp,
                          rawVwc,
                          rawEc,
                          soil_temp,
                          soil_vwc,
                          soil_ec);

    if (soil_temp < -40.0 || soil_temp > 80.0) {
      if (_bus.getLogger() && _debugEnable) {
        _bus.getLogger()->println(F("[DRV][RikaSoil3in1] Range check fail: temperature is outside allowed range"), true);
      }
      continue;
    }

    if (soil_vwc < 0.0 || soil_vwc > 100.0) {
      if (_bus.getLogger() && _debugEnable) {
        _bus.getLogger()->println(F("[DRV][RikaSoil3in1] Range check fail: VWC is outside allowed range"), true);
      }
      continue;
    }

    if (soil_ec < 0.0 || soil_ec > 50.0) {
      if (_bus.getLogger() && _debugEnable) {
        _bus.getLogger()->println(F("[DRV][RikaSoil3in1] Range check fail: EC is outside allowed range"), true);
      }
      continue;
    }

    markSuccess();
    return true;
  }

  if (!gotAnyValidFrame) {
    setFallbackValues();
  }

  markFailure();
  return false;
}

bool RikaSoilSensor3in1::changeAddress(uint8_t newAddress,
                                       uint8_t maxRetries,
                                       uint16_t readTimeoutMs,
                                       uint16_t afterReqDelayMs) {
  if (newAddress == 0 || newAddress > 247) {
    return false;
  }

  // Rika soil address setup writes the new Modbus node address to register
  // 0x0200 using the manufacturer's broadcast-like address 0xFE. Keep only
  // the target sensor connected while this command is enabled.
  uint8_t request[8] = {0xFE, 0x06, 0x02, 0x00, 0x00, newAddress, 0x00, 0x00};
  uint8_t response[8] = {0};
  const uint8_t check[4] = {0xFE, 0x06, 0x02, 0x00};

  const bool ok = _bus.SendRequest(request,
                                   8,
                                   response,
                                   8,
                                   check,
                                   4,
                                   maxRetries,
                                   readTimeoutMs,
                                   _debugEnable,
                                   afterReqDelayMs);

  if (ok) {
    _address = newAddress;
  }

  return ok;
}

uint8_t RikaSoilSensor3in1::scanForAddress(uint8_t startAddr,
                                           uint8_t endAddr,
                                           uint16_t readTimeoutMs,
                                           uint16_t afterReqDelayMs) {
  if (startAddr == 0) startAddr = 1;
  if (endAddr > 247) endAddr = 247;
  if (startAddr > endAddr) return 0;

  for (uint16_t addr = startAddr; addr <= endAddr; ++addr) {
    uint8_t request[MAIN_REQUEST_SIZE] = {
      (uint8_t)addr, 0x03, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00
    };

    uint8_t response[MAIN_RESPONSE_SIZE] = {0};
    const uint8_t check[MAIN_CHECK_SIZE] = {(uint8_t)addr, 0x03, 0x06};

    const bool found = _bus.SendRequest(request,
                                        MAIN_REQUEST_SIZE,
                                        response,
                                        MAIN_RESPONSE_SIZE,
                                        check,
                                        MAIN_CHECK_SIZE,
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

bool RikaSoilSensor3in1::readSoilType(SoilType &type,
                                      uint8_t driverRetries,
                                      uint16_t readTimeoutMs,
                                      uint16_t afterReqDelayMs) {
  if (driverRetries == 0) driverRetries = 1;

  for (uint8_t driverAttempt = 1; driverAttempt <= driverRetries; ++driverAttempt) {
    uint8_t request[8] = {_address, 0x03, 0x00, 0x20, 0x00, 0x01, 0x00, 0x00};
    uint8_t response[7] = {0};
    const uint8_t check[3] = {_address, 0x03, 0x02};

    const bool ok = _bus.SendRequest(request,
                                     8,
                                     response,
                                     7,
                                     check,
                                     3,
                                     SENSOR_DEFAULT_BUS_RETRIES,
                                     readTimeoutMs,
                                     _debugEnable,
                                     afterReqDelayMs);
    if (!ok) {
      continue;
    }

    const uint16_t rawType = ((uint16_t)response[3] << 8) | response[4];

    switch (rawType) {
      case 0: type = SOIL_MINERAL; break;
      case 1: type = SOIL_SANDY;   break;
      case 2: type = SOIL_CLAY;    break;
      case 3: type = SOIL_ORGANIC; break;
      default: type = SOIL_UNKNOWN; break;
    }

    currentSoilType = type;
    return true;
  }

  type = SOIL_UNKNOWN;
  currentSoilType = SOIL_UNKNOWN;
  return false;
}

bool RikaSoilSensor3in1::setSoilType(SoilType type,
                                     uint8_t driverRetries,
                                     uint16_t readTimeoutMs,
                                     uint16_t afterReqDelayMs) {
  if (driverRetries == 0) driverRetries = 1;

  for (uint8_t driverAttempt = 1; driverAttempt <= driverRetries; ++driverAttempt) {
    uint8_t request[8] = {_address, 0x06, 0x00, 0x20, 0x00, (uint8_t)type, 0x00, 0x00};
    uint8_t response[8] = {0};
    const uint8_t check[4] = {_address, 0x06, 0x00, 0x20};

    const bool ok = _bus.SendRequest(request,
                                     8,
                                     response,
                                     8,
                                     check,
                                     4,
                                     SENSOR_DEFAULT_BUS_RETRIES,
                                     readTimeoutMs,
                                     _debugEnable,
                                     afterReqDelayMs);
    if (ok) {
      currentSoilType = type;
      return true;
    }
  }

  return false;
}

bool RikaSoilSensor3in1::readEpsilon(double &value,
                                     uint8_t driverRetries,
                                     uint16_t readTimeoutMs,
                                     uint16_t afterReqDelayMs) {
  if (driverRetries == 0) driverRetries = 1;

  for (uint8_t driverAttempt = 1; driverAttempt <= driverRetries; ++driverAttempt) {
    uint8_t request[8] = {_address, 0x04, 0x00, 0x05, 0x00, 0x01, 0x00, 0x00};
    uint8_t response[7] = {0};
    const uint8_t check[3] = {_address, 0x04, 0x02};

    const bool ok = _bus.SendRequest(request,
                                     8,
                                     response,
                                     7,
                                     check,
                                     3,
                                     SENSOR_DEFAULT_BUS_RETRIES,
                                     readTimeoutMs,
                                     _debugEnable,
                                     afterReqDelayMs);
    if (!ok) {
      continue;
    }

    const uint16_t raw = ((uint16_t)response[3] << 8) | response[4];
    value = (double)raw / 100.0;
    epsilon = value;
    return true;
  }

  value = -99.0;
  epsilon = -99.0;
  return false;
}

bool RikaSoilSensor3in1::setCompensationCoeff(uint16_t regAddress,
                                              uint16_t coeffValue,
                                              uint8_t driverRetries,
                                              uint16_t readTimeoutMs,
                                              uint16_t afterReqDelayMs) {
  if (driverRetries == 0) driverRetries = 1;

  const uint8_t regHigh = (uint8_t)((regAddress >> 8) & 0xFF);
  const uint8_t regLow  = (uint8_t)(regAddress & 0xFF);

  for (uint8_t driverAttempt = 1; driverAttempt <= driverRetries; ++driverAttempt) {
    uint8_t request[8] = {
      _address, 0x06, regHigh, regLow,
      (uint8_t)((coeffValue >> 8) & 0xFF),
      (uint8_t)(coeffValue & 0xFF),
      0x00, 0x00
    };

    uint8_t response[8] = {0};
    const uint8_t check[4] = {_address, 0x06, regHigh, regLow};

    const bool ok = _bus.SendRequest(request,
                                     8,
                                     response,
                                     8,
                                     check,
                                     4,
                                     SENSOR_DEFAULT_BUS_RETRIES,
                                     readTimeoutMs,
                                     _debugEnable,
                                     afterReqDelayMs);
    if (ok) {
      return true;
    }
  }

  return false;
}

bool RikaSoilSensor3in1::readCompensationCoeff(uint16_t regAddress,
                                               double &coeffValue,
                                               uint8_t driverRetries,
                                               uint16_t readTimeoutMs,
                                               uint16_t afterReqDelayMs) {
  if (driverRetries == 0) driverRetries = 1;

  const uint8_t regHigh = (uint8_t)((regAddress >> 8) & 0xFF);
  const uint8_t regLow  = (uint8_t)(regAddress & 0xFF);

  for (uint8_t driverAttempt = 1; driverAttempt <= driverRetries; ++driverAttempt) {
    uint8_t request[8] = {_address, 0x03, regHigh, regLow, 0x00, 0x01, 0x00, 0x00};
    uint8_t response[7] = {0};
    const uint8_t check[3] = {_address, 0x03, 0x02};

    const bool ok = _bus.SendRequest(request,
                                     8,
                                     response,
                                     7,
                                     check,
                                     3,
                                     SENSOR_DEFAULT_BUS_RETRIES,
                                     readTimeoutMs,
                                     _debugEnable,
                                     afterReqDelayMs);
    if (!ok) {
      continue;
    }

    const uint16_t raw = ((uint16_t)response[3] << 8) | response[4];
    coeffValue = (double)raw / 100.0;
    return true;
  }

  coeffValue = -99.0;
  return false;
}

const char* RikaSoilSensor3in1::soilTypeToString(SoilType type) {
  switch (type) {
    case SOIL_MINERAL: return "Mineral Soil";
    case SOIL_SANDY:   return "Sandy Soil";
    case SOIL_CLAY:    return "Clay Soil";
    case SOIL_ORGANIC: return "Organic Soil";
    default:           return "Unknown";
  }
}
