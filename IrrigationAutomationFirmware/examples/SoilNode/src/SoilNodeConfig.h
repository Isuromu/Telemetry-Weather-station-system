#pragma once

#include <RadioLib.h>
#include <stdint.h>

#ifndef SOIL_NODE_SLEEP_SECONDS
#define SOIL_NODE_SLEEP_SECONDS 10 // @fixme: 600
#endif

#ifndef SOIL_NODE_SENSOR_POWER_PIN
// Prototype transistor power switch for the RS485 soil sensor.
#define SOIL_NODE_SENSOR_POWER_PIN 27
#endif

#ifndef SOIL_NODE_BATTERY_DIVIDER_RATIO
// The supplied log showed 3.514 V at ADS1115 A0 on a 1S battery, so the
// current prototype reads battery voltage directly rather than through 1:1
// divider scaling. Override this if the fitted PCB divider differs.
#define SOIL_NODE_BATTERY_DIVIDER_RATIO 1.0F
#endif

namespace irrigation::soil_node::config {

// Prototype ESP32-WROOM-32D pinout. This is intentionally different from the
// preliminary production ESP32-C6 pinout documented under docs/.
namespace pins {
inline constexpr int SENSOR_POWER = SOIL_NODE_SENSOR_POWER_PIN;
inline constexpr int I2C_SDA = 21;
inline constexpr int I2C_SCL = 22;
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

namespace sensor {
inline constexpr uint32_t BAUD = 9600;
inline constexpr uint8_t MODBUS_ID = 0x03;
inline constexpr uint16_t FIRST_REGISTER = 0x0000;
inline constexpr uint16_t REGISTER_COUNT = 3;
inline constexpr uint32_t POWER_UP_MS = 2000;
inline constexpr uint32_t RESPONSE_TIMEOUT_MS = 500;
inline constexpr uint8_t READ_ATTEMPTS = 3;
inline constexpr uint32_t RETRY_DELAY_MS = 100;
}  // namespace sensor

namespace battery {
inline constexpr uint8_t ADS1115_CHANNEL = 0;
inline constexpr float DIVIDER_RATIO = SOIL_NODE_BATTERY_DIVIDER_RATIO;
inline constexpr float ENCODED_OFFSET_VOLTS = 2.0F;
inline constexpr float MIN_VOLTS = 2.0F;
inline constexpr float MAX_VOLTS = 4.55F;
}  // namespace battery

namespace lorawan {
inline constexpr const LoRaWANBand_t &REGION = EU868;
inline constexpr uint8_t SUB_BAND = 0;
inline constexpr uint8_t TELEMETRY_FPORT = 10;
inline constexpr uint32_t DEFAULT_SLEEP_SECONDS = SOIL_NODE_SLEEP_SECONDS;
inline constexpr uint32_t MIN_SLEEP_SECONDS = 10;
inline constexpr uint32_t MAX_SLEEP_SECONDS = 24UL * 60UL * 60UL;
inline constexpr char NVS_NAMESPACE[] = "soilnode_lw";
inline constexpr char NVS_NONCES_KEY[] = "nonces";
inline constexpr char NVS_SLEEP_SECONDS_KEY[] = "sleep_s";
}  // namespace lorawan

static_assert(lorawan::DEFAULT_SLEEP_SECONDS >= lorawan::MIN_SLEEP_SECONDS &&
                  lorawan::DEFAULT_SLEEP_SECONDS <=
                      lorawan::MAX_SLEEP_SECONDS,
              "SoilNode sleep interval must be 10..86400 seconds.");

}  // namespace irrigation::soil_node::config
