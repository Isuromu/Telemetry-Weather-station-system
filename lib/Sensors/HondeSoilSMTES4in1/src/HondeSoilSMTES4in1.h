#pragma once
#include <Arduino.h>
#include "RS485Modbus.h"
#include "SensorDriver.h"
#include "Configuration_System.h"

/*
  HondeSoilSMTES4in1

  Driver intent:
  - RS485 Modbus RTU driver for the Honde RD-SMTES soil temperature,
    moisture, EC, and salinity 4-in-1 sensor.
  - Primary measurements:
      soil_moisture    -> %
      soil_temperature -> degrees Celsius
      soil_ec          -> uS/cm
      soil_salinity    -> ppm

  Main read command (readData):
  - Function: 0x03 (Read Holding Registers)
  - Start register: 0x0000
  - Count: 4 registers
  - Payload map:
      reg 0x0000 = soil moisture, unsigned, /100
      reg 0x0001 = soil temperature, signed 16-bit, /100
      reg 0x0002 = EC conductivity, unsigned, uS/cm
      reg 0x0003 = salinity/TDS, unsigned, ppm

  Configuration command:
  - Function: 0x06
  - Register 0x0200 = Modbus device address
*/
class HondeSoilSMTES4in1 : public SensorDriver {
public:
  double soil_moisture;
  double soil_temperature;
  double soil_ec;
  double soil_salinity;

  HondeSoilSMTES4in1(RS485Bus& bus,
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

  bool readSoilData(uint8_t driverRetries = SENSOR_DEFAULT_DRIVER_RETRIES,
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

  bool validateSoilData() const;

  static const uint8_t READ_REQUEST_SIZE = 8;
  static const uint8_t READ_RESPONSE_SIZE = 13;
  static const uint8_t READ_CHECK_SIZE = 3;
};
