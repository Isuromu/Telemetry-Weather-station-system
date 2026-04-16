#pragma once
#include <Arduino.h>

/*
  Configuration.h

  Main station build configuration.

  This file is used by the PRIMARY station firmware in src/main.cpp.

  It is NOT the per-sensor example configuration file.
  Example-specific settings stay inside examples/.../src/config.h
*/

// ============================================================
// Station Identification
// IMPORTANT:
// Use only the backend-approved station ID format.
// Do NOT put random text here in production.
//
// Temporary local test ID:
#define STATION_ID "WS-DEV-0001"

// ============================================================
// Station-level debug
// ============================================================
#define STATION_DEBUG true

// ============================================================
// Driver families compiled into this firmware build
// This only means the code is available.
// Actual station sensor instances are selected in
// Configuration_Sensors.h
// ============================================================
#define USE_RIKA_LEAF_SENSOR
#define USE_RIKA_SOIL_3IN1
