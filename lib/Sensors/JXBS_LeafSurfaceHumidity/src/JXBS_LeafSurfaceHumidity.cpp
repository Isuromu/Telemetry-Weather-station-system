#include "JXBS_LeafSurfaceHumidity.h"

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

  for (uint8_t attempt = 1; attempt <= driverRetries; ++attempt) {
    // Read 2 registers starting at 0x0020:
    // 0x0020 = humidity
    // 0x0021 = temperature
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

    const uint16_t rawHumidity = ((uint16_t)response[3] << 8) | response[4];
    const int16_t rawTemperature = (int16_t)(((uint16_t)response[5] << 8) | response[6]);

    leaf_humidity = (double)rawHumidity / 10.0;
    leaf_temperature = (double)rawTemperature / 10.0;

    if (validateHumidityTemperature()) {
      return true;
    }
  }

  return false;
}

bool JXBS_LeafSurfaceHumidity::readData() {
  markReadTime(millis());

  const uint8_t driverRetries = SENSOR_DEFAULT_DRIVER_RETRIES;

  for (uint8_t attempt = 1; attempt <= driverRetries; ++attempt) {
    if (!readHumidityTemperature(1, SENSOR_DEFAULT_READ_TIMEOUT_MS, SENSOR_DEFAULT_AFTER_REQ_MS)) {
      continue;
    }

    markSuccess();
    return true;
  }

  setFallbackValues();
  markFailure();
  return false;
}

bool JXBS_LeafSurfaceHumidity::changeAddress(uint8_t newAddress,
                                             uint8_t maxRetries,
                                             uint16_t readTimeoutMs,
                                             uint16_t afterReqDelayMs) {
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