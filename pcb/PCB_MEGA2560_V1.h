#pragma once

#include <Arduino.h>

// ============================================================
// PCB profile: Mega2560 V1
// ============================================================

#define PCB_NAME "MEGA2560_V1"

#define POWERLINE_INDEX_0 0
#define RS485_PORT_INDEX_0 0

#define PCB_DEBUG_SERIAL_BAUD 115200

// ------------------------------------------------------------
// Power Lines
// ------------------------------------------------------------
constexpr uint8_t PCB_POWERLINE_COUNT = 3;

constexpr int8_t PCB_POWERLINE_SWITCH_PINS[PCB_POWERLINE_COUNT] = {2, 3, 4};
constexpr bool   PCB_POWERLINE_ACTIVE_HIGH[PCB_POWERLINE_COUNT] = {true, true, true};

constexpr int8_t PCB_POWERLINE_STATUS_PINS[PCB_POWERLINE_COUNT] = {-1, -1, -1};
constexpr bool   PCB_POWERLINE_STATUS_ACTIVE_HIGH[PCB_POWERLINE_COUNT] = {true, true, true};

constexpr uint8_t PCB_POWERLINE_VOLTAGES[PCB_POWERLINE_COUNT] = {0, 5, 12};
constexpr bool PCB_POWERLINE_CAN_STAY_ON_IN_SLEEP[PCB_POWERLINE_COUNT] = {true, true, true};

// ------------------------------------------------------------
// RS485 Ports
// ------------------------------------------------------------
constexpr uint8_t PCB_RS485_PORT_COUNT = 1;

// Mega2560 Serial2: RX2=D17, TX2=D16. DE/RE is from the older test sketch.
constexpr int8_t PCB_RS485_RX_PINS[PCB_RS485_PORT_COUNT] = {17};
constexpr int8_t PCB_RS485_TX_PINS[PCB_RS485_PORT_COUNT] = {16};
constexpr int8_t PCB_RS485_DE_PINS[PCB_RS485_PORT_COUNT] = {25};
constexpr bool   PCB_RS485_DE_ACTIVE_HIGH[PCB_RS485_PORT_COUNT] = {true};

constexpr int8_t PCB_RS485_ENABLE_PINS[PCB_RS485_PORT_COUNT] = {-1};
constexpr bool   PCB_RS485_ENABLE_ACTIVE_HIGH[PCB_RS485_PORT_COUNT] = {true};

constexpr int8_t PCB_RS485_STATUS_PINS[PCB_RS485_PORT_COUNT] = {-1};
constexpr bool   PCB_RS485_STATUS_ACTIVE_HIGH[PCB_RS485_PORT_COUNT] = {true};

constexpr uint16_t PCB_RS485_ENABLE_DELAY_MS[PCB_RS485_PORT_COUNT] = {0};

// ------------------------------------------------------------
// Misc service pins
// ------------------------------------------------------------
#define PCB_SERVICE_BUTTON_PIN 6

// ------------------------------------------------------------
// I2C bus
// ------------------------------------------------------------
#define PCB_I2C_SDA_PIN 20
#define PCB_I2C_SCL_PIN 21

// ------------------------------------------------------------
// Rain gauge pulse/counter interface
// ------------------------------------------------------------
#define PCB_RAIN_COUNTER_RESET_PIN 10
#define PCB_RAIN_BYPASS_INTERRUPT_PIN 19
