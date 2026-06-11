#pragma once

#include <Arduino.h>

/*
  PCB_TELEMETRY_ESP32S3_V2.h

  Hardware profile for the Telemetry Weather Station PCB.

  This board is close to the Amudario ESP32-S3 V2 profile, but the RS485 UART
  wiring is intentionally reversed:

    Amudario Firmware: RX=17, TX=18
    Telemetry PCB:     RX=18, TX=17

  The values below are the actual ESP32-S3 GPIO pins passed to
  HardwareSerial::begin(baud, config, rxPin, txPin).
*/

#define PCB_NAME "TELEMETRY_ESP32S3_V2"

// ------------------------------------------------------------
// Generic index constants
// ------------------------------------------------------------
#define POWERLINE_INDEX_0 0
#define RS485_PORT_INDEX_0 0

// ------------------------------------------------------------
// Debug / service serial
// ------------------------------------------------------------
#define PCB_DEBUG_SERIAL_BAUD 115200

// ------------------------------------------------------------
// Power lines
// ------------------------------------------------------------
constexpr uint8_t PCB_POWERLINE_COUNT = 1;

// Sensor power is not driven until the real Telemetry PCB rail-control GPIO is
// confirmed. GPIO4 is used by I2C SDA for DS3231, so it must not be toggled as
// a power switch here.
constexpr int8_t PCB_POWERLINE_SWITCH_PINS[PCB_POWERLINE_COUNT] = {-1};
constexpr bool   PCB_POWERLINE_ACTIVE_HIGH[PCB_POWERLINE_COUNT] = {false};

// No feedback/status pin is wired on this profile yet.
constexpr int8_t PCB_POWERLINE_STATUS_PINS[PCB_POWERLINE_COUNT] = {-1};
constexpr bool   PCB_POWERLINE_STATUS_ACTIVE_HIGH[PCB_POWERLINE_COUNT] = {true};

constexpr uint8_t PCB_POWERLINE_VOLTAGES[PCB_POWERLINE_COUNT] = {12};

// The Telemetry board can keep the sensor power rail on while the firmware is
// awake between reads. Sleep behavior is handled by the station logic.
constexpr bool PCB_POWERLINE_CAN_STAY_ON_IN_SLEEP[PCB_POWERLINE_COUNT] = {true};

// ------------------------------------------------------------
// RS485 port
// ------------------------------------------------------------
constexpr uint8_t PCB_RS485_PORT_COUNT = 1;

// Telemetry board RS485 UART pinout. This is opposite to Amudario Firmware.
constexpr int8_t PCB_RS485_RX_PINS[PCB_RS485_PORT_COUNT] = {18};
constexpr int8_t PCB_RS485_TX_PINS[PCB_RS485_PORT_COUNT] = {17};
constexpr int8_t PCB_RS485_DE_PINS[PCB_RS485_PORT_COUNT] = {21};
constexpr bool   PCB_RS485_DE_ACTIVE_HIGH[PCB_RS485_PORT_COUNT] = {true};

// Optional RS485 interface enable pin. Keep -1 when the transceiver is always
// available or controlled only by the DE pin above.
constexpr int8_t PCB_RS485_ENABLE_PINS[PCB_RS485_PORT_COUNT] = {-1};
constexpr bool   PCB_RS485_ENABLE_ACTIVE_HIGH[PCB_RS485_PORT_COUNT] = {true};

// Optional interface status pins.
constexpr int8_t PCB_RS485_STATUS_PINS[PCB_RS485_PORT_COUNT] = {-1};
constexpr bool   PCB_RS485_STATUS_ACTIVE_HIGH[PCB_RS485_PORT_COUNT] = {true};

// Extra delay after enabling the RS485 interface, if the hardware needs it.
constexpr uint16_t PCB_RS485_ENABLE_DELAY_MS[PCB_RS485_PORT_COUNT] = {0};

// ------------------------------------------------------------
// Misc service pins
// ------------------------------------------------------------
#define PCB_SERVICE_BUTTON_PIN 14

// ------------------------------------------------------------
// I2C bus
// ------------------------------------------------------------
#define PCB_I2C_SDA_PIN 4
#define PCB_I2C_SCL_PIN 5

// ------------------------------------------------------------
// External RTC
// ------------------------------------------------------------
#define PCB_DS3231_ADDRESS 0x68
