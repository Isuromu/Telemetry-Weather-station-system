#pragma once
#include <Arduino.h>
#include "RS485Modbus.h"
#include "SensorDriver.h"
#include "Configuration_System.h"

/*
  JXBS_LeafSurfaceHumidity

  Driver intent:
  - RS485 Modbus RTU driver for JXBS-3001-YMSD / JXBS-style leaf surface
    humidity transmitters.
  - Primary measurements:
      leaf_humidity    -> %RH
      leaf_temperature -> degrees Celsius

  Main read command (readData):
  - Function: 0x03 (Read Holding Registers)
  - Start register: 0x0020
  - Count: 2 registers
  - Payload map:
      reg 0x0020 = leaf surface humidity, unsigned, /10
      reg 0x0021 = leaf temperature for this tested batch, unsigned,
                   raw * 0.05 - 80.0

  Temperature note:
  - This sensor batch does not match the older signed int16 /10 decoding.
  - Keep bytes [5],[6] big-endian; only the formula is different.
  - Equivalent formula: (raw - 1600) / 20.0

  Configuration command:
  - Function: 0x06
  - Register 0x0100 = Modbus device address
*/
class JXBS_LeafSurfaceHumidity : public SensorDriver {
public:
  double leaf_humidity;
  double leaf_temperature;

  JXBS_LeafSurfaceHumidity(RS485Bus& bus,
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
