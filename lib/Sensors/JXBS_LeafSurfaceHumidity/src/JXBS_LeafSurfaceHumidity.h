#pragma once
#include <Arduino.h>
#include "RS485Modbus.h"
#include "SensorDriver.h"
#include "Configuration_System.h"

/*
  JXBS_LeafSurfaceHumidity

  Driver for JXBS-3001-YMSD / JXBS-style leaf surface humidity transmitter.

  Supplier family:
    - JXBS / JXCT style

  Interface:
    - RS485 / Modbus RTU

  Serial format:
    - 8N1

  Default baud:
    - 9600

  Default Modbus address:
    - 0x01

  Measured values:
    1) Leaf surface humidity
       - Register: 0x0020
       - Units: %RH
       - Raw scale: /10
       - Measurement range: 0..100 %RH
       - Resolution: 0.1 %RH
       - Accuracy: ±5 %RH (@25°C)

    2) Leaf surface temperature
       - Register: 0x0021
       - Units: °C
       - Raw scale: signed int16 /10
       - Measurement range: -20..80 °C
       - Resolution: 0.01 °C in brochure text, but Modbus example and scale usage are 0.1 °C
       - Accuracy: ±1 °C (@25°C)

  Register usage:
    - Read 2 registers starting from 0x0020:
      humidity + temperature in one request

  Supported write operation:
    - changeAddress() via register 0x0100

  Not implemented on purpose:
    - baud rate change
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
                         uint16_t readTimeoutMs = 120,
                         uint16_t afterReqDelayMs = SENSOR_DEFAULT_AFTER_REQ_MS);

private:
  RS485Bus& _bus;

  bool validateHumidityTemperature() const;
};