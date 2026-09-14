#include "WaterLevelLoRaProtocol.h"

#include <math.h>

namespace irrigation::water_level::lorawan_protocol {
namespace {

uint16_t clampU16(float value) {
  if (!isfinite(value) || value <= 0.0F) return 0;
  if (value >= 65535.0F) return UINT16_MAX;
  return static_cast<uint16_t>(lroundf(value));
}

int16_t clampI16(float value) {
  if (!isfinite(value)) return 0;
  if (value <= -32768.0F) return INT16_MIN;
  if (value >= 32767.0F) return INT16_MAX;
  return static_cast<int16_t>(lroundf(value));
}

void putU16(uint8_t *payload, size_t offset, uint16_t value) {
  payload[offset] = static_cast<uint8_t>(value >> 8);
  payload[offset + 1] = static_cast<uint8_t>(value & 0xFF);
}

}  // namespace

void encodeTelemetry(const Telemetry &telemetry,
                     uint8_t payload[TELEMETRY_SIZE]) {
  payload[0] = VERSION;
  payload[1] = static_cast<uint8_t>((telemetry.pressureValid ? 0x01 : 0x00) |
                                    (telemetry.loadOn ? 0x02 : 0x00));
  putU16(payload, 2, clampU16(telemetry.batteryVoltage * 1000.0F));
  putU16(payload, 4,
         static_cast<uint16_t>(clampI16(telemetry.pressureBar * 1000.0F)));
  putU16(payload, 6, clampU16(telemetry.depthMeters * 1000.0F));
  putU16(payload, 8, clampU16(telemetry.levelPercent * 10.0F));
}

}  // namespace irrigation::water_level::lorawan_protocol
