#pragma once
#include <Arduino.h>
#include "RS485Modbus.h"
#include "SensorDriver.h"
#include "Configuration_System.h"

/*
  SmallLeafTemperatureHumidity

  Driver intent:
  - RS485 Modbus RTU driver for the smaller leaf temperature/humidity sensor
    used in the Telemetry Weather Station.
  - This tested small sensor batch uses raw / 100 for both values.

  Main read command:
  - Function: 0x03 (Read Holding Registers)
  - Start register: 0x0000
  - Count: 2 registers
  - Payload map:
      reg 0x0000 = leaf temperature, signed 16-bit, /100 C
      reg 0x0001 = leaf humidity, unsigned 16-bit, /100 %RH

  Example from hardware log:
  - Response payload 0x097B 0x12BF means:
      temperature = 2427 / 100 = 24.27 C
      humidity    = 4799 / 100 = 47.99 %RH

  Configuration command:
  - Function: 0x06
  - Register 0x0030 = Modbus device address
*/
class SmallLeafTemperatureHumidity : public SensorDriver {
public:
  int16_t leaf_temperature_raw;
  uint16_t leaf_humidity_raw;
  double leaf_temperature;
  double leaf_humidity;

  SmallLeafTemperatureHumidity(RS485Bus& bus,
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

  bool readHumidityTemperature(uint8_t driverRetries = SENSOR_DEFAULT_DRIVER_RETRIES,
                               uint16_t readTimeoutMs = SENSOR_DEFAULT_READ_TIMEOUT_MS,
                               uint16_t afterReqDelayMs = SENSOR_DEFAULT_AFTER_REQ_MS);

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
  bool _lastParsedFrame;

  bool validateHumidityTemperature() const;

  static const uint8_t READ_REQUEST_SIZE = 8;
  static const uint8_t READ_RESPONSE_SIZE = 9;
  static const uint8_t READ_CHECK_SIZE = 3;
};
