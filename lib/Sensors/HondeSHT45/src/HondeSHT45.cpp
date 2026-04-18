#include "HondeSHT45.h"
#include <math.h>

HondeSHT45::HondeSHT45(const char* sensorId,
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
      air_temperature(0.0),
      air_humidity(0.0),
      _wire(&Wire),
      _log(nullptr),
      _lastParsedFrame(false) {}

void HondeSHT45::begin(TwoWire* wire) {
  _wire = wire ? wire : &Wire;
}

void HondeSHT45::setLogger(PrintController* logger) {
  _log = logger;
}

void HondeSHT45::setFallbackValues() {
  air_temperature = -99.0;
  air_humidity = -99.0;
}

bool HondeSHT45::validateTemperatureHumidity() const {
  if (isnan(air_temperature) || isnan(air_humidity)) return false;
  if (air_temperature < -40.0 || air_temperature > 125.0) return false;
  if (air_humidity < 0.0 || air_humidity > 100.0) return false;
  return true;
}

bool HondeSHT45::writeCommand(uint8_t command) {
  if (!_wire) return false;

  _wire->beginTransmission(_address);
  _wire->write(command);
  const uint8_t result = _wire->endTransmission();

  if (_log && _debugEnable) {
    _log->print(F("[DRV][HondeSHT45] TX command 0x"), true);
    _log->print((unsigned int)command, true, " | I2C status=", HEX);
    _log->println((unsigned int)result, true);
  }

  return result == 0;
}

bool HondeSHT45::readMeasurementBytes(uint8_t response[6]) {
  if (!_wire || !response) return false;

  const uint8_t requested = 6;
  const uint8_t received = _wire->requestFrom((int)_address, (int)requested);

  if (_log && _debugEnable) {
    _log->print(F("[DRV][HondeSHT45] RX bytes="), true);
    _log->println((unsigned int)received, true);
  }

  if (received != requested) {
    while (_wire->available() > 0) {
      (void)_wire->read();
    }
    return false;
  }

  for (uint8_t i = 0; i < requested; ++i) {
    response[i] = (uint8_t)_wire->read();
  }

  if (_log && _debugEnable) {
    _log->print(F("[DRV][HondeSHT45] RX raw -> "), true);
    for (uint8_t i = 0; i < requested; ++i) {
      _log->print((unsigned int)response[i], true, i + 1 < requested ? " " : "", HEX);
    }
    _log->println("", true);
  }

  return true;
}

void HondeSHT45::logParsed(uint16_t rawTemperature, uint16_t rawHumidity) const {
  if (!_log || !_debugEnable) return;

  _log->println(F("[DRV][HondeSHT45] Parsed response:"), true);

  _log->print(F("  temperature raw = "), true);
  _log->print((unsigned int)rawTemperature, true, "", DEC);
  _log->print(F(" | temperature = "), true);
  _log->print(air_temperature, true, " C", 2);
  _log->println("", true);

  _log->print(F("  humidity raw = "), true);
  _log->print((unsigned int)rawHumidity, true, "", DEC);
  _log->print(F(" | humidity = "), true);
  _log->print(air_humidity, true, " %RH", 2);
  _log->println("", true);
}

uint8_t HondeSHT45::crc8(const uint8_t* data, size_t len) {
  uint8_t crc = 0xFF;

  for (size_t i = 0; i < len; ++i) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      if (crc & 0x80) {
        crc = (uint8_t)((crc << 1) ^ 0x31);
      } else {
        crc <<= 1;
      }
    }
  }

  return crc;
}

bool HondeSHT45::crcOk(uint8_t msb, uint8_t lsb, uint8_t expectedCrc) {
  const uint8_t data[2] = {msb, lsb};
  return crc8(data, sizeof(data)) == expectedCrc;
}

bool HondeSHT45::readTemperatureHumidity(uint8_t driverRetries,
                                         uint16_t measurementDelayMs) {
  if (driverRetries == 0) driverRetries = 1;
  _lastParsedFrame = false;

  for (uint8_t attempt = 1; attempt <= driverRetries; ++attempt) {
    if (!writeCommand(COMMAND_MEASURE_HIGH_PRECISION)) {
      delay(10);
      continue;
    }

    delay(measurementDelayMs);

    uint8_t response[6] = {0};
    if (!readMeasurementBytes(response)) {
      delay(10);
      continue;
    }

    if (!crcOk(response[0], response[1], response[2]) ||
        !crcOk(response[3], response[4], response[5])) {
      if (_log && _debugEnable) {
        _log->println(F("[DRV][HondeSHT45] CRC check failed."), true);
      }
      _lastParsedFrame = true;
      delay(10);
      continue;
    }

    _lastParsedFrame = true;

    const uint16_t rawTemperature = ((uint16_t)response[0] << 8) | response[1];
    const uint16_t rawHumidity = ((uint16_t)response[3] << 8) | response[4];

    air_temperature = -45.0 + (175.0 * (double)rawTemperature / 65535.0);
    air_humidity = -6.0 + (125.0 * (double)rawHumidity / 65535.0);

    if (air_humidity < 0.0) air_humidity = 0.0;
    if (air_humidity > 100.0) air_humidity = 100.0;

    logParsed(rawTemperature, rawHumidity);

    if (validateTemperatureHumidity()) {
      return true;
    }

    if (_log && _debugEnable) {
      _log->println(F("[DRV][HondeSHT45] Range check failed."), true);
    }
  }

  return false;
}

bool HondeSHT45::readData() {
  markReadTime(millis());

  const uint8_t driverRetries = SENSOR_DEFAULT_DRIVER_RETRIES;
  bool gotAnyValidFrame = false;

  for (uint8_t attempt = 1; attempt <= driverRetries; ++attempt) {
    if (readTemperatureHumidity(1, 10)) {
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

bool HondeSHT45::softReset(uint16_t resetDelayMs) {
  const bool ok = writeCommand(COMMAND_SOFT_RESET);
  delay(resetDelayMs);
  return ok;
}
