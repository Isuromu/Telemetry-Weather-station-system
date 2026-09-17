#include "SoilNodeLoRaProtocol.h"

#include <Arduino.h>

namespace irrigation::soil_node::lorawan_protocol {
namespace {

void putU16(uint8_t *payload, size_t offset, uint16_t value) {
  payload[offset] = highByte(value);
  payload[offset + 1] = lowByte(value);
}

void putU32(uint8_t *payload, size_t offset, uint32_t value) {
  payload[offset] = static_cast<uint8_t>(value >> 24);
  payload[offset + 1] = static_cast<uint8_t>(value >> 16);
  payload[offset + 2] = static_cast<uint8_t>(value >> 8);
  payload[offset + 3] = static_cast<uint8_t>(value);
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
    uint32_t maximumSeconds, uint32_t &sleepSeconds, uint16_t &commandId) {
  if (payload == nullptr ||
      (length != LEGACY_SET_SLEEP_INTERVAL_COMMAND_SIZE &&
       length != SET_SLEEP_INTERVAL_COMMAND_SIZE) ||
      payload[0] != SET_SLEEP_INTERVAL_COMMAND) {
    return false;
  }

  const bool hasCommandId = length == SET_SLEEP_INTERVAL_COMMAND_SIZE;
  if (hasCommandId) {
    commandId = (static_cast<uint16_t>(payload[1]) << 8) | payload[2];
    if (commandId == 0 || commandId == LEGACY_COMMAND_ID) return false;
  } else {
    commandId = LEGACY_COMMAND_ID;
  }
  const size_t offset = hasCommandId ? 3 : 1;
  const uint32_t requestedSeconds =
      (static_cast<uint32_t>(payload[offset]) << 24) |
      (static_cast<uint32_t>(payload[offset + 1]) << 16) |
      (static_cast<uint32_t>(payload[offset + 2]) << 8) |
      static_cast<uint32_t>(payload[offset + 3]);
  if (requestedSeconds < minimumSeconds ||
      requestedSeconds > maximumSeconds) {
    return false;
  }

  sleepSeconds = requestedSeconds;
  return true;
}

void encodeCommandAck(const CommandAck &ack,
                      uint8_t payload[COMMAND_ACK_SIZE]) {
  payload[0] = COMMAND_ACK_VERSION;
  putU16(payload, 1, ack.commandId);
  payload[3] = static_cast<uint8_t>(ack.status);
  putU32(payload, 4, ack.activeSleepSeconds);
}

}  // namespace irrigation::soil_node::lorawan_protocol
