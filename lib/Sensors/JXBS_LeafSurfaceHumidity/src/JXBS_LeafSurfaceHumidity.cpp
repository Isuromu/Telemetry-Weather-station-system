#include "JXBS_LeafSurfaceHumidity.h"
#include <math.h>

static bool responseIsJXBSAddressChange(const uint8_t* frame,
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

static double decodeJXBSTemperatureC(uint16_t rawTemperature) {
  // Empirically verified for this tested sensor batch:
  //   room raw 1961 -> 18.05 C
  //   warm stage raw 2237 -> 31.85 C
  //   warm water raw 2282 -> 34.10 C
  //   cold water raw 1703 -> 5.15 C
  double temp_corrected = rawTemperature  / 100.0;
  return temp_corrected;
}

static void logParsedJXBSLeaf(PrintController* log,
                              bool debug,
                              uint16_t rawHumidity,
                              uint16_t rawTemperature,
                              double humidity,
                              double temperature) {
  if (!log || !debug) return;

  log->println(F("[DRV][JXBS_LeafSurfaceHumidity] Parsed response:"), true);

  log->print(F("  humidity raw = "), true);
  log->print((unsigned int)rawHumidity, true, "", DEC);
  log->print(F(" | humidity = "), true);
  log->print(humidity, true, " %RH", 1);
  log->println("", true);

  log->print(F("  temperature raw = "), true);
  log->print((unsigned int)rawTemperature, true, "", DEC);
  log->print(F(" | formula = rawTemperature /100; | temperature = "), true);
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
      leaf_humidity(0.0),
      leaf_temperature(0.0),
      _bus(bus),
      _lastParsedFrame(false) {}

void JXBS_LeafSurfaceHumidity::setFallbackValues() {
  leaf_humidity = -99.0;
  leaf_temperature = -99.0;
}

bool JXBS_LeafSurfaceHumidity::validateHumidityTemperature() const {
  if (isnan(leaf_humidity) || isnan(leaf_temperature)) {
    return false;
  }

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
    uint8_t request[READ_REQUEST_SIZE] = {
      _address, 0x03, 0x00, 0x20, 0x00, 0x02, 0x00, 0x00
    };

    uint8_t response[READ_RESPONSE_SIZE] = {0};
    const uint8_t check[READ_CHECK_SIZE] = {_address, 0x03, 0x04};

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

    const uint16_t rawHumidity = ((uint16_t)response[3] << 8) | response[4];
    const uint16_t rawTemperature = ((uint16_t)response[5] << 8) | response[6];

    leaf_humidity = (double)rawHumidity / 10.0;
    leaf_temperature = decodeJXBSTemperatureC(rawTemperature);

    logParsedJXBSLeaf(_bus.getLogger(),
                      _debugEnable,
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
  // Some batches echo the old address, while others answer from the new
  // address immediately after the write. Accept either valid response.
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
        if (responseIsJXBSAddressChange(&raw[offset], oldAddress, newAddress)) {
          if (_bus.getLogger() && _debugEnable) {
            _bus.getLogger()->print(F("[DRV][JXBS_LeafSurfaceHumidity] Address-change response came from 0x"), true);
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

uint8_t JXBS_LeafSurfaceHumidity::scanForAddress(uint8_t startAddr,
                                                 uint8_t endAddr,
                                                 uint16_t readTimeoutMs,
                                                 uint16_t afterReqDelayMs) {
  if (startAddr == 0) startAddr = 1;
  if (endAddr > 247) endAddr = 247;
  if (startAddr > endAddr) return 0;

  for (uint16_t addr = startAddr; addr <= endAddr; ++addr) {
    uint8_t request[READ_REQUEST_SIZE] = {
      (uint8_t)addr, 0x03, 0x00, 0x20, 0x00, 0x02, 0x00, 0x00
    };

    uint8_t response[READ_RESPONSE_SIZE] = {0};
    const uint8_t check[READ_CHECK_SIZE] = {(uint8_t)addr, 0x03, 0x04};

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
