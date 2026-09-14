#pragma once

#include <stddef.h>
#include <stdint.h>

namespace irrigation::water_level::lorawan_protocol {

inline constexpr uint8_t VERSION = 1;
inline constexpr size_t TELEMETRY_SIZE = 10;

struct Telemetry {
  float batteryVoltage{0.0F};
  float pressureBar{0.0F};
  float depthMeters{0.0F};
  float levelPercent{0.0F};
  bool pressureValid{false};
  bool loadOn{false};
};

void encodeTelemetry(const Telemetry &telemetry,
                     uint8_t payload[TELEMETRY_SIZE]);

}  // namespace irrigation::water_level::lorawan_protocol
