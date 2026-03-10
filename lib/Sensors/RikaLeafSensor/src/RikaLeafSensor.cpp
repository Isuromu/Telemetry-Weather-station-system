#include "RikaLeafSensor.h"

RikaLeafSensor::RikaLeafSensor(RS485Bus& bus,
                               const char* sensorId,
                               uint8_t address,
                               bool debugEnable,
                               uint8_t powerPin,
                               uint8_t enablePin,
                               uint16_t sampleRateMin,
                               uint32_t warmUpTimeMs,
                               uint8_t maxConsecutiveErrors)
    : SensorDriver(sensorId,
                   address,
                   debugEnable,
                   powerPin,
                   enablePin,
                   sampleRateMin,
                   warmUpTimeMs,
                   maxConsecutiveErrors),
      _bus(bus),
      leaf_temp(0.0),
      leaf_humid(0.0) {}

void RikaLeafSensor::setFallbackValues() {
  leaf_temp = -99.0;
  leaf_humid = -99.0;
}

bool RikaLeafSensor::readData() {
  markReadTime(millis());

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
                                   3,
                                   2000,
                                   _debugEnable,
                                   20);

  if (!ok) {
    setFallbackValues();
    markFailure();
    return false;
  }

  const int16_t rawTemp = (int16_t)(((uint16_t)response[5] << 8) | response[6]);
  const uint16_t rawHum = ((uint16_t)response[3] << 8) | response[4];

  leaf_temp = (double)rawTemp / 10.0;
  leaf_humid = (double)rawHum / 10.0;

  if (leaf_temp < -40.0 || leaf_temp > 80.0) {
    setFallbackValues();
    markFailure();
    return false;
  }

  if (leaf_humid < 0.0 || leaf_humid > 100.0) {
    setFallbackValues();
    markFailure();
    return false;
  }

  markSuccess();
  return true;
}

bool RikaLeafSensor::changeAddress(uint8_t newAddress,
                                   uint8_t maxRetries,
                                   uint16_t readTimeoutMs,
                                   uint16_t afterReqDelayMs) {
  uint8_t request[8] = {0xFF, 0x06, 0x02, 0x00, 0x00, newAddress, 0x00, 0x00};
  uint8_t response[8] = {0};
  const uint8_t check[5] = {0xFF, 0x06, 0x02, 0x00, 0x00};

  const bool ok = _bus.SendRequest(request,
                                   8,
                                   response,
                                   8,
                                   check,
                                   5,
                                   maxRetries,
                                   readTimeoutMs,
                                   _debugEnable,
                                   afterReqDelayMs);

  if (ok) {
    _address = newAddress;
  }

  return ok;
}

uint8_t RikaLeafSensor::scanForAddress(uint8_t startAddr,
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