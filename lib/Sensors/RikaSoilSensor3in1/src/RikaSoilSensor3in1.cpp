#include "RikaSoilSensor3in1.h"

/*
  Main measurement frame (readData / scanForAddress):

  Request:
    [addr][0x03][0x00][0x00][0x00][0x03][crc_lo][crc_hi]

  Response:
    [addr][0x03][0x06]
    [temp_hi][temp_lo][vwc_hi][vwc_lo][ec_hi][ec_lo]
    [crc_lo][crc_hi]
*/
RikaSoilSensor3in1::RikaSoilSensor3in1(RS485Bus& bus,
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
      soil_temp(0.0),
      soil_vwc(0.0),
      soil_ec(0.0),
      epsilon(0.0),
      currentSoilType(SOIL_UNKNOWN) {}

void RikaSoilSensor3in1::setFallbackValues() {
  // Sentinels used to indicate invalid/unavailable values after full retry failure.
  soil_temp = -99.0;
  soil_vwc  = -99.0;
  soil_ec   = -99.0;
  epsilon   = -99.0;
  currentSoilType = SOIL_UNKNOWN;
}

bool RikaSoilSensor3in1::readData() {
  // Start-of-cycle timestamp tracked by SensorDriver.
  markReadTime(millis());

  // Driver-level retry count; each loop performs a full bus transaction.
  const uint8_t DRIVER_RETRIES = SENSOR_DEFAULT_DRIVER_RETRIES;

  for (uint8_t driverAttempt = 1; driverAttempt <= DRIVER_RETRIES; ++driverAttempt) {
    // Read 3 holding registers from 0x0000:
    // temperature*10, VWC*10, EC*1000.
    uint8_t request[MAIN_REQUEST_SIZE] = {
      _address, 0x03, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00
    };

    uint8_t response[MAIN_RESPONSE_SIZE] = {0};
    // Prefix for quick frame selection before CRC validation.
    const uint8_t check[MAIN_CHECK_SIZE] = {_address, 0x03, 0x06};

    const bool ok = _bus.SendRequest(request,
                                     MAIN_REQUEST_SIZE,
                                     response,
                                     MAIN_RESPONSE_SIZE,
                                     check,
                                     MAIN_CHECK_SIZE,
                                     SENSOR_DEFAULT_BUS_RETRIES,
                                     SENSOR_DEFAULT_READ_TIMEOUT_MS,
                                     _debugEnable,
                                     SENSOR_DEFAULT_AFTER_REQ_MS);

    if (!ok) {
      continue;
    }

    // Modbus payload uses big-endian 16-bit registers.
    const int16_t rawTemp = (int16_t)(((uint16_t)response[3] << 8) | response[4]);
    const uint16_t rawVwc = ((uint16_t)response[5] << 8) | response[6];
    const uint16_t rawEc  = ((uint16_t)response[7] << 8) | response[8];

    // Sensor fixed-point scaling.
    soil_temp = (double)rawTemp / 10.0;
    soil_vwc  = (double)rawVwc / 10.0;
    soil_ec   = (double)rawEc / 1000.0;

    // Reject physically implausible values to avoid propagating bad frames.
    if (soil_temp < -40.0 || soil_temp > 80.0) {
      continue;
    }

    if (soil_vwc < 0.0 || soil_vwc > 100.0) {
      continue;
    }

    if (soil_ec < 0.0 || soil_ec > 50.0) {
      continue;
    }

    markSuccess();
    return true;
  }

  // All driver attempts exhausted.
  setFallbackValues();
  markFailure();
  return false;
}

bool RikaSoilSensor3in1::changeAddress(uint8_t newAddress,
                                       uint8_t maxRetries,
                                       uint16_t readTimeoutMs,
                                       uint16_t afterReqDelayMs) {
  // Sensor address programming command:
  // function 0x06, register 0x0200, value=newAddress.
  uint8_t request[8] = {0xFE, 0x06, 0x02, 0x00, 0x00, newAddress, 0x00, 0x00};
  uint8_t response[8] = {0};
  const uint8_t check[4] = {0xFE, 0x06, 0x02, 0x00};

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

  if (ok) {
    _address = newAddress;
  }

  return ok;
}

uint8_t RikaSoilSensor3in1::scanForAddress(uint8_t startAddr,
                                           uint8_t endAddr,
                                           uint16_t readTimeoutMs,
                                           uint16_t afterReqDelayMs) {
  // Legal Modbus RTU address range clamp.
  if (startAddr == 0) startAddr = 1;
  if (endAddr > 247) endAddr = 247;
  if (startAddr > endAddr) return 0;

  for (uint16_t addr = startAddr; addr <= endAddr; ++addr) {
    // Probe each candidate using normal measurement read request.
    uint8_t request[MAIN_REQUEST_SIZE] = {
      (uint8_t)addr, 0x03, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00
    };

    uint8_t response[MAIN_RESPONSE_SIZE] = {0};
    const uint8_t check[MAIN_CHECK_SIZE] = {(uint8_t)addr, 0x03, 0x06};

    const bool found = _bus.SendRequest(request,
                                        MAIN_REQUEST_SIZE,
                                        response,
                                        MAIN_RESPONSE_SIZE,
                                        check,
                                        MAIN_CHECK_SIZE,
                                        1,
                                        readTimeoutMs,
                                        _debugEnable,
                                        afterReqDelayMs);
    if (found) {
      // Persist discovered address for subsequent operations.
      _address = (uint8_t)addr;
      return (uint8_t)addr;
    }

    // Brief pacing delay to keep bus scan stable.
    delay(5);
  }

  return 0;
}

bool RikaSoilSensor3in1::readSoilType(SoilType &type,
                                      uint8_t driverRetries,
                                      uint16_t readTimeoutMs,
                                      uint16_t afterReqDelayMs) {
  // Guard against accidental zero retries from caller.
  if (driverRetries == 0) driverRetries = 1;

  for (uint8_t driverAttempt = 1; driverAttempt <= driverRetries; ++driverAttempt) {
    // Holding register 0x0020 contains soil type code.
    uint8_t request[8] = {_address, 0x03, 0x00, 0x20, 0x00, 0x01, 0x00, 0x00};
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

    const uint16_t rawType = ((uint16_t)response[3] << 8) | response[4];

    // Normalize raw register value into strongly typed enum.
    switch (rawType) {
      case 0: type = SOIL_MINERAL; break;
      case 1: type = SOIL_SANDY;   break;
      case 2: type = SOIL_CLAY;    break;
      case 3: type = SOIL_ORGANIC; break;
      default: type = SOIL_UNKNOWN; break;
    }

    currentSoilType = type;
    return true;
  }

  type = SOIL_UNKNOWN;
  currentSoilType = SOIL_UNKNOWN;
  return false;
}

bool RikaSoilSensor3in1::setSoilType(SoilType type,
                                     uint8_t driverRetries,
                                     uint16_t readTimeoutMs,
                                     uint16_t afterReqDelayMs) {
  if (driverRetries == 0) driverRetries = 1;

  for (uint8_t driverAttempt = 1; driverAttempt <= driverRetries; ++driverAttempt) {
    // Write enum value to holding register 0x0020.
    uint8_t request[8] = {_address, 0x06, 0x00, 0x20, 0x00, (uint8_t)type, 0x00, 0x00};
    uint8_t response[8] = {0};
    const uint8_t check[4] = {_address, 0x06, 0x00, 0x20};

    const bool ok = _bus.SendRequest(request,
                                     8,
                                     response,
                                     8,
                                     check,
                                     4,
                                     SENSOR_DEFAULT_BUS_RETRIES,
                                     readTimeoutMs,
                                     _debugEnable,
                                     afterReqDelayMs);
    if (ok) {
      currentSoilType = type;
      return true;
    }
  }

  return false;
}

bool RikaSoilSensor3in1::readEpsilon(double &value,
                                     uint8_t driverRetries,
                                     uint16_t readTimeoutMs,
                                     uint16_t afterReqDelayMs) {
  if (driverRetries == 0) driverRetries = 1;

  for (uint8_t driverAttempt = 1; driverAttempt <= driverRetries; ++driverAttempt) {
    // Input register 0x0005 stores epsilon * 100.
    uint8_t request[8] = {_address, 0x04, 0x00, 0x05, 0x00, 0x01, 0x00, 0x00};
    uint8_t response[7] = {0};
    const uint8_t check[3] = {_address, 0x04, 0x02};

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

    const uint16_t raw = ((uint16_t)response[3] << 8) | response[4];
    // Convert from fixed-point register format.
    value = (double)raw / 100.0;
    epsilon = value;
    return true;
  }

  value = -99.0;
  epsilon = -99.0;
  return false;
}

bool RikaSoilSensor3in1::setCompensationCoeff(uint16_t regAddress,
                                              uint16_t coeffValue,
                                              uint8_t driverRetries,
                                              uint16_t readTimeoutMs,
                                              uint16_t afterReqDelayMs) {
  if (driverRetries == 0) driverRetries = 1;

  // Keep register split explicit; Modbus request frame uses separate high/low bytes.
  const uint8_t regHigh = (uint8_t)((regAddress >> 8) & 0xFF);
  const uint8_t regLow  = (uint8_t)(regAddress & 0xFF);

  for (uint8_t driverAttempt = 1; driverAttempt <= driverRetries; ++driverAttempt) {
    // Function 0x06 writes one register (coefficient raw fixed-point value).
    uint8_t request[8] = {
      _address, 0x06, regHigh, regLow,
      (uint8_t)((coeffValue >> 8) & 0xFF),
      (uint8_t)(coeffValue & 0xFF),
      0x00, 0x00
    };

    uint8_t response[8] = {0};
    const uint8_t check[4] = {_address, 0x06, regHigh, regLow};

    const bool ok = _bus.SendRequest(request,
                                     8,
                                     response,
                                     8,
                                     check,
                                     4,
                                     SENSOR_DEFAULT_BUS_RETRIES,
                                     readTimeoutMs,
                                     _debugEnable,
                                     afterReqDelayMs);
    if (ok) {
      return true;
    }
  }

  return false;
}

bool RikaSoilSensor3in1::readCompensationCoeff(uint16_t regAddress,
                                               double &coeffValue,
                                               uint8_t driverRetries,
                                               uint16_t readTimeoutMs,
                                               uint16_t afterReqDelayMs) {
  if (driverRetries == 0) driverRetries = 1;

  // Keep register split explicit; Modbus request frame uses separate high/low bytes.
  const uint8_t regHigh = (uint8_t)((regAddress >> 8) & 0xFF);
  const uint8_t regLow  = (uint8_t)(regAddress & 0xFF);

  for (uint8_t driverAttempt = 1; driverAttempt <= driverRetries; ++driverAttempt) {
    // Function 0x03 reads one holding register at regAddress.
    uint8_t request[8] = {_address, 0x03, regHigh, regLow, 0x00, 0x01, 0x00, 0x00};
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

    const uint16_t raw = ((uint16_t)response[3] << 8) | response[4];
    // Convention in this driver: coefficient registers are scaled by 100.
    coeffValue = (double)raw / 100.0;
    return true;
  }

  coeffValue = -99.0;
  return false;
}

const char* RikaSoilSensor3in1::soilTypeToString(SoilType type) {
  switch (type) {
    case SOIL_MINERAL: return "Mineral Soil";
    case SOIL_SANDY:   return "Sandy Soil";
    case SOIL_CLAY:    return "Clay Soil";
    case SOIL_ORGANIC: return "Organic Soil";
    default:           return "Unknown";
  }
}
