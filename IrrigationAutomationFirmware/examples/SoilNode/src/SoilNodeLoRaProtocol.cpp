#include "SoilNodeLoRaProtocol.h"

#include <Arduino.h>

namespace irrigation::soil_node::lorawan_protocol {
namespace {

void putU16(uint8_t *payload, size_t offset, uint16_t value) {
  payload[offset] = highByte(value);
  payload[offset + 1] = lowByte(value);
}

}  // namespace

void encodeTelemetry(const Telemetry &telemetry,
                     uint8_t payload[TELEMETRY_SIZE]) {
  payload[0] = telemetry.sensorValid ? 0x01 : 0x00;
  putU16(payload, 1, static_cast<uint16_t>(telemetry.temperature100));
  putU16(payload, 3, telemetry.vwc100);
  putU16(payload, 5, telemetry.ec1000);
  payload[7] = telemetry.batteryEncoded;
}

bool decodeSleepIntervalCommand(
    const uint8_t *payload, size_t length, uint32_t minimumSeconds,
    uint32_t maximumSeconds, uint32_t &sleepSeconds) {
  if (payload == nullptr || length != SET_SLEEP_INTERVAL_COMMAND_SIZE ||
      payload[0] != SET_SLEEP_INTERVAL_COMMAND) {
    return false;
  }

  const uint32_t requestedSeconds =
      (static_cast<uint32_t>(payload[1]) << 24) |
      (static_cast<uint32_t>(payload[2]) << 16) |
      (static_cast<uint32_t>(payload[3]) << 8) |
      static_cast<uint32_t>(payload[4]);
  if (requestedSeconds < minimumSeconds ||
      requestedSeconds > maximumSeconds) {
    return false;
  }

  sleepSeconds = requestedSeconds;
  return true;
}

}  // namespace irrigation::soil_node::lorawan_protocol
