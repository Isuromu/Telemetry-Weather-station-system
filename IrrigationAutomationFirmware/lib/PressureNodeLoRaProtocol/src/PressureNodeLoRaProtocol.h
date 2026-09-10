#pragma once

#include <PressureNodeTypes.h>

#include <stddef.h>
#include <stdint.h>

namespace irrigation::pressure_node::lorawan_protocol {

inline constexpr uint8_t PROTOCOL_VERSION = 2;
inline constexpr size_t COMMAND_PAYLOAD_SIZE = 10;
inline constexpr size_t STATUS_PAYLOAD_SIZE = 32;

inline constexpr uint8_t COMMAND_HAS_VALVE_ACTION = 0x01;
inline constexpr uint8_t COMMAND_HAS_REPORT_INTERVAL = 0x02;
inline constexpr uint8_t COMMAND_HAS_FLOW_TOTAL_RESET = 0x04;
inline constexpr uint8_t COMMAND_ALLOWED_FLAGS =
    COMMAND_HAS_VALVE_ACTION | COMMAND_HAS_REPORT_INTERVAL |
    COMMAND_HAS_FLOW_TOTAL_RESET;

enum class ValveAction : uint8_t {
  None = 0,
  Open = 1,
  Close = 2,
};

enum class CommandDecodeStatus : uint8_t {
  Ok,
  WrongPort,
  WrongLength,
  UnsupportedVersion,
  InvalidFlags,
  EmptyCommand,
  InvalidValveAction,
  InvalidReservedByte,
  InvalidReportInterval,
};

constexpr const char *commandDecodeStatusName(CommandDecodeStatus status) {
  switch (status) {
    case CommandDecodeStatus::Ok:
      return "OK";
    case CommandDecodeStatus::WrongPort:
      return "WRONG_PORT";
    case CommandDecodeStatus::WrongLength:
      return "WRONG_LENGTH";
    case CommandDecodeStatus::UnsupportedVersion:
      return "UNSUPPORTED_VERSION";
    case CommandDecodeStatus::InvalidFlags:
      return "INVALID_FLAGS";
    case CommandDecodeStatus::EmptyCommand:
      return "EMPTY_COMMAND";
    case CommandDecodeStatus::InvalidValveAction:
      return "INVALID_VALVE_ACTION";
    case CommandDecodeStatus::InvalidReservedByte:
      return "INVALID_RESERVED_BYTE";
    case CommandDecodeStatus::InvalidReportInterval:
      return "INVALID_REPORT_INTERVAL";
  }
  return "UNKNOWN";
}

struct DownlinkCommand {
  bool hasValveAction{false};
  ValveAction valveAction{ValveAction::None};
  bool hasReportInterval{false};
  uint32_t reportIntervalSeconds{0};
  bool hasFlowTotalReset{false};
  uint16_t commandId{0};
};

enum class StatusReason : uint8_t {
  Startup = 0,
  PeriodicReport = 1,
  RemoteOpenApplied = 2,
  RemoteCloseApplied = 3,
  ReportIntervalApplied = 4,
  ValveAndReportIntervalApplied = 5,
  DuplicateCommandIgnored = 6,
  InvalidCommandRejected = 7,
  ValveActuationFailed = 8,
  LoRaWanError = 9,
  LocalCommandApplied = 10,
  NoOpCommandAccepted = 11,
  FlowTotalResetApplied = 12,
  FlowTotalResetFailed = 13,
};

CommandDecodeStatus decodeDownlink(const uint8_t *payload, size_t length,
                                   uint8_t fPort, uint8_t expectedFPort,
                                   uint32_t minimumReportIntervalSeconds,
                                   uint32_t maximumReportIntervalSeconds,
                                   DownlinkCommand &command);

void buildStatusPayload(const PressureControlNodeStatus &status,
                        StatusReason reason, uint32_t reportIntervalSeconds,
                        uint16_t lastCommandId,
                        uint8_t payload[STATUS_PAYLOAD_SIZE]);

}  // namespace irrigation::pressure_node::lorawan_protocol
