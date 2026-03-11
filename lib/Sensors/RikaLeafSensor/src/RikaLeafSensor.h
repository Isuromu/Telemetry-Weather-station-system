#pragma once
#include <Arduino.h>
#include "RS485Modbus.h"
#include "SensorDriver.h"

class RikaLeafSensor : public SensorDriver {
public:
  double leaf_temp;
  double leaf_humid;

  RikaLeafSensor(RS485Bus& bus,
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

  bool changeAddress(uint8_t newAddress,
                     uint8_t maxRetries = 3,
                     uint16_t readTimeoutMs = 500,
                     uint16_t afterReqDelayMs = 20);

  uint8_t scanForAddress(uint8_t startAddr = 1,
                         uint8_t endAddr = 247,
                         uint16_t readTimeoutMs = 120,
                         uint16_t afterReqDelayMs = 20);

private:
  RS485Bus& _bus;
  static const uint8_t READ_REQUEST_SIZE = 8;
  static const uint8_t READ_RESPONSE_SIZE = 9;
  static const uint8_t READ_CHECK_SIZE = 3;
};