#pragma once

#include <stdint.h>

// Copy this file to WaterLevelLoRaSecrets.h and replace every placeholder
// with credentials provisioned specifically for the WaterLevel node. Do not
// commit the resulting secrets file.
namespace irrigation::water_level::lorawan_secrets {

inline constexpr bool CONFIGURED = false;
inline constexpr uint64_t JOIN_EUI = 0x0000000000000000ULL;
inline constexpr uint64_t DEV_EUI = 0x0000000000000000ULL;
inline uint8_t APP_KEY[16] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

}  // namespace irrigation::water_level::lorawan_secrets
