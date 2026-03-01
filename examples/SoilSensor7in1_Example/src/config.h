#pragma once
// =============================================================
//  Soil Sensor 7-in-1 Example — User Configuration
//  Edit this file only. Do NOT hard-code values in main.cpp.
// =============================================================

// Serial / Debug
#define SERIAL_BAUD 115200
#define PRINT_VERBOSITY 2 // 0=SUMMARY  1=IO  2=TRACE
#define PRINT_ENABLED 1   // 0=silent

// RS-485 / Modbus
#define RS485_BAUD 9600
#define SENSOR_DEFAULT_ADDRESS 0x01
#define POLL_INTERVAL_MS 3000
#define SCAN_TIMEOUT_MS 100

// Scan-mode button (GPIO14 → GND, internal pull-up)
#define SCAN_BUTTON_PIN 14
#define SCAN_WINDOW_MS 3000

// ESP32 RS-485 pins (ignored on AVR)
#if defined(ARDUINO_ARCH_ESP32)
#define RS485_RX_PIN 16
#define RS485_TX_PIN 17
#define RS485_DE_PIN 21
#endif
