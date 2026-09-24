#pragma once

#include <stdint.h>

// Protocol constants verified against the INVT GD200A Series VFD manual.
// This staging header intentionally exposes no motor-control API yet.
namespace invt::gd200a::protocol {

constexpr uint8_t READ_HOLDING_REGISTERS = 0x03;
constexpr uint8_t WRITE_SINGLE_REGISTER = 0x06;

constexpr uint16_t CONTROL_COMMAND = 0x2000;
constexpr uint16_t FREQUENCY_COMMAND = 0x2001;
constexpr uint16_t RUN_STATE = 0x2100;
constexpr uint16_t STATUS_WORD = 0x2101;
constexpr uint16_t FAULT_CODE = 0x2102;
constexpr uint16_t DEVICE_CODE = 0x2103;

constexpr uint16_t OPERATING_FREQUENCY = 0x3000;
constexpr uint16_t SETTING_FREQUENCY = 0x3001;
constexpr uint16_t BUS_VOLTAGE = 0x3002;
constexpr uint16_t OUTPUT_VOLTAGE = 0x3003;
constexpr uint16_t OUTPUT_CURRENT = 0x3004;
constexpr uint16_t OPERATING_SPEED = 0x3005;
constexpr uint16_t ALTERNATE_FAULT_CODE = 0x5000;

constexpr uint16_t EXPECTED_DEVICE_CODE = 0x0107;

constexpr uint16_t COMMAND_FORWARD_RUN = 0x0001;
constexpr uint16_t COMMAND_REVERSE_RUN = 0x0002;
constexpr uint16_t COMMAND_FORWARD_JOG = 0x0003;
constexpr uint16_t COMMAND_REVERSE_JOG = 0x0004;
constexpr uint16_t COMMAND_STOP = 0x0005;
constexpr uint16_t COMMAND_COAST_TO_STOP = 0x0006;
constexpr uint16_t COMMAND_FAULT_RESET = 0x0007;
constexpr uint16_t COMMAND_JOG_STOP = 0x0008;

constexpr uint16_t parameterAddress(uint8_t group, uint8_t index) {
  return static_cast<uint16_t>((static_cast<uint16_t>(group) << 8U) | index);
}

constexpr uint16_t frequencyHzToRaw(float frequencyHz) {
  return frequencyHz <= 0.0f
             ? 0
             : static_cast<uint16_t>(frequencyHz * 100.0f + 0.5f);
}

}  // namespace invt::gd200a::protocol
