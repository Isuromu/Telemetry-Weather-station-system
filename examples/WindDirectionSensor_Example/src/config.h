#pragma once

// =============================================================
//  Wind Direction Sensor Example — User Configuration
// =============================================================

#define SERIAL_BAUD 115200
#define PRINT_VERBOSITY 2
#define PRINT_ENABLED 1

#define RS485_BAUD 9600
#define SENSOR_DEFAULT_ADDRESS 0x01
#define POLL_INTERVAL_MS 3000
#define SCAN_TIMEOUT_MS 100

#define SCAN_BUTTON_PIN 14
#define SCAN_WINDOW_MS 3000

#if defined(ARDUINO_ARCH_ESP32)
#define RS485_RX_PIN 16
#define RS485_TX_PIN 17
#define RS485_DE_PIN 21
#endif
