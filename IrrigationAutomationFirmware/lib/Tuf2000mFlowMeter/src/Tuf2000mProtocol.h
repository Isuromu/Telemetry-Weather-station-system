#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace irrigation::pressure_node::tuf2000m {

enum class FloatWordOrder : uint8_t {
  Unspecified,
  HighWordFirst,
  LowWordFirst
};

namespace protocol {

inline constexpr uint8_t READ_HOLDING_REGISTERS = 0x03;
inline constexpr size_t READ_REQUEST_LENGTH_WITHOUT_CRC = 6;

// The manual numbers registers from 1, while the Modbus request uses a
// zero-based start address. Its example maps REG0001 to start address 0x0000.
constexpr uint16_t modbusAddress(uint16_t documentedRegister) {
  return documentedRegister == 0 ? 0 : documentedRegister - 1U;
}

inline constexpr uint16_t FLOW_RATE_REGISTER = modbusAddress(1);
inline constexpr uint16_t VELOCITY_REGISTER = modbusAddress(5);
inline constexpr uint16_t ERROR_CODE_REGISTER = modbusAddress(72);
inline constexpr uint16_t SIGNAL_QUALITY_REGISTER = modbusAddress(92);
// REG0113-REG0118 provide net, positive, and negative accumulators directly
// as REAL4 cubic-metre values. They avoid the unit/multiplier reconstruction
// required by the older LONG-plus-fraction registers.
inline constexpr uint16_t FLOW_TOTALS_REGISTER = modbusAddress(113);

// One request for REG0001-REG0006 returns flow rate, energy flow rate, and
// velocity. The energy value in the middle is intentionally ignored.
inline constexpr uint16_t FLOW_AND_VELOCITY_REGISTER_COUNT = 6;
inline constexpr uint8_t FLOW_AND_VELOCITY_BYTE_COUNT = 12;
inline constexpr uint16_t ERROR_CODE_REGISTER_COUNT = 1;
inline constexpr uint8_t ERROR_CODE_BYTE_COUNT = 2;
inline constexpr uint16_t FLOW_TOTALS_REGISTER_COUNT = 6;
inline constexpr uint8_t FLOW_TOTALS_BYTE_COUNT = 12;

// Bits that make an instantaneous flow sample unreliable. Output-only
// overflow bits and energy/analog-input-only faults remain visible in the
// reading but do not invalidate flow by themselves.
inline constexpr uint16_t FLOW_VALIDITY_ERROR_MASK = 0x4F3FU;

struct ReadHoldingRegistersRequest {
  uint8_t bytes[READ_REQUEST_LENGTH_WITHOUT_CRC]{};
};

constexpr ReadHoldingRegistersRequest makeReadHoldingRegistersRequest(
    uint8_t slaveAddress, uint16_t startAddress, uint16_t registerCount) {
  return {{slaveAddress,
           READ_HOLDING_REGISTERS,
           static_cast<uint8_t>(startAddress >> 8U),
           static_cast<uint8_t>(startAddress & 0xFFU),
           static_cast<uint8_t>(registerCount >> 8U),
           static_cast<uint8_t>(registerCount & 0xFFU)}};
}

constexpr uint16_t decodeUint16(const uint8_t *bytes) {
  return (static_cast<uint16_t>(bytes[0]) << 8U) |
         static_cast<uint16_t>(bytes[1]);
}

inline bool decodeReal4(const uint8_t *bytes, FloatWordOrder wordOrder,
                        float &value) {
  if (bytes == nullptr || wordOrder == FloatWordOrder::Unspecified)
    return false;

  uint32_t raw = 0;
  if (wordOrder == FloatWordOrder::HighWordFirst) {
    raw = (static_cast<uint32_t>(bytes[0]) << 24U) |
          (static_cast<uint32_t>(bytes[1]) << 16U) |
          (static_cast<uint32_t>(bytes[2]) << 8U) |
          static_cast<uint32_t>(bytes[3]);
  } else {
    raw = (static_cast<uint32_t>(bytes[2]) << 24U) |
          (static_cast<uint32_t>(bytes[3]) << 16U) |
          (static_cast<uint32_t>(bytes[0]) << 8U) |
          static_cast<uint32_t>(bytes[1]);
  }
  static_assert(sizeof(value) == sizeof(raw),
                "TUF-2000M REAL4 requires a 32-bit float.");
  memcpy(&value, &raw, sizeof(value));
  return true;
}

inline bool decodeFlowAndVelocity(const uint8_t *data, size_t dataLength,
                                  FloatWordOrder wordOrder,
                                  float &flowRateM3PerHour,
                                  float &velocityMetersPerSecond) {
  if (data == nullptr || dataLength != FLOW_AND_VELOCITY_BYTE_COUNT)
    return false;
  return decodeReal4(&data[0], wordOrder, flowRateM3PerHour) &&
         decodeReal4(&data[8], wordOrder, velocityMetersPerSecond);
}

inline bool decodeFlowTotals(const uint8_t *data, size_t dataLength,
                             FloatWordOrder wordOrder,
                             float &netCubicMeters,
                             float &positiveCubicMeters,
                             float &negativeCubicMeters) {
  if (data == nullptr || dataLength != FLOW_TOTALS_BYTE_COUNT) return false;
  return decodeReal4(&data[0], wordOrder, netCubicMeters) &&
         decodeReal4(&data[4], wordOrder, positiveCubicMeters) &&
         decodeReal4(&data[8], wordOrder, negativeCubicMeters);
}

}  // namespace protocol
}  // namespace irrigation::pressure_node::tuf2000m
