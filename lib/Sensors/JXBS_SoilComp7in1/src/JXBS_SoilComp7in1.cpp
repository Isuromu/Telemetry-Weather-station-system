#include "JXBS_SoilComp7in1.h"

static void logParsedJXBS7_MoistTemp(PrintController* log,
                                     bool debug,
                                     uint8_t moistHi,
                                     uint8_t moistLo,
                                     uint8_t tempHi,
                                     uint8_t tempLo,
                                     uint16_t rawMoisture,
                                     int16_t rawTemp,
                                     double moisture,
                                     double temp) {
  if (!log || !debug) return;

  log->println(F("[DRV][JXBS_SoilComp7in1] Parsed moisture + temperature:"), true);

  log->print(F("  bytes [3],[4] -> moisture raw = 0x"), true);
  log->print((unsigned int)moistHi, true, "", HEX);
  log->print((unsigned int)moistLo, true, " ", HEX);
  log->print(F("| combined = "), true);
  log->print((unsigned int)rawMoisture, true, "", DEC);
  log->print(F(" | moisture = "), true);
  log->print(moisture, true, " %", 1);
  log->println("", true);

  log->print(F("  bytes [5],[6] -> temperature raw = 0x"), true);
  log->print((unsigned int)tempHi, true, "", HEX);
  log->print((unsigned int)tempLo, true, " ", HEX);
  log->print(F("| combined = "), true);
  log->print((int)rawTemp, true, "", DEC);
  log->print(F(" | temperature = "), true);
  log->print(temp, true, " C", 1);
  log->println("", true);
}

static void logParsedJXBS7_EC(PrintController* log,
                              bool debug,
                              uint8_t ecHi,
                              uint8_t ecLo,
                              uint16_t rawEc,
                              double ec) {
  if (!log || !debug) return;

  log->println(F("[DRV][JXBS_SoilComp7in1] Parsed conductivity:"), true);
  log->print(F("  bytes [3],[4] -> EC raw = 0x"), true);
  log->print((unsigned int)ecHi, true, "", HEX);
  log->print((unsigned int)ecLo, true, " ", HEX);
  log->print(F("| combined = "), true);
  log->print((unsigned int)rawEc, true, "", DEC);
  log->print(F(" | EC = "), true);
  log->print(ec, true, " us/cm", 0);
  log->println("", true);
}

static void logParsedJXBS7_PH(PrintController* log,
                              bool debug,
                              uint8_t phHi,
                              uint8_t phLo,
                              uint16_t rawPh,
                              double ph) {
  if (!log || !debug) return;

  log->println(F("[DRV][JXBS_SoilComp7in1] Parsed pH:"), true);
  log->print(F("  bytes [3],[4] -> pH raw = 0x"), true);
  log->print((unsigned int)phHi, true, "", HEX);
  log->print((unsigned int)phLo, true, " ", HEX);
  log->print(F("| combined = "), true);
  log->print((unsigned int)rawPh, true, "", DEC);
  log->print(F(" | pH = "), true);
  log->print(ph, true, "", 2);
  log->println("", true);
}

static void logParsedJXBS7_NPK(PrintController* log,
                               bool debug,
                               uint8_t nHi,
                               uint8_t nLo,
                               uint8_t pHi,
                               uint8_t pLo,
                               uint8_t kHi,
                               uint8_t kLo,
                               uint16_t rawN,
                               uint16_t rawP,
                               uint16_t rawK) {
  if (!log || !debug) return;

  log->println(F("[DRV][JXBS_SoilComp7in1] Parsed NPK:"), true);

  log->print(F("  bytes [3],[4] -> N raw = 0x"), true);
  log->print((unsigned int)nHi, true, "", HEX);
  log->print((unsigned int)nLo, true, " ", HEX);
  log->print(F("| combined = "), true);
  log->print((unsigned int)rawN, true, " mg/kg", DEC);
  log->println("", true);

  log->print(F("  bytes [5],[6] -> P raw = 0x"), true);
  log->print((unsigned int)pHi, true, "", HEX);
  log->print((unsigned int)pLo, true, " ", HEX);
  log->print(F("| combined = "), true);
  log->print((unsigned int)rawP, true, " mg/kg", DEC);
  log->println("", true);

  log->print(F("  bytes [7],[8] -> K raw = 0x"), true);
  log->print((unsigned int)kHi, true, "", HEX);
  log->print((unsigned int)kLo, true, " ", HEX);
  log->print(F("| combined = "), true);
  log->print((unsigned int)rawK, true, " mg/kg", DEC);
  log->println("", true);
}

