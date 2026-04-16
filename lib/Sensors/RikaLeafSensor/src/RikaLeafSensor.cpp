#include "RikaLeafSensor.h"
#include "Configuration_System.h"

/*
  Frame layout used by this driver (readData and scanForAddress):

  Hardware mode:
    - Normal reading: connect white wire to V- / GND.
    - Address change: connect white wire to V+ before calling changeAddress().

  Request:
    [addr][0x03][0x00][0x00][0x00][0x02][crc_lo][crc_hi]
     addr = current sensor Modbus address

  Response:
    [addr][0x03][0x04][hum_hi][hum_lo][tmp_hi][tmp_lo][crc_lo][crc_hi]
*/
RikaLeafSensor::RikaLeafSensor(RS485Bus& bus,
                               const char* sensorId,
                               uint8_t address,
                               bool debugEnable,
                               uint8_t powerLineIndex,
                               uint8_t interfaceIndex,
                               uint16_t sampleRateMin,
                               uint32_t warmUpTimeMs,
                               uint8_t maxConsecutiveErrors,
                               uint32_t minUsefulPowerOffMs)
    : SensorDriver(sensorId,
                   address,
                   debugEnable,
                   powerLineIndex,
                   interfaceIndex,
                   sampleRateMin,
                   warmUpTimeMs,
                   maxConsecutiveErrors,
                   minUsefulPowerOffMs),
      _bus(bus),
      leaf_temp(0.0),
      leaf_humid(0.0) {}

void RikaLeafSensor::setFallbackValues() {
  // Explicit sentinel values used by application logic to detect stale/invalid reads.
  leaf_temp = -99.0;
  leaf_humid = -99.0;
}

bool RikaLeafSensor::readData() {
  // SensorDriver timestamp is updated at the start of each logical read cycle.
  markReadTime(millis());

  // Driver-level retries sit above RS485Bus retries.
  // Each attempt re-sends the same request and re-validates the decoded values.
  const uint8_t DRIVER_RETRIES = SENSOR_DEFAULT_DRIVER_RETRIES;

  for (uint8_t driverAttempt = 1; driverAttempt <= DRIVER_RETRIES; ++driverAttempt) {
    // Read 2 holding registers starting at 0x0000:
    //   register 0 -> humidity * 10
    //   register 1 -> signed temperature * 10
    uint8_t request[READ_REQUEST_SIZE] = {
        _address, 0x03, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00
    };

    uint8_t response[READ_RESPONSE_SIZE] = {0};
    // Prefix check performed by RS485Bus before CRC validation.
    const uint8_t check[READ_CHECK_SIZE] = {_address, 0x03, 0x04};

    const bool ok = _bus.SendRequest(request,
                                     READ_REQUEST_SIZE,
                                     response,
                                     READ_RESPONSE_SIZE,
                                     check,
                                     READ_CHECK_SIZE,
                                     SENSOR_DEFAULT_BUS_RETRIES,
                                     SENSOR_DEFAULT_READ_TIMEOUT_MS,
                                     _debugEnable,
                                     SENSOR_DEFAULT_AFTER_REQ_MS);

    if (!ok) {
      continue;
    }

    // Big-endian register extraction from Modbus payload.
    const uint16_t rawHum = ((uint16_t)response[3] << 8) | response[4];
    const int16_t rawTemp = (int16_t)(((uint16_t)response[5] << 8) | response[6]);

    // Sensor-provided fixed-point conversion.
    leaf_humid = (double)rawHum / 10.0;
    leaf_temp  = (double)rawTemp / 10.0;

    // Defensive data sanity gates. If they fail, we retry the full transaction.
    if (leaf_temp < -40.0 || leaf_temp > 80.0) {
      continue;
    }

    if (leaf_humid < 0.0 || leaf_humid > 100.0) {
      continue;
    }

    markSuccess();
    return true;
  }

  // Full read cycle failed across all retries.
  setFallbackValues();
  markFailure();
  return false;
}

bool RikaLeafSensor::changeAddress(uint8_t newAddress,
                                   uint8_t maxRetries,
                                   uint16_t readTimeoutMs,
                                   uint16_t afterReqDelayMs) {
  if (newAddress == 0 || newAddress > 247) {
    return false;
  }

  // Common Rika/JXCT convention:
  // - use address 0xFF for address-write command
  // - write new node address into register 0x0200
  // Hardware condition for this Rika leaf sensor:
  // - connect white wire to V+ before calling this function
  // - restore white wire to V- / GND before normal readData()/scanForAddress()
  // Use this only when the target sensor is the only device that can respond
  // to this address-write command on the RS485 bus.
  uint8_t request[8] = {0xFF, 0x06, 0x02, 0x00, 0x00, newAddress, 0x00, 0x00};
  uint8_t response[8] = {0};
  const uint8_t check[5] = {0xFF, 0x06, 0x02, 0x00, 0x00};

  const bool ok = _bus.SendRequest(request,
                                   8,
                                   response,
                                   8,
                                   check,
                                   5,
                                   maxRetries,
                                   readTimeoutMs,
                                   _debugEnable,
                                   afterReqDelayMs);

  if (ok) {
    _address = newAddress;
  }

  return ok;
}

uint8_t RikaLeafSensor::scanForAddress(uint8_t startAddr,
                                      uint8_t endAddr,
                                      uint16_t readTimeoutMs,
                                      uint16_t afterReqDelayMs) {
  // Clamp to legal Modbus node range.
  if (startAddr == 0) startAddr = 1;
  if (endAddr > 247) endAddr = 247;
  if (startAddr > endAddr) return 0;

  for (uint16_t addr = startAddr; addr <= endAddr; ++addr) {
    // Probe candidate address using normal measurement read command.
    uint8_t request[READ_REQUEST_SIZE] = {
        (uint8_t)addr, 0x03, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00
    };
    uint8_t response[READ_RESPONSE_SIZE] = {0};
    const uint8_t check[READ_CHECK_SIZE] = {(uint8_t)addr, 0x03, 0x04};

    const bool found = _bus.SendRequest(request,
                                        READ_REQUEST_SIZE,
                                        response,
                                        READ_RESPONSE_SIZE,
                                        check,
                                        READ_CHECK_SIZE,
                                        1,
                                        readTimeoutMs,
                                        _debugEnable,
                                        afterReqDelayMs);
    if (found) {
      // Keep discovered address in sync with runtime driver state.
      _address = (uint8_t)addr;
      return (uint8_t)addr;
    }

    // Small gap keeps scan traffic reasonable on noisy buses.
    delay(5);
  }

  return 0;
}
