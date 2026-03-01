#pragma once

// =============================================================
//  Rika Leaf Sensor Example — User Configuration
//  Edit values here; do NOT modify main.cpp for settings.
// =============================================================

// -------------------------------------------------------------
//  Serial / Debug
// -------------------------------------------------------------

// Baud rate for the USB debug monitor (Serial)
#define SERIAL_BAUD 115200

// PrintController verbosity level:
//   0 = SUMMARY  — high-level status messages only
//   1 = IO       — TX / RX hex frames included
//   2 = TRACE    — full internal details + raw buffers
#define PRINT_VERBOSITY 2

// Set to 0 to disable all PrintController output (silent mode)
#define PRINT_ENABLED 1

// -------------------------------------------------------------
//  RS-485 / Modbus
// -------------------------------------------------------------

// Baud rate for the RS-485 bus (must match sensor DIP switch)
#define RS485_BAUD 9600

// Default Modbus address to poll in normal operation (1–247)
#define SENSOR_DEFAULT_ADDRESS 0x01

// Scan range for address discovery mode.
// JXBS-3001-YMSD manual states supported address range is 0..252.
// Set these to narrow scan if needed.
#define SCAN_ADDRESS_MIN 0
#define SCAN_ADDRESS_MAX 252

// How often the sensor is polled in normal operation (milliseconds)
#define POLL_INTERVAL_MS 3000

// Timeout for each raw Modbus frame during address scan (milliseconds)
#define SCAN_TIMEOUT_MS 100

// -------------------------------------------------------------
//  GPIO / Hardware
// -------------------------------------------------------------

// GPIO pin connected to the scan-mode button (button → GND).
// Internal pull-up is enabled automatically; no external resistor needed.
#define SCAN_BUTTON_PIN 14

// How long (milliseconds) the firmware waits for button press at boot
// before continuing to normal operation
#define SCAN_WINDOW_MS 5000

// ESP32-specific RS-485 UART pin assignment
// (ignored on AVR targets — Serial2 is used there instead)
#if defined(ARDUINO_ARCH_ESP32)
#define RS485_RX_PIN 16
#define RS485_TX_PIN 17
#define RS485_DE_PIN 21 // Direction-enable (DE/RE) pin
#endif
