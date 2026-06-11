#pragma once
#include <Arduino.h>
#include "RS485Modbus.h"
#include "SensorDriver.h"
#include "Configuration_System.h"

class JXCTModbusSensorBase : public SensorDriver {
public:
  bool changeAddress(uint8_t newAddress,
                     uint8_t maxRetries = SENSOR_DEFAULT_BUS_RETRIES,
                     uint16_t readTimeoutMs = 500,
                     uint16_t afterReqDelayMs = SENSOR_DEFAULT_AFTER_REQ_MS);

  uint8_t scanForAddress(uint8_t startAddr = 1,
                         uint8_t endAddr = 247,
                         uint16_t readTimeoutMs = 150,
                         uint16_t afterReqDelayMs = SENSOR_DEFAULT_AFTER_REQ_MS);

protected:
  static const uint8_t MAX_READ_REGISTERS = 24;

  JXCTModbusSensorBase(RS485Bus& bus,
                        const char* sensorId,
                        uint8_t address,
                        uint16_t scanRegister,
                        bool debugEnable,
                        uint8_t powerLineIndex,
                        uint8_t interfaceIndex,
                        uint16_t sampleRateMin,
                        uint32_t warmUpTimeMs,
                        uint8_t maxConsecutiveErrors,
                        uint32_t minUsefulPowerOffMs);

  bool readRegisterBlock(uint16_t startRegister,
                         uint8_t registerCount,
                         uint16_t* words,
                         uint8_t driverRetries,
                         uint16_t readTimeoutMs,
                         uint16_t afterReqDelayMs);

  bool writeSingleRegisterEcho(uint16_t registerAddress,
                               uint16_t value,
                               bool acceptValueAsAddress,
                               uint8_t maxRetries,
                               uint16_t readTimeoutMs,
                               uint16_t afterReqDelayMs);

  void logParsedU16(const __FlashStringHelper* driverLabel,
                    const __FlashStringHelper* valueLabel,
                    uint16_t raw,
                    double value,
                    const __FlashStringHelper* unit,
                    uint8_t decimals) const;

  static bool isFaultRaw16(uint16_t raw);
  static double scaleU16(uint16_t raw, double divisor);
  static uint32_t joinU32(uint16_t highWord, uint16_t lowWord);

  RS485Bus& _bus;
  uint16_t _scanRegister;
  bool _lastParsedFrame;
};
