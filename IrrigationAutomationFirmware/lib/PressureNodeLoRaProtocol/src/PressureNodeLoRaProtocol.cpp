#include "PressureNodeLoRaProtocol.h"

#include <limits.h>
#include <math.h>

namespace irrigation::pressure_node::lorawan_protocol {
namespace {

uint16_t readUint16Be(const uint8_t *data) {
  return (static_cast<uint16_t>(data[0]) << 8U) | data[1];
}

uint32_t readUint32Be(const uint8_t *data) {
  return (static_cast<uint32_t>(data[0]) << 24U) |
         (static_cast<uint32_t>(data[1]) << 16U) |
         (static_cast<uint32_t>(data[2]) << 8U) | data[3];
}

void writeUint16Be(uint8_t *data, uint16_t value) {
  data[0] = static_cast<uint8_t>(value >> 8U);
  data[1] = static_cast<uint8_t>(value);
}

void writeUint32Be(uint8_t *data, uint32_t value) {
  data[0] = static_cast<uint8_t>(value >> 24U);
  data[1] = static_cast<uint8_t>(value >> 16U);
  data[2] = static_cast<uint8_t>(value >> 8U);
  data[3] = static_cast<uint8_t>(value);
}

int16_t scaleInt16(float value, float multiplier) {
  if (!isfinite(value)) return INT16_MIN;
  const double scaled = static_cast<double>(value) * multiplier;
  if (scaled <= static_cast<double>(INT16_MIN)) return INT16_MIN + 1;
  if (scaled >= static_cast<double>(INT16_MAX)) return INT16_MAX;
  return static_cast<int16_t>(lround(scaled));
}

int32_t scaleInt32(float value, float multiplier) {
  if (!isfinite(value)) return INT32_MIN;
  const double scaled = static_cast<double>(value) * multiplier;
  if (scaled <= static_cast<double>(INT32_MIN)) return INT32_MIN + 1;
  if (scaled >= static_cast<double>(INT32_MAX)) return INT32_MAX;
  return static_cast<int32_t>(llround(scaled));
}

uint16_t batteryMillivolts(const BatteryReading &reading) {
  if (!reading.hasVoltage() || !isfinite(reading.voltageV) ||
      reading.voltageV < 0.0F)
    return UINT16_MAX;
  const double millivolts = static_cast<double>(reading.voltageV) * 1000.0;
  if (millivolts >= 65534.0) return 65534;
  return static_cast<uint16_t>(lround(millivolts));
}

uint32_t flowTotalLiters(const FlowTotalReading &reading) {
  if (!reading.hasSinceResetVolume() ||
      !isfinite(reading.sinceResetCubicMeters) ||
      reading.sinceResetCubicMeters < 0.0F)
    return UINT32_MAX;
  const double liters =
      static_cast<double>(reading.sinceResetCubicMeters) * 1000.0;
  if (liters >= static_cast<double>(UINT32_MAX - 1U))
    return UINT32_MAX - 1U;
  return static_cast<uint32_t>(llround(liters));
}

void writeInt16Be(uint8_t *data, int16_t value) {
  writeUint16Be(data, static_cast<uint16_t>(value));
}

void writeInt32Be(uint8_t *data, int32_t value) {
  writeUint32Be(data, static_cast<uint32_t>(value));
}

}  // namespace

CommandDecodeStatus decodeDownlink(const uint8_t *payload, size_t length,
                                   uint8_t fPort, uint8_t expectedFPort,
                                   uint32_t minimumReportIntervalSeconds,
                                   uint32_t maximumReportIntervalSeconds,
                                   DownlinkCommand &command) {
  command = {};
  if (fPort != expectedFPort) return CommandDecodeStatus::WrongPort;
  if (payload == nullptr || length != COMMAND_PAYLOAD_SIZE)
    return CommandDecodeStatus::WrongLength;
  if (payload[0] != PROTOCOL_VERSION)
    return CommandDecodeStatus::UnsupportedVersion;

  const uint8_t flags = payload[1];
  if ((flags & ~COMMAND_ALLOWED_FLAGS) != 0)
    return CommandDecodeStatus::InvalidFlags;
  if ((flags & COMMAND_ALLOWED_FLAGS) == 0)
    return CommandDecodeStatus::EmptyCommand;
  if (payload[3] != 0)
    return CommandDecodeStatus::InvalidReservedByte;

  command.hasValveAction = (flags & COMMAND_HAS_VALVE_ACTION) != 0;
  command.valveAction = static_cast<ValveAction>(payload[2]);
  if (command.hasValveAction) {
    if (command.valveAction != ValveAction::None &&
        command.valveAction != ValveAction::Open &&
        command.valveAction != ValveAction::Close)
      return CommandDecodeStatus::InvalidValveAction;
  } else if (command.valveAction != ValveAction::None) {
    return CommandDecodeStatus::InvalidValveAction;
  }

  command.hasReportInterval =
      (flags & COMMAND_HAS_REPORT_INTERVAL) != 0;
  command.reportIntervalSeconds = readUint32Be(&payload[4]);
  if (command.hasReportInterval) {
    if (minimumReportIntervalSeconds > maximumReportIntervalSeconds ||
        command.reportIntervalSeconds < minimumReportIntervalSeconds ||
        command.reportIntervalSeconds > maximumReportIntervalSeconds)
      return CommandDecodeStatus::InvalidReportInterval;
  } else if (command.reportIntervalSeconds != 0) {
    return CommandDecodeStatus::InvalidReportInterval;
  }

  command.hasFlowTotalReset =
      (flags & COMMAND_HAS_FLOW_TOTAL_RESET) != 0;

  command.commandId = readUint16Be(&payload[8]);
  return CommandDecodeStatus::Ok;
}

void buildStatusPayload(const PressureControlNodeStatus &status,
                        StatusReason reason, uint32_t reportIntervalSeconds,
                        uint16_t lastCommandId,
                        uint8_t payload[STATUS_PAYLOAD_SIZE]) {
  if (payload == nullptr) return;

  uint8_t flags = 0;
  if (status.upstreamPressure.hasSample()) flags |= 0x01;
  if (status.downstreamPressure.hasSample()) flags |= 0x02;
  if (status.battery.hasVoltage()) flags |= 0x04;
  if (status.flow.hasSample()) flags |= 0x08;
  if (status.upstreamPressure.engineeringUnitsValidated()) flags |= 0x10;
  if (status.downstreamPressure.engineeringUnitsValidated()) flags |= 0x20;
  if (status.valve.lastCommanded != PressureControlValveState::Unknown)
    flags |= 0x40;
  if (status.flow.diagnosticsAvailable) flags |= 0x80;

  payload[0] = PROTOCOL_VERSION;
  payload[1] = flags;
  payload[2] = static_cast<uint8_t>(status.valve.lastCommanded);
  payload[3] = static_cast<uint8_t>(reason);

  const bool upstreamAvailable = status.upstreamPressure.hasSample();
  const bool downstreamAvailable = status.downstreamPressure.hasSample();
  writeInt16Be(&payload[4], upstreamAvailable
                                ? scaleInt16(
                                      status.upstreamPressure.pressureBar,
                                      100.0F)
                                : INT16_MIN);
  writeInt16Be(&payload[6], upstreamAvailable
                                ? scaleInt16(
                                      status.upstreamPressure.temperatureC,
                                      100.0F)
                                : INT16_MIN);
  writeInt16Be(&payload[8], downstreamAvailable
                                ? scaleInt16(
                                      status.downstreamPressure.pressureBar,
                                      100.0F)
                                : INT16_MIN);
  writeInt16Be(&payload[10], downstreamAvailable
                                 ? scaleInt16(
                                       status.downstreamPressure.temperatureC,
                                       100.0F)
                                 : INT16_MIN);
  writeUint16Be(&payload[12], batteryMillivolts(status.battery));
  writeInt32Be(&payload[14],
               status.flow.hasSample()
                   ? scaleInt32(status.flow.value, 1000.0F)
                   : INT32_MIN);
  writeInt16Be(&payload[18],
               status.flow.hasVelocity()
                   ? scaleInt16(status.flow.velocityMetersPerSecond, 1000.0F)
                   : INT16_MIN);
  writeUint16Be(&payload[20], status.flow.diagnosticsAvailable
                                  ? status.flow.deviceErrorBits
                                  : UINT16_MAX);
  writeUint32Be(&payload[22], reportIntervalSeconds);
  writeUint16Be(&payload[26], lastCommandId);
  writeUint32Be(&payload[28], flowTotalLiters(status.flowTotal));
}

}  // namespace irrigation::pressure_node::lorawan_protocol
