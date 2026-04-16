#pragma once
#include <Arduino.h>
#include "RS485Modbus.h"
#include "SensorDriver.h"
#include "Configuration_System.h"

/*
  HondeMiniWeather5in1

  Driver intent:
  - RS485 Modbus RTU driver for the Honde HD-MWSM5-01 MINI compact weather
    station when configured as a 5-in-1 wind and air sensor.
  - Primary measurements:
      air_temp       -> degrees Celsius
      air_humidity   -> relative humidity, %
      air_pressure   -> hPa
      wind_speed     -> meters per second
      wind_direction -> actual wind direction, degrees

  Main read command (readData):
  - Function: 0x03 (Read Holding Registers)
  - Start register: 0x0000
  - Count: 13 registers
  - Payload map used by this driver:
      reg 0x0000 = digital temperature, signed, /10
      reg 0x0001 = digital humidity, signed, /10
      reg 0x0006 = atmospheric pressure, signed, /10
      reg 0x000B = wind speed, unsigned, /100
      reg 0x000C = actual wind direction, signed integer degrees
*/
class HondeMiniWeather5in1 : public SensorDriver {
public:
  double air_temp;
  double air_humidity;
  double air_pressure;
  double wind_speed;
  uint16_t wind_direction;

  HondeMiniWeather5in1(RS485Bus& bus,
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

  // Writes the Modbus device address to register 0x0020.
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

  bool validateWeatherData() const;

  static int16_t readSignedRegister(const uint8_t response[], uint8_t registerIndex);
  static uint16_t readUnsignedRegister(const uint8_t response[], uint8_t registerIndex);

  static const uint16_t INVALID_REGISTER_VALUE = 0x7FFF;

  static const uint8_t READ_REQUEST_SIZE = 8;
  static const uint8_t READ_RESPONSE_SIZE = 31;
  static const uint8_t READ_CHECK_SIZE = 3;

  static const uint8_t SCAN_RESPONSE_SIZE = 9;
};
