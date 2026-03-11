#pragma once
#include <Arduino.h>
#include "RS485Modbus.h"
#include "SensorDriver.h"
#include "Configuration_System.h"

class RikaSoilSensor3in1 : public SensorDriver {
public:
  enum SoilType : uint8_t {
    SOIL_MINERAL = 0,
    SOIL_SANDY   = 1,
    SOIL_CLAY    = 2,
    SOIL_ORGANIC = 3,
    SOIL_UNKNOWN = 255
  };

  double soil_temp;
  double soil_vwc;
  double soil_ec;
  double epsilon;
  SoilType currentSoilType;

  RikaSoilSensor3in1(RS485Bus& bus,
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

  bool readSoilType(SoilType &type,
                    uint8_t driverRetries = 3,
                    uint16_t readTimeoutMs = 500,
                    uint16_t afterReqDelayMs = 20);

  bool setSoilType(SoilType type,
                   uint8_t driverRetries = 3,
                   uint16_t readTimeoutMs = 500,
                   uint16_t afterReqDelayMs = 20);

  bool readEpsilon(double &value,
                   uint8_t driverRetries = 3,
                   uint16_t readTimeoutMs = 500,
                   uint16_t afterReqDelayMs = 20);

  bool setCompensationCoeff(uint16_t regAddress,
                            uint16_t coeffValue,
                            uint8_t driverRetries = 3,
                            uint16_t readTimeoutMs = 500,
                            uint16_t afterReqDelayMs = 20);

  bool readCompensationCoeff(uint16_t regAddress,
                             double &coeffValue,
                             uint8_t driverRetries = 3,
                             uint16_t readTimeoutMs = 500,
                             uint16_t afterReqDelayMs = 20);

  static const char* soilTypeToString(SoilType type);

private:
  RS485Bus& _bus;

  static const uint8_t MAIN_REQUEST_SIZE   = 8;
  static const uint8_t MAIN_RESPONSE_SIZE  = 11;
  static const uint8_t MAIN_CHECK_SIZE     = 3;
};