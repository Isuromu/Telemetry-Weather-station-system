#include "HondeUltrasonicWind2in1.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static bool bufferContainsText(const uint8_t* buffer, size_t length, const char* text) {
  if (!buffer || !text) return false;

  const size_t textLen = strlen(text);
  if (textLen == 0 || length < textLen) return false;

  for (size_t offset = 0; offset <= (length - textLen); ++offset) {
    if (memcmp(&buffer[offset], text, textLen) == 0) {
      return true;
    }
  }

  return false;
}

static float decodeHondeFloat(uint8_t d3, uint8_t d2, uint8_t d1, uint8_t d0) {
  const uint32_t raw = ((uint32_t)d3 << 24) |
                       ((uint32_t)d2 << 16) |
                       ((uint32_t)d1 << 8) |
                       (uint32_t)d0;

  float value = 0.0f;
  memcpy(&value, &raw, sizeof(value));
  return value;
}

static void logParsedHondeWind2in1(PrintController* log,
                                   bool debug,
                                   uint8_t dirHi,
                                   uint8_t dirLo,
                                   uint8_t speedD3,
                                   uint8_t speedD2,
                                   uint8_t speedD1,
                                   uint8_t speedD0,
                                   uint16_t rawDirection,
                                   double windSpeed) {
  if (!log || !debug) return;

  log->println(F("[DRV][HondeWind2in1] Parsed wind response:"), true);

  log->print(F("  bytes [3],[4] -> direction raw = 0x"), true);
  log->print((unsigned int)dirHi, true, "", HEX);
  log->print((unsigned int)dirLo, true, " ", HEX);
  log->print(F("| direction = "), true);
  log->print((unsigned int)rawDirection, true, " deg", DEC);
  log->println("", true);

  log->print(F("  float bytes D3 D2 D1 D0 -> "), true);
  log->print((unsigned int)speedD3, true, " ", HEX);
  log->print((unsigned int)speedD2, true, " ", HEX);
  log->print((unsigned int)speedD1, true, " ", HEX);
  log->print((unsigned int)speedD0, true, "", HEX);
  log->print(F(" | wind speed = "), true);
  log->print(windSpeed, true, " m/s", 2);
  log->println("", true);
}

