#pragma once

#include <Arduino.h>

/*
  PCB_ESP32S3_V2_TEST.h

  Current REAL hardware state for your test board:
  - ESP32-S3
  - one RS485 port
  - current RS485 module uses automatic direction switching
  - DE/RE GPIO direction control is NOT used
  - extra RS485 interface enable/disable does NOT exist yet
  - one 12V sensor power line
  - 12V power line is physically always ON for now
  - no power-line status pin yet
  - no interface status pin yet

  This file describes the CURRENT TEST hardware only.
*/

// ============================================================
// PCB identity
// ============================================================
#define PCB_NAME "ESP32S3_V2_TEST"

// ============================================================
// Common indexes
// Always count from zero
// ============================================================
#define POWERLINE_INDEX_0 0
#define RS485_PORT_INDEX_0 0

// ============================================================
// Debug / service UART
// Use the same UART side as your upload bridge
// ============================================================
#define PCB_DEBUG_SERIAL_BAUD 115200

// ============================================================
// Power lines
// ============================================================
constexpr uint8_t PCB_POWERLINE_COUNT = 1;

// No MCU control yet -> keep -1
constexpr int8_t PCB_POWERLINE_SWITCH_PINS[PCB_POWERLINE_COUNT] = {-1};
constexpr bool   PCB_POWERLINE_ACTIVE_HIGH[PCB_POWERLINE_COUNT] = {false};

// No hardware status feedback yet
constexpr int8_t PCB_POWERLINE_STATUS_PINS[PCB_POWERLINE_COUNT] = {-1};
constexpr bool   PCB_POWERLINE_STATUS_ACTIVE_HIGH[PCB_POWERLINE_COUNT] = {true};

constexpr uint8_t PCB_POWERLINE_VOLTAGES[PCB_POWERLINE_COUNT] = {12};

// Current test board has this line always ON physically
constexpr bool PCB_POWERLINE_CAN_STAY_ON_IN_SLEEP[PCB_POWERLINE_COUNT] = {true};

// ============================================================
// RS485 ports
// ============================================================
constexpr uint8_t PCB_RS485_PORT_COUNT = 1;

// Real current UART pins
constexpr int8_t PCB_RS485_RX_PINS[PCB_RS485_PORT_COUNT] = {17};
constexpr int8_t PCB_RS485_TX_PINS[PCB_RS485_PORT_COUNT] = {18};

// Current module uses AUTO direction switching -> no DE/RE control pin
constexpr int8_t PCB_RS485_DE_PINS[PCB_RS485_PORT_COUNT] = {-1};
constexpr bool   PCB_RS485_DE_ACTIVE_HIGH[PCB_RS485_PORT_COUNT] = {true};

// Extra full-interface enable pin does NOT exist yet
constexpr int8_t PCB_RS485_ENABLE_PINS[PCB_RS485_PORT_COUNT] = {-1};
constexpr bool   PCB_RS485_ENABLE_ACTIVE_HIGH[PCB_RS485_PORT_COUNT] = {true};

// No interface status pin yet
constexpr int8_t PCB_RS485_STATUS_PINS[PCB_RS485_PORT_COUNT] = {-1};
constexpr bool   PCB_RS485_STATUS_ACTIVE_HIGH[PCB_RS485_PORT_COUNT] = {true};

// No additional stabilization delay yet
constexpr uint16_t PCB_RS485_ENABLE_DELAY_MS[PCB_RS485_PORT_COUNT] = {0};

// ============================================================
// Service pins
// ============================================================
#define PCB_SERVICE_BUTTON_PIN 14