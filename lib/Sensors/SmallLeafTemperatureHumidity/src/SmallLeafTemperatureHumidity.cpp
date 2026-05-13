#include "SmallLeafTemperatureHumidity.h"
#include <math.h>

static bool responseIsSmallLeafAddressChange(const uint8_t* frame,
                                             uint8_t oldAddress,
                                             uint8_t newAddress) {
  if (!frame) return false;

  const bool responseAddressOk = (frame[0] == oldAddress || frame[0] == newAddress);
  return responseAddressOk &&
         frame[1] == 0x06 &&
         frame[2] == 0x00 &&
         frame[3] == 0x30 &&
         frame[4] == 0x00 &&
         frame[5] == newAddress &&
         RS485Bus::verifyCrc16ModbusFrame(frame, 8);
}

static void logParsedSmallLeaf(PrintController* log,
                               bool debug,
                               uint8_t tempHi,
                               uint8_t tempLo,
                               uint8_t humidHi,
                               uint8_t humidLo,
                               int16_t rawTemperature,
                               uint16_t rawHumidity,
                               double temperature,
                               double humidity) {
  if (!log || !debug) return;

  log->println(F("[DRV][SmallLeafTempHumidity] Parsed response:"), true);

  log->print(F("  bytes [3],[4] -> temperature raw = 0x"), true);
  log->print((unsigned int)tempHi, true, "", HEX);
  log->print((unsigned int)tempLo, true, " ", HEX);
  log->print(F("| combined = "), true);
  log->print((int)rawTemperature, true, "", DEC);
  log->print(F(" | formula = raw / 100 | temperature = "), true);
  log->print(temperature, true, " C", 2);
  log->println("", true);

  log->print(F("  bytes [5],[6] -> humidity raw = 0x"), true);
  log->print((unsigned int)humidHi, true, "", HEX);
  log->print((unsigned int)humidLo, true, " ", HEX);
  log->print(F("| combined = "), true);
  log->print((unsigned int)rawHumidity, true, "", DEC);
  log->print(F(" | formula = raw / 100 | humidity = "), true);
  log->print(humidity, true, " %RH", 2);
  log->println("", true);
}

SmallLeafTemperatureHumidity::SmallLeafTemperatureHumidity(
    RS485Bus& bus,
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
      leaf_temperature_raw(0),
      leaf_humidity_raw(0),
      leaf_temperature(0.0),
      leaf_humidity(0.0),
      _bus(bus),
      _lastParsedFrame(false) {}

void SmallLeafTemperatureHumidity::setFallbackValues() {
  leaf_temperature_raw = 0;
  leaf_humidity_raw = 0;
  leaf_temperature = -99.0;
  leaf_humidity = -99.0;
}

bool SmallLeafTemperatureHumidity::validateHumidityTemperature() const {
  if (isnan(leaf_temperature) || isnan(leaf_humidity)) return false;
  if (leaf_temperature < -40.0 || leaf_temperature > 100.0) return false;
  if (leaf_humidity < 0.0 || leaf_humidity > 100.0) return false;
  return true;
}

bool SmallLeafTemperatureHumidity::readHumidityTemperature(uint8_t driverRetries,
                                                           uint16_t readTimeoutMs,
                                                           uint16_t afterReqDelayMs) {
  if (driverRetries == 0) driverRetries = 1;
  _lastParsedFrame = false;

  for (uint8_t attempt = 1; attempt <= driverRetries; ++attempt) {
    uint8_t request[READ_REQUEST_SIZE] = {
      _address, 0x03, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00
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

    const uint8_t tempHi = response[3];
    const uint8_t tempLo = response[4];
    const uint8_t humidHi = response[5];
    const uint8_t humidLo = response[6];

    leaf_temperature_raw = (int16_t)(((uint16_t)tempHi << 8) | tempLo);
    leaf_humidity_raw = ((uint16_t)humidHi << 8) | humidLo;

    leaf_temperature = (double)leaf_temperature_raw / 100.0;
    leaf_humidity = (double)leaf_humidity_raw / 100.0;

    logParsedSmallLeaf(_bus.getLogger(),
                       _debugEnable,
                       tempHi,
                       tempLo,
                       humidHi,
                       humidLo,
                       leaf_temperature_raw,
                       leaf_humidity_raw,
                       leaf_temperature,
                       leaf_humidity);

    if (validateHumidityTemperature()) {
      return true;
    }

    if (_bus.getLogger() && _debugEnable) {
      _bus.getLogger()->println(F("[DRV][SmallLeafTempHumidity] Range check fail: parsed values are outside allowed range"), true);
    }
  }

  return false;
}

bool SmallLeafTemperatureHumidity::readData() {
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

bool SmallLeafTemperatureHumidity::changeAddress(uint8_t newAddress,
                                                 uint8_t maxRetries,
                                                 uint16_t readTimeoutMs,
                                                 uint16_t afterReqDelayMs) {
  if (newAddress == 0 || newAddress > 247) {
    return false;
  }

  uint8_t request[8] = {_address, 0x06, 0x00, 0x30, 0x00, newAddress, 0x00, 0x00};
  if (maxRetries == 0) maxRetries = 1;

  const uint8_t oldAddress = _address;
  for (uint8_t attempt = 1; attempt <= maxRetries; ++attempt) {
    _bus.CRC_Calc(request, sizeof(request), _debugEnable);
    _bus.Request_RS485(request, sizeof(request), afterReqDelayMs, _debugEnable);

    const size_t bytesRead = _bus.Read_RS485(readTimeoutMs, _debugEnable);
    const uint8_t* raw = _bus.rawData();

    if (bytesRead >= 8 && raw) {
      for (size_t offset = 0; offset <= (bytesRead - 8); ++offset) {
        if (responseIsSmallLeafAddressChange(&raw[offset], oldAddress, newAddress)) {
          if (_bus.getLogger() && _debugEnable) {
            _bus.getLogger()->print(F("[DRV][SmallLeafTempHumidity] Address-change response came from 0x"), true);
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

uint8_t SmallLeafTemperatureHumidity::scanForAddress(uint8_t startAddr,
                                                     uint8_t endAddr,
                                                     uint16_t readTimeoutMs,
                                                     uint16_t afterReqDelayMs) {
  if (startAddr == 0) startAddr = 1;
  if (endAddr > 247) endAddr = 247;
  if (startAddr > endAddr) return 0;

  for (uint16_t addr = startAddr; addr <= endAddr; ++addr) {
    uint8_t request[READ_REQUEST_SIZE] = {
      (uint8_t)addr, 0x03, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00
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
