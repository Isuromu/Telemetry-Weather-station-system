#pragma once

#include <stdint.h>

// Fallback used when the node-local lorawan_keys.h is absent. Both files are
// interchangeable: ValveLoRaWan.h prefers the real one and compiles with this
// zeroed placeholder otherwise, so a fresh clone still builds.
//
// To commission a board, copy this file to lorawan_keys.h in the same
// directory and replace every value with the credentials of that end node.
// lorawan_keys.h is git-ignored; never commit or log real keys, and never copy
// credentials from another end node.
namespace LoRaKeys {

inline constexpr uint64_t joinEui = 0x0000000000000000ULL;
inline constexpr uint64_t devEui = 0x0000000000000000ULL;
inline uint8_t appKey[16] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

}  // namespace LoRaKeys