JXBS_SoilComp7in1::JXBS_SoilComp7in1(RS485Bus& bus,
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
      _lastParsedFrame(false),
      soil_moisture(0.0),
      soil_temp(0.0),
      soil_ec(0.0),
      soil_ph(0.0),
      soil_nitrogen(0),
      soil_phosphorus(0),
      soil_potassium(0) {}

void JXBS_SoilComp7in1::setFallbackValues() {
  soil_moisture = -99.0;
  soil_temp = -99.0;
  soil_ec = -99.0;
  soil_ph = -99.0;
  soil_nitrogen = 0xFFFF;
  soil_phosphorus = 0xFFFF;
  soil_potassium = 0xFFFF;
}

bool JXBS_SoilComp7in1::validateMoistureTemperature() const {
  if (soil_moisture < 0.0 || soil_moisture > 100.0) return false;
  if (soil_temp < -40.0 || soil_temp > 80.0) return false;
  return true;
}

bool JXBS_SoilComp7in1::validateConductivity() const {
  if (soil_ec < 0.0 || soil_ec > 10000.0) return false;
  return true;
}

bool JXBS_SoilComp7in1::validatePH() const {
  if (soil_ph < 3.0 || soil_ph > 9.0) return false;
  return true;
}

bool JXBS_SoilComp7in1::validateNPK() const {
  if (soil_nitrogen > 1999) return false;
  if (soil_phosphorus > 1999) return false;
  if (soil_potassium > 1999) return false;
  return true;
}

bool JXBS_SoilComp7in1::readMoistureTemperature(uint8_t driverRetries,
                                                uint16_t readTimeoutMs,
                                                uint16_t afterReqDelayMs) {
  if (driverRetries == 0) driverRetries = 1;
  _lastParsedFrame = false;

  for (uint8_t attempt = 1; attempt <= driverRetries; ++attempt) {
    uint8_t request[8] = {_address, 0x03, 0x00, 0x12, 0x00, 0x02, 0x00, 0x00};
    uint8_t response[9] = {0};
    const uint8_t check[3] = {_address, 0x03, 0x04};

    const bool ok = _bus.SendRequest(request,
                                     8,
                                     response,
                                     9,
                                     check,
                                     3,
                                     SENSOR_DEFAULT_BUS_RETRIES,
                                     readTimeoutMs,
                                     _debugEnable,
                                     afterReqDelayMs);
    if (!ok) {
      continue;
    }

    _lastParsedFrame = true;

    const uint8_t moistHi = response[3];
    const uint8_t moistLo = response[4];
    const uint8_t tempHi = response[5];
    const uint8_t tempLo = response[6];

    const uint16_t rawMoisture = ((uint16_t)moistHi << 8) | moistLo;
    const int16_t rawTemp = (int16_t)(((uint16_t)tempHi << 8) | tempLo);

    soil_moisture = (double)rawMoisture / 10.0;
    soil_temp = (double)rawTemp / 10.0;

    logParsedJXBS7_MoistTemp(_bus.getLogger(),
                             _debugEnable,
                             moistHi,
                             moistLo,
                             tempHi,
                             tempLo,
                             rawMoisture,
                             rawTemp,
                             soil_moisture,
                             soil_temp);

    if (validateMoistureTemperature()) {
      return true;
    }

    if (_bus.getLogger() && _debugEnable) {
      _bus.getLogger()->println(F("[DRV][JXBS_SoilComp7in1] Range check fail: moisture/temperature"), true);
    }
  }

  return false;
}

bool JXBS_SoilComp7in1::readConductivity(uint8_t driverRetries,
                                         uint16_t readTimeoutMs,
                                         uint16_t afterReqDelayMs) {
  if (driverRetries == 0) driverRetries = 1;
  _lastParsedFrame = false;

  for (uint8_t attempt = 1; attempt <= driverRetries; ++attempt) {
    uint8_t request[8] = {_address, 0x03, 0x00, 0x15, 0x00, 0x01, 0x00, 0x00};
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

    _lastParsedFrame = true;

    const uint8_t ecHi = response[3];
    const uint8_t ecLo = response[4];
    const uint16_t rawEc = ((uint16_t)ecHi << 8) | ecLo;
    soil_ec = (double)rawEc;

    logParsedJXBS7_EC(_bus.getLogger(), _debugEnable, ecHi, ecLo, rawEc, soil_ec);

    if (validateConductivity()) {
      return true;
    }

    if (_bus.getLogger() && _debugEnable) {
      _bus.getLogger()->println(F("[DRV][JXBS_SoilComp7in1] Range check fail: conductivity"), true);
    }
  }

  return false;
}

bool JXBS_SoilComp7in1::readPH(uint8_t driverRetries,
                               uint16_t readTimeoutMs,
                               uint16_t afterReqDelayMs) {
  if (driverRetries == 0) driverRetries = 1;
  _lastParsedFrame = false;

  for (uint8_t attempt = 1; attempt <= driverRetries; ++attempt) {
    uint8_t request[8] = {_address, 0x03, 0x00, 0x06, 0x00, 0x01, 0x00, 0x00};
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

    _lastParsedFrame = true;

    const uint8_t phHi = response[3];
    const uint8_t phLo = response[4];
    const uint16_t rawPH = ((uint16_t)phHi << 8) | phLo;
    soil_ph = (double)rawPH / 100.0;

    logParsedJXBS7_PH(_bus.getLogger(), _debugEnable, phHi, phLo, rawPH, soil_ph);

    if (validatePH()) {
      return true;
    }

    if (_bus.getLogger() && _debugEnable) {
      _bus.getLogger()->println(F("[DRV][JXBS_SoilComp7in1] Range check fail: pH"), true);
    }
  }

  return false;
}

bool JXBS_SoilComp7in1::readNPK(uint8_t driverRetries,
                                uint16_t readTimeoutMs,
                                uint16_t afterReqDelayMs) {
  if (driverRetries == 0) driverRetries = 1;
  _lastParsedFrame = false;

  for (uint8_t attempt = 1; attempt <= driverRetries; ++attempt) {
    uint8_t request[8] = {_address, 0x03, 0x00, 0x1E, 0x00, 0x03, 0x00, 0x00};
    uint8_t response[11] = {0};
    const uint8_t check[3] = {_address, 0x03, 0x06};

    const bool ok = _bus.SendRequest(request,
                                     8,
                                     response,
                                     11,
                                     check,
                                     3,
                                     SENSOR_DEFAULT_BUS_RETRIES,
                                     readTimeoutMs,
                                     _debugEnable,
                                     afterReqDelayMs);
    if (!ok) {
      continue;
    }

    _lastParsedFrame = true;

    const uint8_t nHi = response[3];
    const uint8_t nLo = response[4];
    const uint8_t pHi = response[5];
    const uint8_t pLo = response[6];
    const uint8_t kHi = response[7];
    const uint8_t kLo = response[8];

    soil_nitrogen   = ((uint16_t)nHi << 8) | nLo;
    soil_phosphorus = ((uint16_t)pHi << 8) | pLo;
    soil_potassium  = ((uint16_t)kHi << 8) | kLo;

    logParsedJXBS7_NPK(_bus.getLogger(),
                       _debugEnable,
                       nHi,
                       nLo,
                       pHi,
                       pLo,
                       kHi,
                       kLo,
                       soil_nitrogen,
                       soil_phosphorus,
                       soil_potassium);

    if (validateNPK()) {
      return true;
    }

    if (_bus.getLogger() && _debugEnable) {
      _bus.getLogger()->println(F("[DRV][JXBS_SoilComp7in1] Range check fail: NPK"), true);
    }
  }

  return false;
}

bool JXBS_SoilComp7in1::readData() {
  markReadTime(millis());

  const uint8_t driverRetries = SENSOR_DEFAULT_DRIVER_RETRIES;
  bool gotAnyValidFrame = false;

  for (uint8_t attempt = 1; attempt <= driverRetries; ++attempt) {
    if (!readMoistureTemperature(1, SENSOR_DEFAULT_READ_TIMEOUT_MS, SENSOR_DEFAULT_AFTER_REQ_MS)) {
      if (_lastParsedFrame) gotAnyValidFrame = true;
      continue;
    }
    if (_lastParsedFrame) gotAnyValidFrame = true;

    if (!readConductivity(1, SENSOR_DEFAULT_READ_TIMEOUT_MS, SENSOR_DEFAULT_AFTER_REQ_MS)) {
      if (_lastParsedFrame) gotAnyValidFrame = true;
      continue;
    }
    if (_lastParsedFrame) gotAnyValidFrame = true;

    if (!readPH(1, SENSOR_DEFAULT_READ_TIMEOUT_MS, SENSOR_DEFAULT_AFTER_REQ_MS)) {
      if (_lastParsedFrame) gotAnyValidFrame = true;
      continue;
    }
    if (_lastParsedFrame) gotAnyValidFrame = true;

    if (!readNPK(1, SENSOR_DEFAULT_READ_TIMEOUT_MS, SENSOR_DEFAULT_AFTER_REQ_MS)) {
      if (_lastParsedFrame) gotAnyValidFrame = true;
      continue;
    }
    if (_lastParsedFrame) gotAnyValidFrame = true;

    markSuccess();
    return true;
  }

  if (!gotAnyValidFrame) {
    setFallbackValues();
  }

  markFailure();
  return false;
}

bool JXBS_SoilComp7in1::changeAddress(uint8_t newAddress,
                                      uint8_t maxRetries,
                                      uint16_t readTimeoutMs,
                                      uint16_t afterReqDelayMs) {
  if (newAddress == 0 || newAddress > 247) {
    return false;
  }

  // JXBS-style address setup writes the new node address to register 0x0100.
  // Keep only the target sensor connected while this command is enabled.
  uint8_t request[8] = {_address, 0x06, 0x01, 0x00, 0x00, newAddress, 0x00, 0x00};
  uint8_t response[8] = {0};
  const uint8_t check[2] = {_address, 0x06};

  const bool ok = _bus.SendRequest(request,
                                   8,
                                   response,
                                   8,
                                   check,
                                   2,
                                   maxRetries,
                                   readTimeoutMs,
                                   _debugEnable,
                                   afterReqDelayMs);

  if (!ok) {
    return false;
  }

  if (response[2] != 0x01 || response[3] != 0x00 || response[4] != 0x00 || response[5] != newAddress) {
    return false;
  }

  _address = newAddress;
  return true;
}

uint8_t JXBS_SoilComp7in1::scanForAddress(uint8_t startAddr,
                                          uint8_t endAddr,
                                          uint16_t readTimeoutMs,
                                          uint16_t afterReqDelayMs) {
  if (startAddr == 0) startAddr = 1;
  if (endAddr > 247) endAddr = 247;
  if (startAddr > endAddr) return 0;

  for (uint16_t addr = startAddr; addr <= endAddr; ++addr) {
    uint8_t request[8] = {(uint8_t)addr, 0x03, 0x00, 0x12, 0x00, 0x02, 0x00, 0x00};
    uint8_t response[9] = {0};
    const uint8_t check[3] = {(uint8_t)addr, 0x03, 0x04};

    const bool found = _bus.SendRequest(request,
                                        8,
                                        response,
                                        9,
                                        check,
                                        3,
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
