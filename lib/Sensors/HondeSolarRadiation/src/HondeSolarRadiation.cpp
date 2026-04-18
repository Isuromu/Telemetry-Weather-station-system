#include "HondeSolarRadiation.h"
#include <math.h>

static void logParsedHondeSolarRadiation(PrintController* log,
                                         bool debug,
                                         uint16_t rawRadiation,
                                         double radiation) {
  if (!log || !debug) return;

  log->println(F("[DRV][HondeSolarRadiation] Parsed response:"), true);
  log->print(F("  solar radiation raw = "), true);
  log->print((unsigned int)rawRadiation, true, "", DEC);
  log->print(F(" | solar radiation = "), true);
  log->print(radiation, true, " W/m2", 0);
  log->println("", true);
}

HondeSolarRadiation::HondeSolarRadiation(RS485Bus& bus,
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
      solar_radiation(0.0),
      _bus(bus),
      _lastParsedFrame(false) {}

void HondeSolarRadiation::setFallbackValues() {
  solar_radiation = -99.0;
}

bool HondeSolarRadiation::validateSolarRadiation() const {
  if (isnan(solar_radiation)) return false;
  if (solar_radiation < 0.0 || solar_radiation > 2500.0) return false;
  return true;
}

bool HondeSolarRadiation::readSolarRadiation(uint8_t driverRetries,
                                             uint16_t readTimeoutMs,
                                             uint16_t afterReqDelayMs) {
  if (driverRetries == 0) driverRetries = 1;
  _lastParsedFrame = false;

  for (uint8_t attempt = 1; attempt <= driverRetries; ++attempt) {
    uint8_t request[READ_REQUEST_SIZE] = {
      _address, 0x03, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00
    };

    uint8_t response[READ_RESPONSE_SIZE] = {0};
    const uint8_t check[READ_CHECK_SIZE] = {_address, 0x03, 0x02};

    const bool ok = _bus.SendRequest(request,
                                     READ_REQUEST_SIZE,
                                     response,
                                     READ_RESPONSE_SIZE,
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

    const uint16_t rawRadiation = ((uint16_t)response[3] << 8) | response[4];
    solar_radiation = (double)rawRadiation;

    logParsedHondeSolarRadiation(_bus.getLogger(), _debugEnable, rawRadiation, solar_radiation);

    if (validateSolarRadiation()) {
      return true;
    }

    if (_bus.getLogger() && _debugEnable) {
      _bus.getLogger()->println(F("[DRV][HondeSolarRadiation] Range check fail: solar radiation"), true);
    }
  }

  return false;
}

bool HondeSolarRadiation::readData() {
  markReadTime(millis());

  const uint8_t driverRetries = SENSOR_DEFAULT_DRIVER_RETRIES;
  bool gotAnyValidFrame = false;

  for (uint8_t attempt = 1; attempt <= driverRetries; ++attempt) {
    if (readSolarRadiation(1, SENSOR_DEFAULT_READ_TIMEOUT_MS, SENSOR_DEFAULT_AFTER_REQ_MS)) {
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

bool HondeSolarRadiation::changeAddress(uint8_t newAddress,
                                        uint8_t maxRetries,
                                        uint16_t readTimeoutMs,
                                        uint16_t afterReqDelayMs) {
  if (newAddress == 0) {
    return false;
  }

  // Honde RD-TSRS2L manual:
  //   function 0x10, register 0x0501 = communication address.
  //   manual encodes the address in the high byte and 0x00 in the low byte.
  // Keep only the target sensor connected while this command is enabled.
  uint8_t request[ADDRESS_WRITE_REQUEST_SIZE] = {
    _address, 0x10, 0x05, 0x01, 0x00, 0x01, 0x02, newAddress, 0x00, 0x00, 0x00
  };
  uint8_t response[ADDRESS_WRITE_RESPONSE_SIZE] = {0};
  const uint8_t check[4] = {_address, 0x10, 0x05, 0x01};

  const bool ok = _bus.SendRequest(request,
                                   ADDRESS_WRITE_REQUEST_SIZE,
                                   response,
                                   ADDRESS_WRITE_RESPONSE_SIZE,
                                   check,
                                   4,
                                   maxRetries,
                                   readTimeoutMs,
                                   _debugEnable,
                                   afterReqDelayMs);

  if (!ok) {
    return false;
  }

  if (response[4] != 0x00 || response[5] != 0x01) {
    return false;
  }

  _address = newAddress;
  return true;
}

uint8_t HondeSolarRadiation::scanForAddress(uint8_t startAddr,
                                            uint8_t endAddr,
                                            uint16_t readTimeoutMs,
                                            uint16_t afterReqDelayMs) {
  if (startAddr == 0) startAddr = 1;
  if (endAddr > 247) endAddr = 247;
  if (startAddr > endAddr) return 0;

  for (uint16_t addr = startAddr; addr <= endAddr; ++addr) {
    uint8_t request[READ_REQUEST_SIZE] = {
      (uint8_t)addr, 0x03, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00
    };

    uint8_t response[READ_RESPONSE_SIZE] = {0};
    const uint8_t check[READ_CHECK_SIZE] = {(uint8_t)addr, 0x03, 0x02};

    const bool found = _bus.SendRequest(request,
                                        READ_REQUEST_SIZE,
                                        response,
                                        READ_RESPONSE_SIZE,
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
