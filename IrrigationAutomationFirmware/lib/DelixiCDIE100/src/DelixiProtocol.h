#pragma once

#include <stdint.h>

namespace delixi::protocol {

constexpr uint8_t READ_HOLDING_REGISTERS = 0x03;
constexpr uint8_t WRITE_SINGLE_REGISTER = 0x06;

constexpr uint16_t RUN_COMMAND = 0xA000;
constexpr uint16_t FREQUENCY_COMMAND = 0xA001;
constexpr uint16_t RUN_STATE = 0xB000;
constexpr uint16_t FAULT_CODE = 0xB001;

constexpr uint16_t MONITOR_OUTPUT_FREQUENCY = 0x9000;
constexpr uint16_t MONITOR_REFERENCE_FREQUENCY = 0x9001;
constexpr uint16_t MONITOR_OUTPUT_CURRENT = 0x9002;
constexpr uint16_t MONITOR_OUTPUT_VOLTAGE = 0x9003;
constexpr uint16_t MONITOR_COMMUNICATION_SET_VALUE = 0x901B;

constexpr uint16_t COMMAND_FORWARD_RUN = 0x0001;
constexpr uint16_t COMMAND_REVERSE_RUN = 0x0002;
constexpr uint16_t COMMAND_FORWARD_JOG = 0x0003;
constexpr uint16_t COMMAND_REVERSE_JOG = 0x0004;
constexpr uint16_t COMMAND_FREE_STOP = 0x0005;
constexpr uint16_t COMMAND_DECELERATION_STOP = 0x0006;
constexpr uint16_t COMMAND_FAULT_RESET = 0x0007;

constexpr uint16_t parameterAddress(uint8_t group, uint8_t level,
                                    uint8_t decimalIndex,
                                    bool volatileRam = false) {
  const uint8_t groupLevel =
      static_cast<uint8_t>((group << 4U) | (level & 0x0FU));
  const uint8_t high =
      volatileRam ? static_cast<uint8_t>(groupLevel + 0x04U) : groupLevel;
  return static_cast<uint16_t>((static_cast<uint16_t>(high) << 8U) |
                               decimalIndex);
}

constexpr uint16_t frequencyPercentToRaw(float percent) {
  return percent <= 0.0f
             ? 0
             : (percent >= 100.0f
                    ? 10000
                    : static_cast<uint16_t>(percent * 100.0f + 0.5f));
}

constexpr uint16_t frequencyHzToRawPercent(float frequencyHz,
                                           float maximumFrequencyHz) {
  return maximumFrequencyHz <= 0.0f
             ? 0
             : frequencyPercentToRaw(frequencyHz * 100.0f /
                                     maximumFrequencyHz);
}

}  // namespace delixi::protocol

