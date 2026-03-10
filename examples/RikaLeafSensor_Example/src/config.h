#pragma once

// =============================================================
//  Rika Leaf Sensor Simple Example - Configuration
// =============================================================

// ----- Debug Serial -----
#define SERIAL_BAUD       115200

// ----- RS485 Bus -----
#define RS485_BAUD        9600
#define RS485_CONFIG      SERIAL_8N1

// ----- Sensor Settings -----
#define SENSOR_ID         "leaf_s1"
#define SENSOR_ADDRESS    0x80
#define SENSOR_DEBUG      false

// ----- Address Change -----
// Press the button at boot to change the current sensor address to NEW_ADDRESS.
// IMPORTANT:
//   - Only one sensor must be on the RS485 bus during address change.
//   - The sensor white wire must be connected to V+ for address change mode.
#define NEW_ADDRESS       0x01
#define CHANGE_BUTTON_PIN 14

// ----- Diagnostic Scan -----
// If true, setup() scans addresses 1..247 and stores the found address.
// This is only for field diagnostics and first-time setup.
#define DO_SCAN           false

// ----- Simple Polling Loop -----
#define POLL_INTERVAL_MS  2000
