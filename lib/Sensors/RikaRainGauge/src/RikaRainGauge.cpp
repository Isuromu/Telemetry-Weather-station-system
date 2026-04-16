#include "RikaRainGauge.h"

RikaRainGauge::RikaRainGauge(RS485Bus& bus,
                             const char* sensorId,
                             uint8_t address,
                             bool debugEnable,
                             uint8_t powerLineIndex,
                             uint8_t interfaceIndex,
                             uint16_t sampleRateMin,
                             uint32_t warmUpTimeMs,
                             uint8_t maxConsecutiveErrors,
                             uint32_t minUsefulPowerOffMs,
                             CountingMode countingMode,
                             uint8_t pcf8574Address,
                             int8_t counterResetPin,
                             int8_t interruptPin,
                             uint16_t interruptDebounceMs)
    : RainGaugeCounter(sensorId,
                       address,
                       debugEnable,
                       powerLineIndex,
                       interfaceIndex,
                       sampleRateMin,
                       warmUpTimeMs,
                       maxConsecutiveErrors,
                       minUsefulPowerOffMs,
                       MM_PER_PULSE,
                       countingMode,
                       pcf8574Address,
                       counterResetPin,
                       interruptPin,
                       interruptDebounceMs) {
  _bus = &bus;
}

bool RikaRainGauge::readRs485Rainfall(double& rainfallMm) {
  if (!_bus) return false;

  uint8_t request[8] = {_address, 0x03, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00};
  uint8_t response[7] = {0};
  const uint8_t check[3] = {_address, 0x03, 0x02};

  const bool ok = _bus->SendRequest(request,
                                    8,
                                    response,
                                    7,
                                    check,
                                    3,
                                    SENSOR_DEFAULT_BUS_RETRIES,
                                    SENSOR_DEFAULT_READ_TIMEOUT_MS,
                                    _debugEnable,
                                    SENSOR_DEFAULT_AFTER_REQ_MS);
  if (!ok) return false;

  const uint16_t raw = ((uint16_t)response[3] << 8) | response[4];
  rainfallMm = (double)raw / 10.0;
  return true;
}

bool RikaRainGauge::resetRs485Rainfall() {
  if (!_bus) return false;

  uint8_t request[11] = {_address, 0x10, 0x00, 0x00, 0x00, 0x01, 0x02, 0x00, 0x00, 0x00, 0x00};
  uint8_t response[8] = {0};
  const uint8_t check[6] = {_address, 0x10, 0x00, 0x00, 0x00, 0x01};

  return _bus->SendRequest(request,
                           11,
                           response,
                           8,
                           check,
                           6,
                           SENSOR_DEFAULT_BUS_RETRIES,
                           SENSOR_DEFAULT_READ_TIMEOUT_MS,
                           _debugEnable,
                           SENSOR_DEFAULT_AFTER_REQ_MS);
}

bool RikaRainGauge::changeAddress(uint8_t newAddress,
                                  uint8_t maxRetries,
                                  uint16_t readTimeoutMs,
                                  uint16_t afterReqDelayMs) {
  if (!_bus || newAddress == 0 || newAddress > 247) return false;

  uint8_t request[11] = {0x00, 0x10, 0x10, 0x00, 0x00, 0x01, 0x02, 0x00, newAddress, 0x00, 0x00};
  uint8_t response[8] = {0};
  const uint8_t check[6] = {0x00, 0x10, 0x10, 0x00, 0x00, 0x01};

  const bool ok = _bus->SendRequest(request,
                                    11,
                                    response,
                                    8,
                                    check,
                                    6,
                                    maxRetries,
                                    readTimeoutMs,
                                    _debugEnable,
                                    afterReqDelayMs);
  if (ok) {
    _address = newAddress;
  }
  return ok;
}
