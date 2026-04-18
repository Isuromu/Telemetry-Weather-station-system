#pragma once
#include <Arduino.h>
#include "RS485Modbus.h"
#include "SensorDriver.h"
#include "Configuration_System.h"

/*
  JXSZ_WaterSuspendedSolids

  Driver intent:
  - RS485 Modbus RTU driver for the JXSZ-1001-SS water suspended solids sensor.
  - Primary measurements:
      water_temperature_C       -> degrees Celsius
      suspended_solids_raw      -> raw 16-bit suspended solids register
      suspended_solids_mg_L     -> engineering value after scale division

  Manual / local protocol note:
  - Source PDF: docs/datasheets/JXSZ-1001-Water Suspended.pdf
  - Parsed protocol note:
    docs/protocols/JXSZ_1001_SS_Water_Suspended_Solids_Sensor_RS485.md

  Default communication:
  - Protocol: Modbus RTU
  - Function: 0x03 (Read Holding Registers)
  - Default address: 0x01
  - Default serial: 9600 bps, 8N1
  - Manual baud options: 2400 / 4800 / 9600

  Register map:
  - 0x0001 = water temperature, signed 16-bit raw / 10 C
  - 0x0002 = suspended solids, unsigned 16-bit raw / scale divisor mg/L
  - 0x0100 = device address, read/write with function 0x06
  - 0x0101 = baud rate, read/write (not implemented here)

  Suspended solids scaling:
  - The manual table text and worked example conflict.
  - The worked example maps raw 189 to 18.9, so the default divisor is 10.0.
  - If your real sensor label/manual variant expects 0.01 mg/L resolution, set
    the divisor to 100.0 in the example config.
*/
class JXSZ_WaterSuspendedSolids : public SensorDriver {
public:
  double water_temperature_C;
  uint16_t suspended_solids_raw;
  double suspended_solids_mg_L;

  JXSZ_WaterSuspendedSolids(RS485Bus& bus,
                            const char* sensorId,
                            uint8_t address,
                            bool debugEnable = false,
                            double suspendedSolidsScaleDivisor = 10.0,
                            double maxSuspendedSolids_mg_L = 20000.0,
                            uint8_t powerLineIndex = 0,
                            uint8_t interfaceIndex = 0,
                            uint16_t sampleRateMin = 1,
                            uint32_t warmUpTimeMs = 500,
                            uint8_t maxConsecutiveErrors = 10,
                            uint32_t minUsefulPowerOffMs = 60000UL);

  bool readData() override;
  void setFallbackValues() override;

  bool readTemperatureSuspendedSolids(uint8_t driverRetries = SENSOR_DEFAULT_DRIVER_RETRIES,
                                      uint16_t readTimeoutMs = SENSOR_DEFAULT_READ_TIMEOUT_MS,
                                      uint16_t afterReqDelayMs = SENSOR_DEFAULT_AFTER_REQ_MS);

  bool readTemperature(uint8_t driverRetries = SENSOR_DEFAULT_DRIVER_RETRIES,
                       uint16_t readTimeoutMs = SENSOR_DEFAULT_READ_TIMEOUT_MS,
                       uint16_t afterReqDelayMs = SENSOR_DEFAULT_AFTER_REQ_MS);

  bool readSuspendedSolids(uint8_t driverRetries = SENSOR_DEFAULT_DRIVER_RETRIES,
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

  void setSuspendedSolidsScaleDivisor(double divisor);
  double getSuspendedSolidsScaleDivisor() const { return _suspendedSolidsScaleDivisor; }

  void setMaxSuspendedSolids(double maxSuspendedSolids_mg_L);
  double getMaxSuspendedSolids() const { return _maxSuspendedSolids_mg_L; }

private:
  RS485Bus& _bus;
  double _suspendedSolidsScaleDivisor;
  double _maxSuspendedSolids_mg_L;
  bool _lastParsedFrame;

  bool validateTemperature() const;
  bool validateSuspendedSolids() const;
  bool validateTemperatureSuspendedSolids() const;
  double scaledSuspendedSolids(uint16_t raw) const;

  static const uint8_t READ_ONE_REQUEST_SIZE = 8;
  static const uint8_t READ_ONE_RESPONSE_SIZE = 7;
  static const uint8_t READ_TWO_REQUEST_SIZE = 8;
  static const uint8_t READ_TWO_RESPONSE_SIZE = 9;
  static const uint8_t READ_CHECK_SIZE = 3;
};
