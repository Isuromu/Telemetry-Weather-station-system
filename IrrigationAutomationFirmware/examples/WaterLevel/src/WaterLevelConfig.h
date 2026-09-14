#pragma once

#include <stdint.h>

#ifndef WATER_LEVEL_SLEEP_SECONDS
#define WATER_LEVEL_SLEEP_SECONDS 900
#endif

namespace irrigation::water_level::config {

namespace pins {
inline constexpr int BATTERY_ADC = 35;
inline constexpr int LOAD_CONTROL = 27;
inline constexpr int RS485_DE_RE = -1;
inline constexpr int RS485_RX = 16;
inline constexpr int RS485_TX = 17;

inline constexpr int LORA_NSS = 5;
inline constexpr int LORA_DIO1 = 26;
inline constexpr int LORA_RESET = 14;
inline constexpr int LORA_BUSY = 25;
inline constexpr int LORA_SCK = 18;
inline constexpr int LORA_MISO = 19;
inline constexpr int LORA_MOSI = 23;
inline constexpr int LORA_TX_ENABLE = 32;
inline constexpr int LORA_RX_ENABLE = 33;
}  // namespace pins

namespace battery {
inline constexpr float DIVIDER_HIGH_OHM = 100000.0F;
inline constexpr float DIVIDER_LOW_OHM = 20000.0F;
inline constexpr float CALIBRATION = 12.10F / 12.46F;
inline constexpr float LOW_VOLTAGE = 11.5F;
}  // namespace battery

namespace sensor {
inline constexpr uint8_t MODBUS_ID = 1;
inline constexpr uint8_t MODBUS_FUNCTION = 0x03;
inline constexpr uint16_t MODBUS_REGISTER = 0x0004;
inline constexpr uint16_t MODBUS_REGISTER_COUNT = 0x0001;
inline constexpr float PRESSURE_SCALE_BAR = 0.001F;
inline constexpr float RANGE_METERS = 5.0F;
inline constexpr float WATER_DENSITY_KG_M3 = 1000.0F;
inline constexpr float GRAVITY_M_S2 = 9.81F;
inline constexpr uint32_t RESPONSE_TIMEOUT_MS = 300;
}  // namespace sensor

namespace lorawan {
inline constexpr uint8_t TELEMETRY_FPORT = 40;
inline constexpr uint8_t SUB_BAND = 0;
inline constexpr uint32_t SLEEP_SECONDS = WATER_LEVEL_SLEEP_SECONDS;
inline constexpr bool CONFIRMED_UPLINK = false;
inline constexpr char NVS_NAMESPACE[] = "waterlevel_lw";
inline constexpr char NVS_NONCES_KEY[] = "nonces";
}  // namespace lorawan

static_assert(lorawan::SLEEP_SECONDS >= 60 &&
                  lorawan::SLEEP_SECONDS <= 24UL * 60UL * 60UL,
              "WaterLevel sleep interval must be 60..86400 seconds.");

}  // namespace irrigation::water_level::config
