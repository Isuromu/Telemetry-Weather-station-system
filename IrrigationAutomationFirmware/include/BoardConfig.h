#pragma once

#include <ProjectConfig.h>

#if ACTIVE_BOARD == BOARD_ESP32_WROOM32D
#include <config/boards/ESP32_WROOM32D.h>
namespace irrigation {
inline constexpr const BoardProfile &ActiveBoard = boards::ESP32_WROOM32D;
}
#elif ACTIVE_BOARD == BOARD_ESP32_H2
#include <config/boards/ESP32_H2.h>
namespace irrigation {
inline constexpr const BoardProfile &ActiveBoard = boards::ESP32_H2;
}
#else
#error "Unknown ACTIVE_BOARD selection"
#endif

static_assert(irrigation::ActiveBoard.pinoutComplete,
              "The selected board pinout is not available yet.");

