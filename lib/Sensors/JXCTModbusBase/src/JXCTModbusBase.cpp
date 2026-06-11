#include "JXCTModbusBase.h"

JXCTModbusSensorBase::JXCTModbusSensorBase(RS485Bus& bus,
                                             const char* sensorId,
                                             uint8_t address,
                                             uint16_t scanRegister,
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
      _scanRegister(scanRegister),
      _lastParsedFrame(false) {}

bool JXCTModbusSensorBase::readRegisterBlock(uint16_t startRegister,
                                              uint8_t registerCount,
                                              uint16_t* words,
                                              uint8_t driverRetries,
                                              uint16_t readTimeoutMs,
                                              uint16_t afterReqDelayMs) {
  if (!words || registerCount == 0 || registerCount > MAX_READ_REGISTERS) {
    return false;
  }
  if (driverRetries == 0) driverRetries = 1;
  _lastParsedFrame = false;

  const uint8_t responseSize = (uint8_t)(5 + (2 * registerCount));

  for (uint8_t attempt = 1; attempt <= driverRetries; ++attempt) {
    uint8_t request[8] = {
      _address,
      0x03,
      (uint8_t)(startRegister >> 8),
      (uint8_t)(startRegister & 0xFF),
      0x00,
      registerCount,
      0x00,
      0x00
    };
    uint8_t response[5 + (2 * MAX_READ_REGISTERS)] = {0};
    const uint8_t check[3] = {_address, 0x03, (uint8_t)(2 * registerCount)};

    const bool ok = _bus.SendRequest(request,
                                     sizeof(request),
                                     response,
                                     responseSize,
                                     check,
                                     sizeof(check),
                                     SENSOR_DEFAULT_BUS_RETRIES,
                                     readTimeoutMs,
                                     _debugEnable,
                                     afterReqDelayMs);
    if (!ok) {
      continue;
    }

    _lastParsedFrame = true;
    for (uint8_t i = 0; i < registerCount; ++i) {
      words[i] = ((uint16_t)response[3 + (2 * i)] << 8) | response[4 + (2 * i)];
    }
    return true;
  }

  return false;
}

bool JXCTModbusSensorBase::writeSingleRegisterEcho(uint16_t registerAddress,
                                                    uint16_t value,
                                                    bool acceptValueAsAddress,
                                                    uint8_t maxRetries,
                                                    uint16_t readTimeoutMs,
                                                    uint16_t afterReqDelayMs) {
  if (maxRetries == 0) maxRetries = 1;

  uint8_t request[8] = {
    _address,
    0x06,
    (uint8_t)(registerAddress >> 8),
    (uint8_t)(registerAddress & 0xFF),
    (uint8_t)(value >> 8),
    (uint8_t)(value & 0xFF),
    0x00,
    0x00
  };
  const uint8_t oldAddress = _address;
  const uint8_t possibleNewAddress = (uint8_t)value;

  for (uint8_t attempt = 1; attempt <= maxRetries; ++attempt) {
    _bus.CRC_Calc(request, sizeof(request), _debugEnable);
    _bus.Request_RS485(request, sizeof(request), afterReqDelayMs, _debugEnable);

    const size_t bytesRead = _bus.Read_RS485(readTimeoutMs, _debugEnable);
    const uint8_t* raw = _bus.rawData();

    if (bytesRead >= 8 && raw) {
      for (size_t offset = 0; offset <= (bytesRead - 8); ++offset) {
        const uint8_t* frame = &raw[offset];
        const bool addressOk =
            frame[0] == oldAddress ||
            (acceptValueAsAddress && frame[0] == possibleNewAddress);
        const bool echoOk =
            addressOk &&
            frame[1] == 0x06 &&
            frame[2] == (uint8_t)(registerAddress >> 8) &&
            frame[3] == (uint8_t)(registerAddress & 0xFF) &&
            frame[4] == (uint8_t)(value >> 8) &&
            frame[5] == (uint8_t)(value & 0xFF) &&
            RS485Bus::verifyCrc16ModbusFrame(frame, 8);

        if (echoOk) {
          return true;
        }
      }
    }

    delay(100UL * attempt);
  }

  return false;
}

bool JXCTModbusSensorBase::changeAddress(uint8_t newAddress,
                                          uint8_t maxRetries,
                                          uint16_t readTimeoutMs,
                                          uint16_t afterReqDelayMs) {
  if (newAddress == 0 || newAddress > 247) {
    return false;
  }

  if (writeSingleRegisterEcho(0x0100, newAddress, true, maxRetries, readTimeoutMs, afterReqDelayMs)) {
    _address = newAddress;
    return true;
  }

  return false;
}

uint8_t JXCTModbusSensorBase::scanForAddress(uint8_t startAddr,
                                              uint8_t endAddr,
                                              uint16_t readTimeoutMs,
                                              uint16_t afterReqDelayMs) {
  if (startAddr == 0) startAddr = 1;
  if (endAddr > 247) endAddr = 247;
  if (startAddr > endAddr) return 0;

  for (uint16_t addr = startAddr; addr <= endAddr; ++addr) {
    uint8_t request[8] = {
      (uint8_t)addr,
      0x03,
      (uint8_t)(_scanRegister >> 8),
      (uint8_t)(_scanRegister & 0xFF),
      0x00,
      0x01,
      0x00,
      0x00
    };
    uint8_t response[7] = {0};
    const uint8_t check[3] = {(uint8_t)addr, 0x03, 0x02};

    const bool found = _bus.SendRequest(request,
                                        sizeof(request),
                                        response,
                                        sizeof(response),
                                        check,
                                        sizeof(check),
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

void JXCTModbusSensorBase::logParsedU16(const __FlashStringHelper* driverLabel,
                                         const __FlashStringHelper* valueLabel,
                                         uint16_t raw,
                                         double value,
                                         const __FlashStringHelper* unit,
                                         uint8_t decimals) const {
  if (!_bus.getLogger() || !_debugEnable) return;

  _bus.getLogger()->print(F("[DRV]["), true);
  _bus.getLogger()->print(driverLabel, true);
  _bus.getLogger()->print(F("] "), true);
  _bus.getLogger()->print(valueLabel, true);
  _bus.getLogger()->print(F(" raw = "), true);
  _bus.getLogger()->print((unsigned int)raw, true, "", DEC);
  _bus.getLogger()->print(F(" | value = "), true);
  _bus.getLogger()->print(value, true, "", decimals);
  if (unit) {
    _bus.getLogger()->print(F(" "), true);
    _bus.getLogger()->print(unit, true);
  }
  _bus.getLogger()->println("", true);
}

bool JXCTModbusSensorBase::isFaultRaw16(uint16_t raw) {
  return raw == 0xFFFFU || raw == 0x7FFFU || raw == 0x8000U;
}

double JXCTModbusSensorBase::scaleU16(uint16_t raw, double divisor) {
  return (double)raw / (divisor > 0.0 ? divisor : 1.0);
}

uint32_t JXCTModbusSensorBase::joinU32(uint16_t highWord, uint16_t lowWord) {
  return ((uint32_t)highWord << 16) | (uint32_t)lowWord;
}