#pragma once
#include <Arduino.h>
#include "RS485Modbus.h"
#include "SensorDriver.h"
#include "Configuration_System.h"

/*
  HondeSolarRadiation

  Driver intent:
  - RS485 Modbus RTU driver for the Honde RD-TSRS2L global solar radiation
    sensor / pyranometer.
  - Primary measurement:
      solar_radiation -> W/m2

  Manual:
  - "Solar radiation sensor manual 2.0"
  - Product model: RD-TSRS2L

  RS485 wiring from the manual:
  - Red    -> power +
  - Black  -> GND
  - Yellow -> RS485-A
  - Green  -> RS485-B

  Installation notes from the manual:
  - Install with a clear sky view and avoid shadows, especially near sunrise
    and sunset.
  - For horizontal installation, level the sensor using the bubble level and
    adjustment feet.
  - The terminal/cable side should face north.
  - Keep the glass dome clean; use a soft cloth with water or alcohol.
  - Recommended recalibration cycle: every two years.

  Main read command (readData):
  - Function: 0x03 (Read Holding Registers)
  - Start register: 0x0000
  - Count: 1 register
  - Payload map:
      reg 0x0000 = solar radiation, unsigned, W/m2

  Configuration command:
  - Function: 0x10
  - Register 0x0501 = device address according to the manual.
  - Register 0x0503 = baud rate according to the manual, not implemented here.
*/
class HondeSolarRadiation : public SensorDriver {
public:
  double solar_radiation;

  HondeSolarRadiation(RS485Bus& bus,
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

  bool readSolarRadiation(uint8_t driverRetries = SENSOR_DEFAULT_DRIVER_RETRIES,
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

  bool validateSolarRadiation() const;

  static const uint8_t READ_REQUEST_SIZE = 8;
  static const uint8_t READ_RESPONSE_SIZE = 7;
  static const uint8_t READ_CHECK_SIZE = 3;
  static const uint8_t ADDRESS_WRITE_REQUEST_SIZE = 11;
  static const uint8_t ADDRESS_WRITE_RESPONSE_SIZE = 8;
};
