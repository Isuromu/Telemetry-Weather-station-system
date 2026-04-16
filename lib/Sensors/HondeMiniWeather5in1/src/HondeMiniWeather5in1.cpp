#include "HondeMiniWeather5in1.h"
#include <math.h>

static void logParsedHondeMini5in1(PrintController* log,
                                   bool debug,
                                   int16_t rawTemp,
                                   int16_t rawHumidity,
                                   int16_t rawPressure,
                                   uint16_t rawWindSpeed,
                                   uint16_t rawDirection,
                                   double airTemp,
                                   double airHumidity,
                                   double airPressure,
                                   double windSpeed) {
  if (!log || !debug) return;

  log->println(F("[DRV][HondeMini5in1] Parsed weather response:"), true);

  log->print(F("  temperature raw = "), true);
  log->print((int)rawTemp, true, "", DEC);
  log->print(F(" | air temperature = "), true);
  log->print(airTemp, true, " C", 1);
  log->println("", true);

  log->print(F("  humidity raw = "), true);
  log->print((int)rawHumidity, true, "", DEC);
  log->print(F(" | relative humidity = "), true);
  log->print(airHumidity, true, " %RH", 1);
  log->println("", true);

  log->print(F("  pressure raw = "), true);
  log->print((int)rawPressure, true, "", DEC);
  log->print(F(" | atmospheric pressure = "), true);
  log->print(airPressure, true, " hPa", 1);
  log->println("", true);

  log->print(F("  wind speed raw = "), true);
  log->print((unsigned int)rawWindSpeed, true, "", DEC);
  log->print(F(" | wind speed = "), true);
  log->print(windSpeed, true, " m/s", 2);
  log->println("", true);

  log->print(F("  direction raw = "), true);
  log->print((unsigned int)rawDirection, true, "", DEC);
  log->print(F(" | wind direction = "), true);
  log->print((unsigned int)rawDirection, true, " deg", DEC);
  log->println("", true);
}

