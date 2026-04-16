#include "JXBS_LeafSurfaceHumidity.h"

static double decodeJXBSTemperatureC(uint16_t rawTemperature) {
  // Empirically verified for this tested sensor batch:
  //   room raw 1961 -> 18.05 C
  //   warm stage raw 2237 -> 31.85 C
  //   warm water raw 2282 -> 34.10 C
  //   cold water raw 1703 -> 5.15 C
  return ((double)rawTemperature * 0.05) - 80.0;
}

static void logParsedJXBSLeaf(PrintController* log,
                              bool debug,
                              uint8_t humHi,
                              uint8_t humLo,
                              uint8_t tempHi,
                              uint8_t tempLo,
                              uint16_t rawHumidity,
                              uint16_t rawTemperature,
                              double humidity,
                              double temperature) {
  if (!log || !debug) return;

  log->println(F("[DRV][JXBS_LeafSurfaceHumidity] Parsed response:"), true);

  log->print(F("  bytes [3],[4] -> humidity raw = 0x"), true);
  log->print((unsigned int)humHi, true, "", HEX);
  log->print((unsigned int)humLo, true, " ", HEX);
  log->print(F("| combined = "), true);
  log->print((unsigned int)rawHumidity, true, "", DEC);
  log->print(F(" | humidity = "), true);
  log->print(humidity, true, " %RH", 1);
  log->println("", true);

  log->print(F("  bytes [5],[6] -> temperature raw = 0x"), true);
  log->print((unsigned int)tempHi, true, "", HEX);
  log->print((unsigned int)tempLo, true, " ", HEX);
  log->print(F("| combined = "), true);
  log->print((unsigned int)rawTemperature, true, "", DEC);
  log->print(F(" | formula = raw*0.05 - 80.0 | temperature = "), true);
  log->print(temperature, true, " C", 2);
  log->println("", true);
}

JXBS_LeafSurfaceHumidity::JXBS_LeafSurfaceHumidity(RS485Bus& bus,
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
      leaf_humidity(0.0),
      leaf_temperature(0.0) {}

void JXBS_LeafSurfaceHumidity::setFallbackValues() {
  leaf_humidity = -99.0;
  leaf_temperature = -99.0;
}

bool JXBS_LeafSurfaceHumidity::validateHumidityTemperature() const {
  if (leaf_humidity < 0.0 || leaf_humidity > 100.0) return false;
  if (leaf_temperature < -20.0 || leaf_temperature > 80.0) return false;
  return true;
}

bool JXBS_LeafSurfaceHumidity::readHumidityTemperature(uint8_t driverRetries,
                                                       uint16_t readTimeoutMs,
                                                       uint16_t afterReqDelayMs) {
  if (driverRetries == 0) driverRetries = 1;
  _lastParsedFrame = false;

  for (uint8_t attempt = 1; attempt <= driverRetries; ++attempt) {
    uint8_t request[8] = {_address, 0x03, 0x00, 0x20, 0x00, 0x02, 0x00, 0x00};
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

    const uint8_t humHi = response[3];
    const uint8_t humLo = response[4];
    const uint8_t tempHi = response[5];
    const uint8_t tempLo = response[6];

    const uint16_t rawHumidity = ((uint16_t)humHi << 8) | humLo;
    const uint16_t rawTemperature = ((uint16_t)tempHi << 8) | tempLo;

    leaf_humidity = (double)rawHumidity / 10.0;
    leaf_temperature = decodeJXBSTemperatureC(rawTemperature);

    logParsedJXBSLeaf(_bus.getLogger(),
                      _debugEnable,
                      humHi,
                      humLo,
                      tempHi,
                      tempLo,
                      rawHumidity,
                      rawTemperature,
                      leaf_humidity,
                      leaf_temperature);

    if (validateHumidityTemperature()) {
      return true;
    }

    if (_bus.getLogger() && _debugEnable) {
      _bus.getLogger()->println(
          F("[DRV][JXBS_LeafSurfaceHumidity] Range check fail: parsed values are outside allowed range"),
          true);
    }
  }

  return false;
}

bool JXBS_LeafSurfaceHumidity::readData() {
  markReadTime(millis());

  const uint8_t driverRetries = SENSOR_DEFAULT_DRIVER_RETRIES;
  bool gotAnyValidFrame = false;

  for (uint8_t attempt = 1; attempt <= driverRetries; ++attempt) {
    if (readHumidityTemperature(1, SENSOR_DEFAULT_READ_TIMEOUT_MS, SENSOR_DEFAULT_AFTER_REQ_MS)) {
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

bool JXBS_LeafSurfaceHumidity::changeAddress(uint8_t newAddress,
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

uint8_t JXBS_LeafSurfaceHumidity::scanForAddress(uint8_t startAddr,
                                                 uint8_t endAddr,
                                                 uint16_t readTimeoutMs,
                                                 uint16_t afterReqDelayMs) {
  if (startAddr == 0) startAddr = 1;
  if (endAddr > 247) endAddr = 247;
  if (startAddr > endAddr) return 0;

  for (uint16_t addr = startAddr; addr <= endAddr; ++addr) {
    uint8_t request[8] = {(uint8_t)addr, 0x03, 0x00, 0x20, 0x00, 0x02, 0x00, 0x00};
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