HondeUltrasonicWind2in1::HondeUltrasonicWind2in1(RS485Bus& bus,
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
      wind_speed(0.0),
      wind_direction(0) {}

void HondeUltrasonicWind2in1::setFallbackValues() {
  wind_speed = -99.0;
  wind_direction = 0xFFFF;
}

bool HondeUltrasonicWind2in1::validateWindData() const {
  if (isnan(wind_speed)) return false;
  if (wind_speed < 0.0 || wind_speed > 70.0) return false;
  if (wind_direction > 359) return false;
  return true;
}

bool HondeUltrasonicWind2in1::readData() {
  markReadTime(millis());

  const uint8_t driverRetries = SENSOR_DEFAULT_DRIVER_RETRIES;

  for (uint8_t attempt = 1; attempt <= driverRetries; ++attempt) {
    uint8_t request[READ_REQUEST_SIZE] = {
      _address, 0x03, 0x00, 0x01, 0x00, 0x03, 0x00, 0x00
    };

    uint8_t response[READ_RESPONSE_SIZE] = {0};
    const uint8_t check[READ_CHECK_SIZE] = {_address, 0x03, 0x06};

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

    const uint8_t dirHi = response[3];
    const uint8_t dirLo = response[4];

    // The response order for wind speed is reg 0x0002 then reg 0x0003:
    // D1, D0, D3, D2. Reorder to D3, D2, D1, D0 for IEEE754 decoding.
    const uint8_t speedD1 = response[5];
    const uint8_t speedD0 = response[6];
    const uint8_t speedD3 = response[7];
    const uint8_t speedD2 = response[8];

    wind_direction = ((uint16_t)dirHi << 8) | dirLo;
    wind_speed = (double)decodeHondeFloat(speedD3, speedD2, speedD1, speedD0);

    logParsedHondeWind2in1(_bus.getLogger(),
                           _debugEnable,
                           dirHi,
                           dirLo,
                           speedD3,
                           speedD2,
                           speedD1,
                           speedD0,
                           wind_direction,
                           wind_speed);

    if (validateWindData()) {
      markSuccess();
      return true;
    }

    if (_bus.getLogger() && _debugEnable) {
      _bus.getLogger()->println(F("[DRV][HondeWind2in1] Range check fail: wind data"), true);
    }
  }

  setFallbackValues();
  markFailure();
  return false;
}

bool HondeUltrasonicWind2in1::sendAsciiConfigCommand(const char* command,
                                                     const char* expectedText,
                                                     uint8_t maxRetries,
                                                     uint16_t readTimeoutMs,
                                                     uint16_t afterReqDelayMs) {
  if (!command || !expectedText) return false;
  if (maxRetries == 0) maxRetries = 1;

  const size_t commandLen = strlen(command);
  if (commandLen == 0 || commandLen > 31) return false;

  for (uint8_t attempt = 1; attempt <= maxRetries; ++attempt) {
    uint8_t request[32] = {0};
    memcpy(request, command, commandLen);

    // Honde compact weather station address setup is ASCII over RS485:
    //   >*      enter configure mode
    //   >ID n   set device address
    //   >!      return to normal mode
    //   >RESET  reboot
    // These frames are not Modbus RTU frames and do not carry CRC bytes.
    _bus.Request_RS485(request, commandLen, afterReqDelayMs, _debugEnable);
    const size_t bytesRead = _bus.Read_RS485(readTimeoutMs, _debugEnable);

    if (bufferContainsText(_bus.rawData(), bytesRead, expectedText)) {
      return true;
    }

    delay(100UL * attempt);
  }

  return false;
}

bool HondeUltrasonicWind2in1::changeAddress(uint8_t newAddress,
                                            uint8_t maxRetries,
                                            uint16_t readTimeoutMs,
                                            uint16_t afterReqDelayMs) {
  if (newAddress == 0 || newAddress > 247) {
    return false;
  }

  // Use this only with one Honde compact weather station on the bus. The ASCII
  // configuration mode is not addressed like a normal Modbus write.
  if (!sendAsciiConfigCommand(">*\r\n", "CONFIGURE MODE", maxRetries, readTimeoutMs, afterReqDelayMs)) {
    return false;
  }

  char idCommand[16] = {0};
  snprintf(idCommand, sizeof(idCommand), ">ID %u\r\n", (unsigned int)newAddress);
  if (!sendAsciiConfigCommand(idCommand, "CMD IS SET", maxRetries, readTimeoutMs, afterReqDelayMs)) {
    return false;
  }

  if (!sendAsciiConfigCommand(">!\r\n", "NORMAL MODE", maxRetries, readTimeoutMs, afterReqDelayMs)) {
    return false;
  }

  (void)sendAsciiConfigCommand(">RESET\r\n", "System start ok", 1, readTimeoutMs, afterReqDelayMs);

  _address = newAddress;
  return true;
}

uint8_t HondeUltrasonicWind2in1::scanForAddress(uint8_t startAddr,
                                                uint8_t endAddr,
                                                uint16_t readTimeoutMs,
                                                uint16_t afterReqDelayMs) {
  if (startAddr == 0) startAddr = 1;
  if (endAddr > 247) endAddr = 247;
  if (startAddr > endAddr) return 0;

  for (uint16_t addr = startAddr; addr <= endAddr; ++addr) {
    uint8_t request[READ_REQUEST_SIZE] = {
      (uint8_t)addr, 0x03, 0x00, 0x01, 0x00, 0x03, 0x00, 0x00
    };

    uint8_t response[READ_RESPONSE_SIZE] = {0};
    const uint8_t check[READ_CHECK_SIZE] = {(uint8_t)addr, 0x03, 0x06};

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
