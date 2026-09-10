#pragma once

#include <Arduino.h>

namespace SystemConfig {

namespace Battery {
constexpr float DIVIDER_HIGH_OHM = 100000.0F;
constexpr float DIVIDER_LOW_OHM = 22000.0F;
constexpr float CALIBRATION = 1.0F;
constexpr uint8_t SAMPLE_COUNT = 32;
}

namespace PressureSensor {
constexpr float FULL_SCALE_BAR = 10.0F;
constexpr uint8_t ADDRESS_PRIMARY = 0x7F;
constexpr uint8_t ADDRESS_ALTERNATE = 0x6D;
constexpr uint32_t I2C_FREQUENCY_HZ = 100000;
}

namespace PressureControlValve {
constexpr bool POWER_ENABLE_ACTIVE_HIGH = true;

// Bench starting values. Validate on the exact installed latching solenoid.
constexpr uint16_t POWER_SETTLE_MS = 20;
constexpr uint16_t SOLENOID_PULSE_MS = 250;
constexpr uint16_t POST_PULSE_MS = 20;
constexpr uint16_t HYDRAULIC_SETTLE_MS = 1000;

// Current assumed physical mapping.
// Swap only here if bench test proves the actual valve moves oppositely.
constexpr bool OPEN_IN1_HIGH = true;
constexpr bool OPEN_IN2_HIGH = false;
constexpr bool CLOSE_IN1_HIGH = false;
constexpr bool CLOSE_IN2_HIGH = true;
}

namespace SerialConsole {
constexpr uint32_t BAUD = 115200;
constexpr size_t COMMAND_BUFFER_LENGTH = 80;
}

namespace RawLoRaBench {
// These are only for a raw-radio bring-up example.
// Final network parameters must follow the deployed radio/network configuration.
constexpr float FREQUENCY_MHZ = 868.0F;
constexpr float BANDWIDTH_KHZ = 125.0F;
constexpr uint8_t SPREADING_FACTOR = 9;
constexpr uint8_t CODING_RATE = 7;
constexpr uint8_t SYNC_WORD = 0x12;
constexpr int8_t OUTPUT_POWER_DBM = 14;
constexpr uint16_t PREAMBLE_LENGTH = 8;
}

}  // namespace SystemConfig