HondeMiniWeather5in1::HondeMiniWeather5in1(RS485Bus& bus,
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
      air_temp(0.0),
      air_humidity(0.0),
      air_pressure(0.0),
      wind_speed(0.0),
      wind_direction(0) {}

void HondeMiniWeather5in1::setFallbackValues() {
  air_temp = -99.0;
  air_humidity = -99.0;
  air_pressure = -99.0;
  wind_speed = -99.0;
  wind_direction = 0xFFFF;
}

int16_t HondeMiniWeather5in1::readSignedRegister(const uint8_t response[], uint8_t registerIndex) {
  const uint8_t dataIndex = (uint8_t)(3 + (registerIndex * 2));
  return (int16_t)(((uint16_t)response[dataIndex] << 8) | response[dataIndex + 1]);
}

uint16_t HondeMiniWeather5in1::readUnsignedRegister(const uint8_t response[], uint8_t registerIndex) {
  const uint8_t dataIndex = (uint8_t)(3 + (registerIndex * 2));
  return ((uint16_t)response[dataIndex] << 8) | response[dataIndex + 1];
}

bool HondeMiniWeather5in1::validateWeatherData() const {
  if (isnan(air_temp) || isnan(air_humidity) || isnan(air_pressure) || isnan(wind_speed)) return false;
  if (air_temp < -40.0 || air_temp > 80.0) return false;
  if (air_humidity < 0.0 || air_humidity > 100.0) return false;
  if (air_pressure < 300.0 || air_pressure > 1100.0) return false;
  if (wind_speed < 0.0 || wind_speed > 45.0) return false;
  if (wind_direction > 360) return false;
  return true;
}

bool HondeMiniWeather5in1::readData() {
  markReadTime(millis());

  const uint8_t driverRetries = SENSOR_DEFAULT_DRIVER_RETRIES;

  for (uint8_t attempt = 1; attempt <= driverRetries; ++attempt) {
    uint8_t request[READ_REQUEST_SIZE] = {
      _address, 0x03, 0x00, 0x00, 0x00, 0x0D, 0x00, 0x00
    };

    uint8_t response[READ_RESPONSE_SIZE] = {0};
    const uint8_t check[READ_CHECK_SIZE] = {_address, 0x03, 0x1A};

    const bool ok = _bus.SendRequest(request,
                                     READ_REQUEST_SIZE,
                                     response,
                                     READ_RESPONSE_SIZE,
                                     check,
                                     READ_CHECK_SIZE,
                                     SENSOR_DEFAULT_BUS_RETRIES,
                                     SENSOR_DEFAULT_READ_TIMEOUT_MS,
                                     _debugEnable,
                                     SENSOR_DEFAULT_AFTER_REQ_MS);

    if (!ok) {
      continue;
    }

    const int16_t rawTemp = readSignedRegister(response, 0);
    const int16_t rawHumidity = readSignedRegister(response, 1);
    const int16_t rawPressure = readSignedRegister(response, 6);
    const uint16_t rawWindSpeed = readUnsignedRegister(response, 11);
    const uint16_t rawDirection = readUnsignedRegister(response, 12);

    if ((uint16_t)rawTemp == INVALID_REGISTER_VALUE ||
        (uint16_t)rawHumidity == INVALID_REGISTER_VALUE ||
        (uint16_t)rawPressure == INVALID_REGISTER_VALUE ||
        rawWindSpeed == INVALID_REGISTER_VALUE ||
        rawDirection == INVALID_REGISTER_VALUE) {
      if (_bus.getLogger() && _debugEnable) {
        _bus.getLogger()->println(F("[DRV][HondeMini5in1] Invalid/unconnected register value"), true);
      }
      continue;
    }

    air_temp = (double)rawTemp / 10.0;
    air_humidity = (double)rawHumidity / 10.0;
    air_pressure = (double)rawPressure / 10.0;
    wind_speed = (double)rawWindSpeed / 100.0;
    wind_direction = rawDirection;

    logParsedHondeMini5in1(_bus.getLogger(),
                           _debugEnable,
                           rawTemp,
                           rawHumidity,
                           rawPressure,
                           rawWindSpeed,
                           rawDirection,
                           air_temp,
                           air_humidity,
                           air_pressure,
                           wind_speed);

    if (validateWeatherData()) {
      markSuccess();
      return true;
    }

    if (_bus.getLogger() && _debugEnable) {
      _bus.getLogger()->println(F("[DRV][HondeMini5in1] Range check fail: weather data"), true);
    }
  }

  setFallbackValues();
  markFailure();
  return false;
}

bool HondeMiniWeather5in1::changeAddress(uint8_t newAddress,
                                         uint8_t maxRetries,
                                         uint16_t readTimeoutMs,
                                         uint16_t afterReqDelayMs) {
  if (newAddress == 0 || newAddress > 247) {
    return false;
  }

  // Honde MINI manual:
  //   register 0x0020 = device address
  // Keep only one target sensor on the bus while changing addresses so that
  // another device cannot be configured accidentally.
  uint8_t request[8] = {_address, 0x06, 0x00, 0x20, 0x00, newAddress, 0x00, 0x00};
  uint8_t response[8] = {0};
  const uint8_t check[4] = {_address, 0x06, 0x00, 0x20};

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

  if (!ok) {
    return false;
  }

  if (response[4] != 0x00 || response[5] != newAddress) {
    return false;
  }

  _address = newAddress;
  return true;
}

uint8_t HondeMiniWeather5in1::scanForAddress(uint8_t startAddr,
                                             uint8_t endAddr,
                                             uint16_t readTimeoutMs,
                                             uint16_t afterReqDelayMs) {
  if (startAddr == 0) startAddr = 1;
  if (endAddr > 247) endAddr = 247;
  if (startAddr > endAddr) return 0;

  for (uint16_t addr = startAddr; addr <= endAddr; ++addr) {
    uint8_t request[READ_REQUEST_SIZE] = {
      (uint8_t)addr, 0x03, 0x00, 0x0B, 0x00, 0x02, 0x00, 0x00
    };

    uint8_t response[SCAN_RESPONSE_SIZE] = {0};
    const uint8_t check[READ_CHECK_SIZE] = {(uint8_t)addr, 0x03, 0x04};

    const bool found = _bus.SendRequest(request,
                                        READ_REQUEST_SIZE,
                                        response,
                                        SCAN_RESPONSE_SIZE,
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
