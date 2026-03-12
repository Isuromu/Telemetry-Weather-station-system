#pragma once
#include <Arduino.h>
#include "RS485Modbus.h"
#include "SensorDriver.h"

/*
  RikaLeafSensor

  Driver intent:
  - Reads a Rika/JXCT-style leaf surface sensor over RS485 Modbus RTU.
  - Exposes the latest decoded values through public fields:
      leaf_temp  -> degrees Celsius
      leaf_humid -> %RH (relative humidity)

  Read protocol used by readData():
  - Function: 0x03 (Read Holding Registers)
  - Start register: 0x0000
  - Register count: 2
  - Response payload (4 data bytes):
      [humidity_hi humidity_lo temp_hi temp_lo]
  - Scaling:
      humidity = raw / 10.0
      temp     = signed_raw / 10.0

  Addressing helpers:
  - changeAddress(): broadcast-like write to register 0x0200.
  - scanForAddress(): probes address range with the same read command as readData().

  Reliability model:
  - Uses SensorDriver state tracking (markSuccess/markFailure).
  - On a failed read cycle, setFallbackValues() writes sentinel -99.0.
*/
class RikaLeafSensor : public SensorDriver {
public:
  // Last decoded measurement values from the sensor.
  // Sentinel value on failure: -99.0.
  double leaf_temp;
  double leaf_humid;

  // Construct a driver bound to one RS485 bus and one sensor address.
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

  // Performs one logical sampling attempt (with internal driver retries).
  // Returns true only when frame, CRC, and range validation pass.
  bool readData() override;

  // Writes explicit fallback sentinels when a full read cycle fails.
  void setFallbackValues() override;

  // Writes a new Modbus node address to sensor register 0x0200.
  // On success, the local driver address is updated to newAddress.
  bool changeAddress(uint8_t newAddress,
                     uint8_t maxRetries = 3,
                     uint16_t readTimeoutMs = 500,
                     uint16_t afterReqDelayMs = 20);

  // Probes [startAddr, endAddr] and returns the first responsive address.
  // Returns 0 when no valid response is found.
  uint8_t scanForAddress(uint8_t startAddr = 1,
                         uint8_t endAddr = 247,
                         uint16_t readTimeoutMs = 120,
                         uint16_t afterReqDelayMs = 20);

private:
  RS485Bus& _bus;

  // Request and response frame sizes used by readData()/scanForAddress().
  static const uint8_t READ_REQUEST_SIZE = 8;
  static const uint8_t READ_RESPONSE_SIZE = 9;
  static const uint8_t READ_CHECK_SIZE = 3;
};
