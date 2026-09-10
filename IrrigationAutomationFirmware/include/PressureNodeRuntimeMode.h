#pragma once

#include <stdint.h>

#define PCV_RUNTIME_SERIAL_ONLY 1
#define PCV_RUNTIME_HYBRID_CLASS_C 2
#define PCV_RUNTIME_LOW_POWER_CLASS_A 3

#ifndef PRESSURE_NODE_RUNTIME_MODE
#define PRESSURE_NODE_RUNTIME_MODE PCV_RUNTIME_SERIAL_ONLY
#endif

namespace irrigation::pressure_node::runtime {

enum class Mode : uint8_t {
  SerialOnly = PCV_RUNTIME_SERIAL_ONLY,
  HybridClassC = PCV_RUNTIME_HYBRID_CLASS_C,
  LowPowerClassA = PCV_RUNTIME_LOW_POWER_CLASS_A,
};

inline constexpr Mode ACTIVE_MODE =
    static_cast<Mode>(PRESSURE_NODE_RUNTIME_MODE);

inline constexpr bool SERIAL_COMMANDS_ENABLED =
    ACTIVE_MODE == Mode::SerialOnly || ACTIVE_MODE == Mode::HybridClassC;
inline constexpr bool LORAWAN_ENABLED = ACTIVE_MODE != Mode::SerialOnly;
inline constexpr bool CLASS_C_ENABLED = ACTIVE_MODE == Mode::HybridClassC;
inline constexpr bool DEEP_SLEEP_ENABLED =
    ACTIVE_MODE == Mode::LowPowerClassA;

constexpr const char *modeName(Mode mode) {
  switch (mode) {
    case Mode::SerialOnly:
      return "PCV_SERIAL_ONLY";
    case Mode::HybridClassC:
      return "PCV_HYBRID_CLASS_C";
    case Mode::LowPowerClassA:
      return "PCV_LOW_POWER_CLASS_A";
  }
  return "INVALID";
}

static_assert(PRESSURE_NODE_RUNTIME_MODE >= PCV_RUNTIME_SERIAL_ONLY &&
                  PRESSURE_NODE_RUNTIME_MODE <=
                      PCV_RUNTIME_LOW_POWER_CLASS_A,
              "Select a valid pressure-valve runtime mode.");

}  // namespace irrigation::pressure_node::runtime
