#include "JXBS_SoilComp7in1.h"

JXBS_SoilComp7in1::JXBS_SoilComp7in1(RS485Bus& bus,
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
      soil_moisture(0.0),
      soil_temp(0.0),
      soil_ec(0.0),
      soil_ph(0.0),
      soil_nitrogen(0),
      soil_phosphorus(0),
      soil_potassium(0) {}

void JXBS_SoilComp7in1::setFallbackValues() {
  soil_moisture = -99.0;
  soil_temp = -99.0;
  soil_ec = -99.0;
  soil_ph = -99.0;
  soil_nitrogen = 0xFFFF;
  soil_phosphorus = 0xFFFF;
  soil_potassium = 0xFFFF;
}

bool JXBS_SoilComp7in1::validateMoistureTemperature() const {
  if (soil_moisture < 0.0 || soil_moisture > 100.0) return false;
  if (soil_temp < -40.0 || soil_temp > 80.0) return false;
  return true;
}

bool JXBS_SoilComp7in1::validateConductivity() const {
  if (soil_ec < 0.0 || soil_ec > 10000.0) return false;
  return true;
}

bool JXBS_SoilComp7in1::validatePH() const {
  if (soil_ph < 3.0 || soil_ph > 9.0) return false;
  return true;
}

bool JXBS_SoilComp7in1::validateNPK() const {
  if (soil_nitrogen > 1999) return false;
  if (soil_phosphorus > 1999) return false;
  if (soil_potassium > 1999) return false;
  return true;
}

bool JXBS_SoilComp7in1::readMoistureTemperature(uint8_t driverRetries,
                                                uint16_t readTimeoutMs,
                                                uint16_t afterReqDelayMs) {
  if (driverRetries == 0) driverRetries = 1;

  for (uint8_t attempt = 1; attempt <= driverRetries; ++attempt) {
    uint8_t request[8] = {_address, 0x03, 0x00, 0x12, 0x00, 0x02, 0x00, 0x00};
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

    const uint16_t rawMoisture = ((uint16_t)response[3] << 8) | response[4];
    const int16_t rawTemp = (int16_t)(((uint16_t)response[5] << 8) | response[6]);

    soil_moisture = (double)rawMoisture / 10.0;
    soil_temp = (double)rawTemp / 10.0;

    if (validateMoistureTemperature()) {
      return true;
    }
  }

  return false;
}

bool JXBS_SoilComp7in1::readConductivity(uint8_t driverRetries,
                                         uint16_t readTimeoutMs,
                                         uint16_t afterReqDelayMs) {
  if (driverRetries == 0) driverRetries = 1;

  for (uint8_t attempt = 1; attempt <= driverRetries; ++attempt) {
    uint8_t request[8] = {_address, 0x03, 0x00, 0x15, 0x00, 0x01, 0x00, 0x00};
    uint8_t response[7] = {0};
    const uint8_t check[3] = {_address, 0x03, 0x02};

    const bool ok = _bus.SendRequest(request,
                                     8,
                                     response,
                                     7,
                                     check,
                                     3,
                                     SENSOR_DEFAULT_BUS_RETRIES,
                                     readTimeoutMs,
                                     _debugEnable,
                                     afterReqDelayMs);
    if (!ok) {
      continue;
    }

    const uint16_t rawEc = ((uint16_t)response[3] << 8) | response[4];
    soil_ec = (double)rawEc;

    if (validateConductivity()) {
      return true;
    }
  }

  return false;
}

bool JXBS_SoilComp7in1::readPH(uint8_t driverRetries,
                               uint16_t readTimeoutMs,
                               uint16_t afterReqDelayMs) {
  if (driverRetries == 0) driverRetries = 1;

  for (uint8_t attempt = 1; attempt <= driverRetries; ++attempt) {
    uint8_t request[8] = {_address, 0x03, 0x00, 0x06, 0x00, 0x01, 0x00, 0x00};
    uint8_t response[7] = {0};
    const uint8_t check[3] = {_address, 0x03, 0x02};

    const bool ok = _bus.SendRequest(request,
                                     8,
                                     response,
                                     7,
                                     check,
                                     3,
                                     SENSOR_DEFAULT_BUS_RETRIES,
                                     readTimeoutMs,
                                     _debugEnable,
                                     afterReqDelayMs);
    if (!ok) {
      continue;
    }

    const uint16_t rawPH = ((uint16_t)response[3] << 8) | response[4];
    soil_ph = (double)rawPH / 100.0;

    if (validatePH()) {
      return true;
    }
  }

  return false;
}

bool JXBS_SoilComp7in1::readNPK(uint8_t driverRetries,
                                uint16_t readTimeoutMs,
                                uint16_t afterReqDelayMs) {
  if (driverRetries == 0) driverRetries = 1;

  for (uint8_t attempt = 1; attempt <= driverRetries; ++attempt) {
    uint8_t request[8] = {_address, 0x03, 0x00, 0x1E, 0x00, 0x03, 0x00, 0x00};
    uint8_t response[11] = {0};
    const uint8_t check[3] = {_address, 0x03, 0x06};

    const bool ok = _bus.SendRequest(request,
                                     8,
                                     response,
                                     11,
                                     check,
                                     3,
                                     SENSOR_DEFAULT_BUS_RETRIES,
                                     readTimeoutMs,
                                     _debugEnable,
                                     afterReqDelayMs);
    if (!ok) {
      continue;
    }

    soil_nitrogen   = ((uint16_t)response[3] << 8) | response[4];
    soil_phosphorus = ((uint16_t)response[5] << 8) | response[6];
    soil_potassium  = ((uint16_t)response[7] << 8) | response[8];

    if (validateNPK()) {
      return true;
    }
  }

  return false;
}

bool JXBS_SoilComp7in1::readData() {
  markReadTime(millis());

  const uint8_t driverRetries = SENSOR_DEFAULT_DRIVER_RETRIES;

  for (uint8_t attempt = 1; attempt <= driverRetries; ++attempt) {
    if (!readMoistureTemperature(1, SENSOR_DEFAULT_READ_TIMEOUT_MS, SENSOR_DEFAULT_AFTER_REQ_MS)) {
      continue;
    }

    if (!readConductivity(1, SENSOR_DEFAULT_READ_TIMEOUT_MS, SENSOR_DEFAULT_AFTER_REQ_MS)) {
      continue;
    }

    if (!readPH(1, SENSOR_DEFAULT_READ_TIMEOUT_MS, SENSOR_DEFAULT_AFTER_REQ_MS)) {
      continue;
    }

    if (!readNPK(1, SENSOR_DEFAULT_READ_TIMEOUT_MS, SENSOR_DEFAULT_AFTER_REQ_MS)) {
      continue;
    }

    markSuccess();
    return true;
  }

  setFallbackValues();
  markFailure();
  return false;
}

bool JXBS_SoilComp7in1::changeAddress(uint8_t newAddress,
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

uint8_t JXBS_SoilComp7in1::scanForAddress(uint8_t startAddr,
                                          uint8_t endAddr,
                                          uint16_t readTimeoutMs,
                                          uint16_t afterReqDelayMs) {
  if (startAddr == 0) startAddr = 1;
  if (endAddr > 247) endAddr = 247;
  if (startAddr > endAddr) return 0;

  for (uint16_t addr = startAddr; addr <= endAddr; ++addr) {
    uint8_t request[8] = {(uint8_t)addr, 0x03, 0x00, 0x12, 0x00, 0x02, 0x00, 0x00};
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