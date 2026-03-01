#include "RikaLeafSensor.h"

RikaLeafSensor::RikaLeafSensor(RS485Bus &bus, uint8_t address)
    : _bus(bus), _addr(address) {}

void RikaLeafSensor::setAddress(uint8_t address) { _addr = address; }

uint8_t RikaLeafSensor::address() const { return _addr; }

bool RikaLeafSensor::readAll(RikaLeafSensor_Data &out) {
  // 1. Build request: [addr, 0x03, 0x00, 0x00, 0x00, 0x02, CRC_L, CRC_H]
  uint8_t tx[8] = {_addr, 0x03, 0x00, 0x00, 0x00, 0x02, 0, 0};
  uint16_t crcReq = RS485Bus::crc16Modbus(tx, 6);
  tx[6] = crcReq & 0xFF;
  tx[7] = (crcReq >> 8) & 0xFF;

  // 2. Define expected validation bounds
  uint8_t prefix[3] = {_addr, 0x03, 0x04}; // Expect 4 bytes of data
  uint8_t rxFrame[9]; // 3 (prefix) + 4 (data) + 2 (CRC) = 9

  // 3. Dispatch and extract safely
  bool ok = _bus.transferAndExtractFixedFrame(tx, 8, prefix, 3, 9, rxFrame,
                                              sizeof(rxFrame), 250, 25);

  if (!ok)
    return false;

  // 4. Parse payload
  // Typically for Rika leaf sensors, Reg 0 (bytes 3 & 4) is Temperature, and
  // Reg 1 (bytes 5 & 6) is Humidity Cast to signed 16-bit for temperature to
  // handle negative values properly
  int16_t rawTemp = (int16_t)(((uint16_t)rxFrame[3] << 8) | rxFrame[4]);
  double temp = (double)rawTemp / 10.0;

  // Humidity is generally unsigned 0-100%
  uint16_t rawHum = ((uint16_t)rxFrame[5] << 8) | rxFrame[6];
  double hum = (double)rawHum / 10.0;

  // Optional: logical bounds checks based on typical Rika specifications
  if (temp < -40.0 || temp > 80.0)
    return false;
  if (hum < 0.0 || hum > 100.0)
    return false;

  out.rika_leaf_temp = temp;
  out.rika_leaf_humid = hum;

  return true;
}

bool RikaLeafSensor::changeAddress(uint8_t newAddress) {
  // Broadcast request: [0xFF, 0x06, 0x02, 0x00, 0x00, newAddress, CRC_L, CRC_H]
  // REQUIRES white wire to be connected to positive supply.
  uint8_t tx[8] = {0xFF, 0x06, 0x02, 0x00, 0x00, newAddress, 0, 0};
  uint16_t crcReq = RS485Bus::crc16Modbus(tx, 6);
  tx[6] = crcReq & 0xFF;
  tx[7] = (crcReq >> 8) & 0xFF;

  // Expected response validation based on user code
  uint8_t prefix[5] = {0xFF, 0x06, 0x02, 0x00, 0x00};
  uint8_t rxFrame[8];

  // Dispatch Command
  bool ok = _bus.transferAndExtractFixedFrame(
      tx, 8, prefix, 5, 8, rxFrame, sizeof(rxFrame), 500,
      50 // Give it a bit more time for EEPROM address saving
  );

  if (!ok)
    return false;

  // Update internal address tracking if successful
  _addr = newAddress;
  return true;
}
