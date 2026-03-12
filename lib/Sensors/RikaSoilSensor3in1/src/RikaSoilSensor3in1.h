#pragma once
#include <Arduino.h>
#include "RS485Modbus.h"
#include "SensorDriver.h"
#include "Configuration_System.h"

/*
  RikaSoilSensor3in1

  Driver intent:
  - RS485 Modbus RTU driver for the RK520-02 / JXBS-3001-TR family.
  - Primary measurements:
      soil_temp -> degrees Celsius
      soil_vwc  -> volumetric water content (%)
      soil_ec   -> electrical conductivity (mS/cm)
  - Additional maintenance/config values:
      epsilon, currentSoilType, compensation coefficients

  Main read command (readData):
  - Function: 0x03 (Read Holding Registers)
  - Start register: 0x0000
  - Count: 3 registers
  - Payload map:
      reg0 = signed temperature * 10
      reg1 = VWC * 10
      reg2 = EC * 1000

  Reliability model:
  - Uses SensorDriver status transitions via markSuccess/markFailure.
  - setFallbackValues() sets sentinel values (-99.0 / SOIL_UNKNOWN) on failure.
*/
class RikaSoilSensor3in1 : public SensorDriver {
public:
  // Soil classification values accepted by register 0x0020.
  enum SoilType : uint8_t {
    SOIL_MINERAL = 0,
    SOIL_SANDY   = 1,
    SOIL_CLAY    = 2,
    SOIL_ORGANIC = 3,
    SOIL_UNKNOWN = 255
  };

  // Last decoded values (sentinel -99.0 on failed read cycle).
  double soil_temp;
  double soil_vwc;
  double soil_ec;

  // Last decoded dielectric constant from readEpsilon().
  double epsilon;

  // Cached soil class from readSoilType()/setSoilType().
  SoilType currentSoilType;

  // Construct a driver bound to one RS485 bus and one node address.
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

  // Reads temperature, VWC, and EC in one transaction.
  bool readData() override;

  // Writes fallback sentinels for all driver-exposed values.
  void setFallbackValues() override;

  // Writes new Modbus node address to register 0x0200.
  bool changeAddress(uint8_t newAddress,
                     uint8_t maxRetries = 3,
                     uint16_t readTimeoutMs = 500,
                     uint16_t afterReqDelayMs = 20);

  // Scans an address range and returns first responsive node, or 0 if none.
  uint8_t scanForAddress(uint8_t startAddr = 1,
                         uint8_t endAddr = 247,
                         uint16_t readTimeoutMs = 120,
                         uint16_t afterReqDelayMs = 20);

  // Reads soil type from holding register 0x0020.
  // Output is normalized to SoilType enum and cached in currentSoilType.
  bool readSoilType(SoilType &type,
                    uint8_t driverRetries = 3,
                    uint16_t readTimeoutMs = 500,
                    uint16_t afterReqDelayMs = 20);

  // Writes soil type enum value to holding register 0x0020.
  bool setSoilType(SoilType type,
                   uint8_t driverRetries = 3,
                   uint16_t readTimeoutMs = 500,
                   uint16_t afterReqDelayMs = 20);

  // Reads dielectric constant (epsilon) from input register 0x0005 (scaled /100).
  bool readEpsilon(double &value,
                   uint8_t driverRetries = 3,
                   uint16_t readTimeoutMs = 500,
                   uint16_t afterReqDelayMs = 20);

  // Writes a calibration/compensation coefficient to an arbitrary register.
  // coeffValue should already be in sensor fixed-point register format.
  bool setCompensationCoeff(uint16_t regAddress,
                            uint16_t coeffValue,
                            uint8_t driverRetries = 3,
                            uint16_t readTimeoutMs = 500,
                            uint16_t afterReqDelayMs = 20);

  // Reads a calibration/compensation coefficient and converts it by /100.
  bool readCompensationCoeff(uint16_t regAddress,
                             double &coeffValue,
                             uint8_t driverRetries = 3,
                             uint16_t readTimeoutMs = 500,
                             uint16_t afterReqDelayMs = 20);

  // Utility formatter for logs/diagnostics.
  static const char* soilTypeToString(SoilType type);

private:
  RS485Bus& _bus;

  // Frame sizes used by main measurement transaction.
  static const uint8_t MAIN_REQUEST_SIZE   = 8;
  static const uint8_t MAIN_RESPONSE_SIZE  = 11;
  static const uint8_t MAIN_CHECK_SIZE     = 3;
};
