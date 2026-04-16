#pragma once
#include <Arduino.h>
#include "RS485Modbus.h"
#include "SensorDriver.h"
#include "Configuration_System.h"

/*
  HondeUltrasonicWind2in1

  Driver intent:
  - RS485 Modbus RTU driver for the Honde ultrasonic compact weather station
    when only wind speed and wind direction are used.
  - Primary measurements:
      wind_direction -> degrees, 0..359
      wind_speed     -> meters per second, 0..70

  Main read command (readData):
  - Function: 0x03 (Read Holding Registers)
  - Start register: 0x0001 (manual "Register 2", zero-based address)
  - Count: 3 registers
  - Payload map:
      reg 0x0001 = wind direction, unsigned 16-bit integer
      reg 0x0002 + 0x0003 = wind speed, IEEE754 float

  Honde float byte order:
  - The manual labels the float bytes as D3 D2 D1 D0, but the two Modbus
    registers arrive as D1 D0 D3 D2. The driver reorders them before decoding.
*/
class HondeUltrasonicWind2in1 : public SensorDriver {
public:
  double wind_speed;
  uint16_t wind_direction;

  HondeUltrasonicWind2in1(RS485Bus& bus,
                          const char* sensorId,
                          uint8_t address,
                          bool debugEnable = false,
                          uint8_t powerLineIndex = 0,
                          uint8_t interfaceIndex = 0,
                          uint16_t sampleRateMin = 1,
                          uint32_t warmUpTimeMs = 500,
                          uint8_t maxConsecutiveErrors = 10,
                          uint32_t minUsefulPowerOffMs = 60000UL);

  bool readData() override;
  void setFallbackValues() override;

  // Uses the manufacturer's ASCII setup mode, not a Modbus register write.
  // For address setup, connect only the target sensor to the RS485 bus.
  bool changeAddress(uint8_t newAddress,
                     uint8_t maxRetries = SENSOR_DEFAULT_BUS_RETRIES,
                     uint16_t readTimeoutMs = 500,
                     uint16_t afterReqDelayMs = SENSOR_DEFAULT_AFTER_REQ_MS);

  uint8_t scanForAddress(uint8_t startAddr = 1,
                         uint8_t endAddr = 247,
                         uint16_t readTimeoutMs = 150,
                         uint16_t afterReqDelayMs = SENSOR_DEFAULT_AFTER_REQ_MS);

private:
  RS485Bus& _bus;

  bool validateWindData() const;
  bool sendAsciiConfigCommand(const char* command,
                              const char* expectedText,
                              uint8_t maxRetries,
                              uint16_t readTimeoutMs,
                              uint16_t afterReqDelayMs);

  static const uint8_t READ_REQUEST_SIZE = 8;
  static const uint8_t READ_RESPONSE_SIZE = 11;
  static const uint8_t READ_CHECK_SIZE = 3;
};
