#pragma once
#include <Arduino.h>
#include "RS485Modbus.h"
#include "SensorDriver.h"
#include "Configuration_System.h"

/*
  JXBS_LiquidPH

  Driver intent:
  - RS485 Modbus RTU driver for the JXBS-3001-PH-RS liquid pH sensor.
  - Primary measurements:
      liquid_temperature -> degrees Celsius
      liquid_ph          -> pH

  Manual:
  - "JXBS-3001-PH-RS Water PH Sensor User Manual"
  - Version 2.0, 2020-01-24

  Electrical / installation notes from the manual:
  - Power supply: 12-24 VDC
  - Serial format: 8 data bits, no parity, 1 stop bit
  - Baud rate: 2400 / 4800 / 9600, factory default 9600
  - Manual wire colors:
      Brown       -> power supply +
      Black       -> power supply -
      Yellow/Grey -> RS485-A
      Blue        -> RS485-B
  - Some supplied cable sets may use red/black/yellow/green instead. If so,
    treat red as likely V+, black as V-, yellow as likely RS485-A, and green
    as likely RS485-B only after checking the sensor label or continuity.
  - Keep the pH electrode wet or in its protective cap/storage liquid when not
    installed. Do not let the electrode dry out.

  Main read command (readData):
  - Function: 0x03 (Read Holding Registers)
  - Start register: 0x0001
  - Count: 2 registers
  - Payload map:
      reg 0x0001 = temperature, signed/unsigned 16-bit practical raw, /10 C
      reg 0x0002 = pH, unsigned, /100 pH

  Single-register commands:
  - Temperature only: 0x03, start 0x0001, count 1
  - pH only:          0x03, start 0x0002, count 1

  Configuration command:
  - Function: 0x06
  - Register 0x0100 = Modbus device address
  - Register 0x0101 = baud rate, not implemented here.
*/
class JXBS_LiquidPH : public SensorDriver {
public:
  double liquid_temperature;
  double liquid_ph;

  JXBS_LiquidPH(RS485Bus& bus,
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

  bool readTemperaturePH(uint8_t driverRetries = SENSOR_DEFAULT_DRIVER_RETRIES,
                         uint16_t readTimeoutMs = SENSOR_DEFAULT_READ_TIMEOUT_MS,
                         uint16_t afterReqDelayMs = SENSOR_DEFAULT_AFTER_REQ_MS);

  bool readTemperature(uint8_t driverRetries = SENSOR_DEFAULT_DRIVER_RETRIES,
                       uint16_t readTimeoutMs = SENSOR_DEFAULT_READ_TIMEOUT_MS,
                       uint16_t afterReqDelayMs = SENSOR_DEFAULT_AFTER_REQ_MS);

  bool readPH(uint8_t driverRetries = SENSOR_DEFAULT_DRIVER_RETRIES,
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

  bool validateTemperaturePH() const;
  bool validateTemperature() const;
  bool validatePH() const;

  static const uint8_t READ_ONE_REQUEST_SIZE = 8;
  static const uint8_t READ_ONE_RESPONSE_SIZE = 7;
  static const uint8_t READ_TWO_REQUEST_SIZE = 8;
  static const uint8_t READ_TWO_RESPONSE_SIZE = 9;
  static const uint8_t READ_CHECK_SIZE = 3;
};
