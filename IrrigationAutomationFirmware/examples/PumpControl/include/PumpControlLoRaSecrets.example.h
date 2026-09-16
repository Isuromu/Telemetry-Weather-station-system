#pragma once

#include <stdint.h>

namespace irrigation::soil_node::lorawan_secrets {
inline constexpr bool CONFIGURED = false;
inline constexpr uint64_t JOIN_EUI = 0x0000000000000000ULL;
inline constexpr uint64_t DEV_EUI = 0x0000000000000000ULL;
inline uint8_t APP_KEY[16] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
}  // namespace irrigation::soil_node::lorawan_secrets
