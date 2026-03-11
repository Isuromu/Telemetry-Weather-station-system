#pragma once

#include <Arduino.h>
// ============================================================
// PCB profile: ESP32-S3 V2
// Current board target:
// - ESP32-S3
// - one 12V sensor power line
// - one RS485 port
// ============================================================

#define PCB_NAME "ESP32S3_V2"

// ------------------------------------------------------------
// Generic index constants
// ------------------------------------------------------------
#define POWERLINE_INDEX_0 0
#define RS485_PORT_INDEX_0 0

// ------------------------------------------------------------
// Debug / service UART
// Same port as upload bridge
// ------------------------------------------------------------
#define PCB_DEBUG_SERIAL_BAUD 115200

// ------------------------------------------------------------
// Power Lines
// ------------------------------------------------------------
constexpr uint8_t PCB_POWERLINE_COUNT = 1;

// Current V2 board:
// PWR_12V pin = 4
// In old code LOW = ON, HIGH = OFF
constexpr int8_t PCB_POWERLINE_SWITCH_PINS[PCB_POWERLINE_COUNT] = {4};
constexpr bool   PCB_POWERLINE_ACTIVE_HIGH[PCB_POWERLINE_COUNT] = {false};

// If no hardware status pin exists yet, keep -1
constexpr int8_t PCB_POWERLINE_STATUS_PINS[PCB_POWERLINE_COUNT] = {-1};
constexpr bool   PCB_POWERLINE_STATUS_ACTIVE_HIGH[PCB_POWERLINE_COUNT] = {true};

constexpr uint8_t PCB_POWERLINE_VOLTAGES[PCB_POWERLINE_COUNT] = {12};

// Set to true only if this PCB/hardware design allows the line
// to remain ON through station sleep logic safely
constexpr bool PCB_POWERLINE_CAN_STAY_ON_IN_SLEEP[PCB_POWERLINE_COUNT] = {true};

// ------------------------------------------------------------
// RS485 Ports
// ------------------------------------------------------------
constexpr uint8_t PCB_RS485_PORT_COUNT = 1;

// Current V2 board uses one RS485 UART:
// RX=17 TX=18 DE=21
constexpr int8_t PCB_RS485_RX_PINS[PCB_RS485_PORT_COUNT] = {17};
constexpr int8_t PCB_RS485_TX_PINS[PCB_RS485_PORT_COUNT] = {18};
constexpr int8_t PCB_RS485_DE_PINS[PCB_RS485_PORT_COUNT] = {21};
constexpr bool   PCB_RS485_DE_ACTIVE_HIGH[PCB_RS485_PORT_COUNT] = {true};

// Optional extra enable pin for full RS485 interface power/enable control
// If not used yet, keep -1
constexpr int8_t PCB_RS485_ENABLE_PINS[PCB_RS485_PORT_COUNT] = {-1};
constexpr bool   PCB_RS485_ENABLE_ACTIVE_HIGH[PCB_RS485_PORT_COUNT] = {true};

// Optional interface status pins
constexpr int8_t PCB_RS485_STATUS_PINS[PCB_RS485_PORT_COUNT] = {-1};
constexpr bool   PCB_RS485_STATUS_ACTIVE_HIGH[PCB_RS485_PORT_COUNT] = {true};

// Optional enable stabilization delay
constexpr uint16_t PCB_RS485_ENABLE_DELAY_MS[PCB_RS485_PORT_COUNT] = {0};

// ------------------------------------------------------------
// Misc service pins
// ------------------------------------------------------------
#define PCB_SERVICE_BUTTON_PIN 14