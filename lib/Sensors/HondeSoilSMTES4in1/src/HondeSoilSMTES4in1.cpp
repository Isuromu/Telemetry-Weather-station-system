#include "HondeSoilSMTES4in1.h"
#include <math.h>

static void logParsedHondeSoil4in1(PrintController* log,
                                   bool debug,
                                   uint16_t rawMoisture,
                                   int16_t rawTemperature,
                                   uint16_t rawEc,
                                   uint16_t rawSalinity,
                                   double moisture,
                                   double temperature,
                                   double ec,
                                   double salinity) {
  if (!log || !debug) return;

  log->println(F("[DRV][HondeSoilSMTES4in1] Parsed response:"), true);

  log->print(F("  moisture raw = "), true);
  log->print((unsigned int)rawMoisture, true, "", DEC);
  log->print(F(" | moisture = "), true);
  log->print(moisture, true, " %", 2);
  log->println("", true);

  log->print(F("  temperature raw = "), true);
  log->print((int)rawTemperature, true, "", DEC);
  log->print(F(" | temperature = "), true);
  log->print(temperature, true, " C", 2);
  log->println("", true);

  log->print(F("  EC raw = "), true);
  log->print((unsigned int)rawEc, true, "", DEC);
  log->print(F(" | EC = "), true);
  log->print(ec, true, " uS/cm", 0);
  log->println("", true);

  log->print(F("  salinity raw = "), true);
  log->print((unsigned int)rawSalinity, true, "", DEC);
  log->print(F(" | salinity = "), true);
  log->print(salinity, true, " ppm", 0);
  log->println("", true);
}

HondeSoilSMTES4in1::HondeSoilSMTES4in1(RS485Bus& bus,
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
      _lastParsedFrame(false),
      soil_moisture(0.0),
      soil_temperature(0.0),
      soil_ec(0.0),
      soil_salinity(0.0) {}

void HondeSoilSMTES4in1::setFallbackValues() {
  soil_moisture = -99.0;
  soil_temperature = -99.0;
  soil_ec = -99.0;
  soil_salinity = -99.0;
}

bool HondeSoilSMTES4in1::validateSoilData() const {
  if (isnan(soil_moisture) ||
      isnan(soil_temperature) ||
      isnan(soil_ec) ||
      isnan(soil_salinity)) {
    return false;
  }

  if (soil_moisture < 0.0 || soil_moisture > 100.0) return false;
  if (soil_temperature < -40.0 || soil_temperature > 80.0) return false;
  if (soil_ec < 0.0 || soil_ec > 30000.0) return false;
  if (soil_salinity < 0.0 || soil_salinity > 10000.0) return false;
  return true;
}

bool HondeSoilSMTES4in1::readSoilData(uint8_t driverRetries,
                                      uint16_t readTimeoutMs,
                                      uint16_t afterReqDelayMs) {
  if (driverRetries == 0) driverRetries = 1;
  _lastParsedFrame = false;

  for (uint8_t attempt = 1; attempt <= driverRetries; ++attempt) {
    uint8_t request[READ_REQUEST_SIZE] = {
      _address, 0x03, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00
    };

    uint8_t response[READ_RESPONSE_SIZE] = {0};
    const uint8_t check[READ_CHECK_SIZE] = {_address, 0x03, 0x08};

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

    const uint16_t rawMoisture = ((uint16_t)response[3] << 8) | response[4];
    const int16_t rawTemperature = (int16_t)(((uint16_t)response[5] << 8) | response[6]);
    const uint16_t rawEc = ((uint16_t)response[7] << 8) | response[8];
    const uint16_t rawSalinity = ((uint16_t)response[9] << 8) | response[10];

    soil_moisture = (double)rawMoisture / 100.0;
    soil_temperature = (double)rawTemperature / 100.0;
    soil_ec = (double)rawEc;
    soil_salinity = (double)rawSalinity;

    logParsedHondeSoil4in1(_bus.getLogger(),
                           _debugEnable,
                           rawMoisture,
                           rawTemperature,
                           rawEc,
                           rawSalinity,
                           soil_moisture,
                           soil_temperature,
                           soil_ec,
                           soil_salinity);

    if (validateSoilData()) {
      return true;
    }

    if (_bus.getLogger() && _debugEnable) {
      _bus.getLogger()->println(F("[DRV][HondeSoilSMTES4in1] Range check fail: soil data"), true);
    }
  }

  return false;
}

bool HondeSoilSMTES4in1::readData() {
  markReadTime(millis());

  const uint8_t driverRetries = SENSOR_DEFAULT_DRIVER_RETRIES;
  bool gotAnyValidFrame = false;

  for (uint8_t attempt = 1; attempt <= driverRetries; ++attempt) {
    if (readSoilData(1, SENSOR_DEFAULT_READ_TIMEOUT_MS, SENSOR_DEFAULT_AFTER_REQ_MS)) {
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

bool HondeSoilSMTES4in1::changeAddress(uint8_t newAddress,
                                       uint8_t maxRetries,
                                       uint16_t readTimeoutMs,
                                       uint16_t afterReqDelayMs) {
  if (newAddress == 0 || newAddress > 247) {
    return false;
  }

  // Honde RD-SMTES manual:
  //   register 0x0200 = device address
  // Keep only the target sensor connected while this command is enabled.
  uint8_t request[8] = {_address, 0x06, 0x02, 0x00, 0x00, newAddress, 0x00, 0x00};
  uint8_t response[8] = {0};
  const uint8_t check[4] = {_address, 0x06, 0x02, 0x00};

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

uint8_t HondeSoilSMTES4in1::scanForAddress(uint8_t startAddr,
                                           uint8_t endAddr,
                                           uint16_t readTimeoutMs,
                                           uint16_t afterReqDelayMs) {
  if (startAddr == 0) startAddr = 1;
  if (endAddr > 247) endAddr = 247;
  if (startAddr > endAddr) return 0;

  for (uint16_t addr = startAddr; addr <= endAddr; ++addr) {
    uint8_t request[READ_REQUEST_SIZE] = {
      (uint8_t)addr, 0x03, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00
    };

    uint8_t response[READ_RESPONSE_SIZE] = {0};
    const uint8_t check[READ_CHECK_SIZE] = {(uint8_t)addr, 0x03, 0x08};

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
