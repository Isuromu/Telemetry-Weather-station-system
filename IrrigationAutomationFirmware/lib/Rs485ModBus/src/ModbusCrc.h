#pragma once

#include <stddef.h>
#include <stdint.h>

namespace modbus {

constexpr uint16_t crc16(const uint8_t *data, size_t length) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < length; ++i) {
    crc ^= static_cast<uint16_t>(data[i]);
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc & 0x0001U) ? static_cast<uint16_t>((crc >> 1U) ^ 0xA001U)
                            : static_cast<uint16_t>(crc >> 1U);
    }
  }
  return crc;
}

inline bool verifyFrame(const uint8_t *frame, size_t length) {
  if (frame == nullptr || length < 3) return false;
  const uint16_t expected = crc16(frame, length - 2);
  const uint16_t received = static_cast<uint16_t>(frame[length - 2]) |
                            (static_cast<uint16_t>(frame[length - 1]) << 8U);
  return expected == received;
}

}  // namespace modbus
