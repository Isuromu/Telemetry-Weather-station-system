#pragma once

#include <ConfigTypes.h>

namespace irrigation::boards {

// Pinout transcribed from the supplied ESP32 DevKitC V4 schematic.
// GPIO2 replaces the older GPIO34 power-control assignment.
inline constexpr BoardProfile ESP32_WROOM32D{
    "ESP32_WROOM32D",
    "ESP32 DevKitC V4 / ESP32-WROOM-32D",
    true,
    115200,
    16,
    17,
    Rs485DirectionMode::Automatic,
    -1,
    true,
    2,
    false,
    23,  // LoRa MOSI
    19,  // LoRa MISO
    18,  // LoRa SCK
    5,   // LoRa NSS
    14,  // LoRa NRST
    25,  // LoRa BUSY
    26,  // LoRa DIO1
    -1,  // LoRa DIO2 is not connected in the supplied schematic
    33,  // LoRa RXEN
    32   // LoRa TXEN
};

}  // namespace irrigation::boards

