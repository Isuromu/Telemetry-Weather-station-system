#pragma once
#include <Arduino.h>
#include "RS485Modbus.h"
#include "SensorDriver.h"
#include "Configuration_System.h"

/*
  JXBS_SoilComp7in1

  Driver for JXBS-3001-TR / JXBS-style soil comprehensive 7-in-1 sensor.

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
    1) Soil moisture
       - Register: 0x0012
       - Units: %
       - Raw scale: /10
       - Measurement range: 0..100 %
       - Resolution: 0.10 %
       - Accuracy: ±3% in 0..53 %, ±5% in 53..100 %

    2) Soil temperature
       - Register: 0x0013
       - Units: °C
       - Raw scale: signed int16 /10
       - Measurement range: -40..80 °C
       - Resolution: 0.1 °C
       - Accuracy: ±0.5 °C

    3) Soil conductivity
       - Register: 0x0015
       - Units: us/cm
       - Raw scale: 1:1
       - Measurement range: 0..10000 us/cm
       - Resolution: 10 us/cm

    4) Soil pH
       - Register: 0x0006
       - Units: pH
       - Raw scale: /100
       - Measurement range: 3..9 pH
       - Resolution: 0.01 pH
       - Accuracy: ±0.3 pH

    5) Soil nitrogen
       - Register: 0x001E
       - Units: mg/kg
       - Raw scale: 1:1
       - Measurement range: 0..1999 mg/kg
       - Resolution: 1 mg/kg
       - Accuracy: ±2% F.S

    6) Soil phosphorus
       - Register: 0x001F
       - Units: mg/kg
       - Raw scale: 1:1
       - Measurement range: 0..1999 mg/kg
       - Resolution: 1 mg/kg
       - Accuracy: ±2% F.S

    7) Soil potassium
       - Register: 0x0020
       - Units: mg/kg
       - Raw scale: 1:1
       - Measurement range: 0..1999 mg/kg
       - Resolution: 1 mg/kg
       - Accuracy: ±2% F.S

  Driver structure:
    - readMoistureTemperature()
    - readConductivity()
    - readPH()
    - readNPK()
    - readData() simply calls these four functions in sequence

  Supported write operation:
    - changeAddress() via register 0x0100

  Not implemented on purpose:
    - baud rate changing
    Reason:
      to avoid mixing unverified write encoding into a working driver.
*/

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

  bool validateMoistureTemperature() const;
  bool validateConductivity() const;
  bool validatePH() const;
  bool validateNPK() const;
};