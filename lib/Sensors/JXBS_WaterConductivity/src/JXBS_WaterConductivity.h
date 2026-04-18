#pragma once
#include <Arduino.h>
#include "RS485Modbus.h"
#include "SensorDriver.h"
#include "Configuration_System.h"

/*
  JXBS_WaterConductivity

  Driver intent:
  - RS485 Modbus RTU driver for a JXCT/JXEC-T water conductivity controller
    used with a JXEC-T conductivity probe.
  - Important hardware note: the bare metal JXEC-T probe is not necessarily a
    Modbus slave. It must be connected to the matching transmitter/controller
    assembly that exposes RS485/Modbus.

  Primary measurements:
      water_temperature_C    -> degrees Celsius
      conductivity_raw       -> raw 32-bit EC value from registers 0x0002/0x0003
      conductivity_uS_cm     -> engineering value after scale division

  Default communication:
  - Protocol: Modbus RTU
  - Function: 0x03 (Read Holding Registers)
  - Default address: 0x01
  - Default serial: 9600 bps, 8N1

  Register map:
  - 0x0001 = water temperature, raw / 10 C
  - 0x0002 = conductivity high word
  - 0x0003 = conductivity low word
  - 0x0100 = device address, read/write
  - 0x0101 = baud rate, read/write (not implemented here)

  Conductivity scaling:
  - The raw 32-bit value is common, but the physical interpretation depends on
    probe/controller range and cell constant.
  - The K=1 example uses raw / 100.0 to produce uS/cm.
*/
class JXBS_WaterConductivity : public SensorDriver {
public:
  double water_temperature_C;
  uint32_t conductivity_raw;
  double conductivity_uS_cm;

  JXBS_WaterConductivity(RS485Bus& bus,
                         const char* sensorId,
                         uint8_t address,
                         bool debugEnable = false,
                         double conductivityScaleDivisor = 100.0,
                         double maxConductivity_uS_cm = 200000.0,
                         uint8_t powerLineIndex = 0,
                         uint8_t interfaceIndex = 0,
                         uint16_t sampleRateMin = 1,
                         uint32_t warmUpTimeMs = 500,
                         uint8_t maxConsecutiveErrors = 10,
                         uint32_t minUsefulPowerOffMs = 60000UL);

  bool readData() override;
  void setFallbackValues() override;

  bool readTemperatureConductivity(uint8_t driverRetries = SENSOR_DEFAULT_DRIVER_RETRIES,
                                   uint16_t readTimeoutMs = SENSOR_DEFAULT_READ_TIMEOUT_MS,
                                   uint16_t afterReqDelayMs = SENSOR_DEFAULT_AFTER_REQ_MS);

  bool readTemperature(uint8_t driverRetries = SENSOR_DEFAULT_DRIVER_RETRIES,
                       uint16_t readTimeoutMs = SENSOR_DEFAULT_READ_TIMEOUT_MS,
                       uint16_t afterReqDelayMs = SENSOR_DEFAULT_AFTER_REQ_MS);

  bool readConductivity(uint8_t driverRetries = SENSOR_DEFAULT_DRIVER_RETRIES,
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

  void setConductivityScaleDivisor(double divisor);
  double getConductivityScaleDivisor() const { return _conductivityScaleDivisor; }

  void setMaxConductivity(double maxConductivity_uS_cm);
  double getMaxConductivity() const { return _maxConductivity_uS_cm; }

private:
  RS485Bus& _bus;
  double _conductivityScaleDivisor;
  double _maxConductivity_uS_cm;
  bool _lastParsedFrame;

  bool validateTemperature() const;
  bool validateConductivity() const;
  bool validateTemperatureConductivity() const;
  double scaledConductivity(uint32_t raw) const;

  static const uint8_t READ_ONE_REQUEST_SIZE = 8;
  static const uint8_t READ_ONE_RESPONSE_SIZE = 7;
  static const uint8_t READ_EC_REQUEST_SIZE = 8;
  static const uint8_t READ_EC_RESPONSE_SIZE = 9;
  static const uint8_t READ_ALL_REQUEST_SIZE = 8;
  static const uint8_t READ_ALL_RESPONSE_SIZE = 11;
  static const uint8_t READ_CHECK_SIZE = 3;
};
