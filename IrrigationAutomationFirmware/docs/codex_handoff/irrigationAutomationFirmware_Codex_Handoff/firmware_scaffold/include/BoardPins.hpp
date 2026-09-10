#pragma once

#include <Arduino.h>

namespace BoardPins {

// Battery
constexpr uint8_t BATTERY_ADC = 35;

// Pressure Control Valve driver
constexpr uint8_t PCV_IN1 = 16;
constexpr uint8_t PCV_IN2 = 17;
constexpr uint8_t PCV_POWER_ENABLE = 27;

// Prototype Rev A: two physical I2C buses
constexpr uint8_t I2C_UPSTREAM_SDA = 21;
constexpr uint8_t I2C_UPSTREAM_SCL = 22;

constexpr uint8_t I2C_DOWNSTREAM_SDA = 13;
constexpr uint8_t I2C_DOWNSTREAM_SCL = 4;

// SX1262 / DX-LR30
constexpr uint8_t LORA_NSS = 5;
constexpr uint8_t LORA_MOSI = 23;
constexpr uint8_t LORA_DIO1 = 26;
constexpr uint8_t LORA_RXEN = 33;
constexpr uint8_t LORA_NRST = 14;
constexpr uint8_t LORA_SCK = 18;
constexpr uint8_t LORA_MISO = 19;
constexpr uint8_t LORA_BUSY = 25;
constexpr uint8_t LORA_TXEN = 32;

// Target Rev B, after replacing the second physical I2C bus with a mux.
constexpr uint8_t FUTURE_RS485_TX = 13;
constexpr uint8_t FUTURE_RS485_RX = 34;
constexpr uint8_t FUTURE_RS485_DE_RE = 4;

}  // namespace BoardPins
