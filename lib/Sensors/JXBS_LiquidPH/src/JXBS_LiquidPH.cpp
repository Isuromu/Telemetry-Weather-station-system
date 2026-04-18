#include "JXBS_LiquidPH.h"
#include <math.h>

static bool responseIsJXBSLiquidPHAddressChange(const uint8_t* frame,
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

static void logParsedJXBSLiquidPH(PrintController* log,
                                  bool debug,
                                  int16_t rawTemperature,
                                  uint16_t rawPH,
                                  double temperature,
                                  double ph) {
  if (!log || !debug) return;

  log->println(F("[DRV][JXBS_LiquidPH] Parsed temperature + pH:"), true);

  log->print(F("  temperature raw = "), true);
  log->print((int)rawTemperature, true, "", DEC);
  log->print(F(" | temperature = "), true);
  log->print(temperature, true, " C", 1);
  log->println("", true);

  log->print(F("  pH raw = "), true);
  log->print((unsigned int)rawPH, true, "", DEC);
  log->print(F(" | pH = "), true);
  log->print(ph, true, "", 2);
  log->println("", true);
}

static void logParsedJXBSLiquidTemperature(PrintController* log,
                                           bool debug,
                                           int16_t rawTemperature,
                                           double temperature) {
  if (!log || !debug) return;

  log->println(F("[DRV][JXBS_LiquidPH] Parsed temperature:"), true);
  log->print(F("  temperature raw = "), true);
  log->print((int)rawTemperature, true, "", DEC);
  log->print(F(" | temperature = "), true);
  log->print(temperature, true, " C", 1);
  log->println("", true);
}

static void logParsedJXBSLiquidPHOnly(PrintController* log,
                                      bool debug,
                                      uint16_t rawPH,
                                      double ph) {
  if (!log || !debug) return;

  log->println(F("[DRV][JXBS_LiquidPH] Parsed pH:"), true);
  log->print(F("  pH raw = "), true);
  log->print((unsigned int)rawPH, true, "", DEC);
  log->print(F(" | pH = "), true);
  log->print(ph, true, "", 2);
  log->println("", true);
}

JXBS_LiquidPH::JXBS_LiquidPH(RS485Bus& bus,
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
      liquid_temperature(0.0),
      liquid_ph(0.0),
      _bus(bus),
      _lastParsedFrame(false) {}

void JXBS_LiquidPH::setFallbackValues() {
  liquid_temperature = -99.0;
  liquid_ph = -99.0;
}

bool JXBS_LiquidPH::validateTemperaturePH() const {
  return validateTemperature() && validatePH();
}

bool JXBS_LiquidPH::validateTemperature() const {
  if (isnan(liquid_temperature)) return false;
  if (liquid_temperature < -20.0 || liquid_temperature > 80.0) return false;
  return true;
}

bool JXBS_LiquidPH::validatePH() const {
  if (isnan(liquid_ph)) return false;
  if (liquid_ph < 0.0 || liquid_ph > 14.0) return false;
  return true;
}

bool JXBS_LiquidPH::readTemperaturePH(uint8_t driverRetries,
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
    const uint16_t rawPH = ((uint16_t)response[5] << 8) | response[6];

    liquid_temperature = (double)rawTemperature / 10.0;
    liquid_ph = (double)rawPH / 100.0;

    logParsedJXBSLiquidPH(_bus.getLogger(),
                          _debugEnable,
                          rawTemperature,
                          rawPH,
                          liquid_temperature,
                          liquid_ph);

    if (validateTemperaturePH()) {
      return true;
    }

    if (_bus.getLogger() && _debugEnable) {
      _bus.getLogger()->println(F("[DRV][JXBS_LiquidPH] Range check fail: temperature/pH"), true);
    }
  }

  return false;
}

bool JXBS_LiquidPH::readTemperature(uint8_t driverRetries,
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
    liquid_temperature = (double)rawTemperature / 10.0;

    logParsedJXBSLiquidTemperature(_bus.getLogger(),
                                   _debugEnable,
                                   rawTemperature,
                                   liquid_temperature);

    if (validateTemperature()) {
      return true;
    }

    if (_bus.getLogger() && _debugEnable) {
      _bus.getLogger()->println(F("[DRV][JXBS_LiquidPH] Range check fail: temperature"), true);
    }
  }

  return false;
}

bool JXBS_LiquidPH::readPH(uint8_t driverRetries,
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

    const uint16_t rawPH = ((uint16_t)response[3] << 8) | response[4];
    liquid_ph = (double)rawPH / 100.0;

    logParsedJXBSLiquidPHOnly(_bus.getLogger(), _debugEnable, rawPH, liquid_ph);

    if (validatePH()) {
      return true;
    }

    if (_bus.getLogger() && _debugEnable) {
      _bus.getLogger()->println(F("[DRV][JXBS_LiquidPH] Range check fail: pH"), true);
    }
  }

  return false;
}

bool JXBS_LiquidPH::readData() {
  markReadTime(millis());

  const uint8_t driverRetries = SENSOR_DEFAULT_DRIVER_RETRIES;
  bool gotAnyValidFrame = false;

  for (uint8_t attempt = 1; attempt <= driverRetries; ++attempt) {
    if (readTemperaturePH(1, SENSOR_DEFAULT_READ_TIMEOUT_MS, SENSOR_DEFAULT_AFTER_REQ_MS)) {
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

bool JXBS_LiquidPH::changeAddress(uint8_t newAddress,
                                  uint8_t maxRetries,
                                  uint16_t readTimeoutMs,
                                  uint16_t afterReqDelayMs) {
  if (newAddress == 0 || newAddress > 247) {
    return false;
  }

  // JXBS-style address setup writes the new node address to register 0x0100.
  // Known JXBS leaf hardware replied with the old address; this pH driver
  // accepts both old-address and new-address echo frames so the serial log can
  // show which behavior this sensor uses.
  // Keep only the target sensor connected while this command is enabled.
  uint8_t request[8] = {_address, 0x06, 0x01, 0x00, 0x00, newAddress, 0x00, 0x00};
  if (maxRetries == 0) maxRetries = 1;

  const uint8_t oldAddress = _address;
  for (uint8_t attempt = 1; attempt <= maxRetries; ++attempt) {
    _bus.CRC_Calc(request, sizeof(request), _debugEnable);
    _bus.Request_RS485(request, sizeof(request), afterReqDelayMs, _debugEnable);

    const size_t bytesRead = _bus.Read_RS485(readTimeoutMs, _debugEnable);
    const uint8_t* raw = _bus.rawData();

    if (bytesRead >= 8 && raw) {
      for (size_t offset = 0; offset <= (bytesRead - 8); ++offset) {
        if (responseIsJXBSLiquidPHAddressChange(&raw[offset], oldAddress, newAddress)) {
          if (_bus.getLogger() && _debugEnable) {
            _bus.getLogger()->print(F("[DRV][JXBS_LiquidPH] Address-change response came from 0x"), true);
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

uint8_t JXBS_LiquidPH::scanForAddress(uint8_t startAddr,
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
