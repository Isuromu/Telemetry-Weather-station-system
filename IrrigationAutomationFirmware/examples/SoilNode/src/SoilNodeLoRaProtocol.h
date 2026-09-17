#pragma once

#include <stddef.h>
#include <stdint.h>

namespace irrigation::soil_node::lorawan_protocol {

inline constexpr size_t TELEMETRY_SIZE = 8;
inline constexpr size_t LEGACY_SET_SLEEP_INTERVAL_COMMAND_SIZE = 5;
inline constexpr size_t SET_SLEEP_INTERVAL_COMMAND_SIZE = 7;
inline constexpr size_t COMMAND_ACK_SIZE = 8;
inline constexpr uint8_t SET_SLEEP_INTERVAL_COMMAND = 0x01;
inline constexpr uint8_t COMMAND_ACK_VERSION = 0x01;
inline constexpr uint16_t LEGACY_COMMAND_ID = 0xFFFF;

enum class CommandStatus : uint8_t {
  APPLIED = 0,
  INVALID = 1,
  STORAGE_FAILED = 2,
};

struct CommandAck {
  uint16_t commandId{LEGACY_COMMAND_ID};
  CommandStatus status{CommandStatus::INVALID};
  uint32_t activeSleepSeconds{0};
};

struct Telemetry {
  int16_t temperature100{0};
  uint16_t vwc100{0};
  uint16_t ec1000{0};
  uint8_t batteryEncoded{0};
  bool sensorValid{false};
};

void encodeTelemetry(const Telemetry &telemetry,
                     uint8_t payload[TELEMETRY_SIZE]);

bool decodeSleepIntervalCommand(
    const uint8_t *payload, size_t length, uint32_t minimumSeconds,
    uint32_t maximumSeconds, uint32_t &sleepSeconds, uint16_t &commandId);

void encodeCommandAck(const CommandAck &ack,
                      uint8_t payload[COMMAND_ACK_SIZE]);

}  // namespace irrigation::soil_node::lorawan_protocol
