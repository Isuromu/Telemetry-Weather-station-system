#pragma once

#include <ConfigTypes.h>

namespace irrigation::boards {

// Architecture placeholder only. The user will provide the ESP32-H2 pinout.
// No GPIO number is intentionally assigned here.
inline constexpr BoardProfile ESP32_H2{
    "ESP32_H2",
    "ESP32-H2 (pinout pending)",
    false,
    115200,
    -1,
    -1,
    Rs485DirectionMode::Automatic,
    -1,
    true,
    -1,
    false,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

}  // namespace irrigation::boards

