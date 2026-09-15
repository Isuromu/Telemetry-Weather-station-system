#pragma once

#include <stddef.h>
#include <stdint.h>

namespace irrigation::soil_node::lorawan_protocol {

inline constexpr size_t TELEMETRY_SIZE = 8;

struct Telemetry {
  int16_t temperature100{0};
  uint16_t vwc100{0};
  uint16_t ec1000{0};
  uint8_t batteryEncoded{0};
  bool sensorValid{false};
};

void encodeTelemetry(const Telemetry &telemetry,
                     uint8_t payload[TELEMETRY_SIZE]);

}  // namespace irrigation::soil_node::lorawan_protocol
