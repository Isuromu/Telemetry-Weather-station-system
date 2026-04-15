#pragma once
#include <Arduino.h>
#include "RS485Modbus.h"
#include "SensorDriver.h"
#include "Configuration_System.h"

class JXBS_SoilComp7in1 : public SensorDriver {
public:
  double soil_moisture;
  double soil_temp;
  double soil_ec;
  double soil_ph;
  uint16_t soil_nitrogen;
  uint16_t soil_phosphorus;
  uint16_t soil_potassium;

  JXBS_SoilComp7in1(RS485Bus& bus,
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

  bool readMoistureTemperature(uint8_t driverRetries = SENSOR_DEFAULT_DRIVER_RETRIES,
                               uint16_t readTimeoutMs = SENSOR_DEFAULT_READ_TIMEOUT_MS,
                               uint16_t afterReqDelayMs = SENSOR_DEFAULT_AFTER_REQ_MS);

  bool readConductivity(uint8_t driverRetries = SENSOR_DEFAULT_DRIVER_RETRIES,
                        uint16_t readTimeoutMs = SENSOR_DEFAULT_READ_TIMEOUT_MS,
                        uint16_t afterReqDelayMs = SENSOR_DEFAULT_AFTER_REQ_MS);

  bool readPH(uint8_t driverRetries = SENSOR_DEFAULT_DRIVER_RETRIES,
              uint16_t readTimeoutMs = SENSOR_DEFAULT_READ_TIMEOUT_MS,
              uint16_t afterReqDelayMs = SENSOR_DEFAULT_AFTER_REQ_MS);

  bool readNPK(uint8_t driverRetries = SENSOR_DEFAULT_DRIVER_RETRIES,
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
  bool _lastParsedFrame;

  bool validateMoistureTemperature() const;
  bool validateConductivity() const;
  bool validatePH() const;
  bool validateNPK() const;
};