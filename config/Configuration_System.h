#pragma once
#include <Arduino.h>

/*
  Configuration_System.h

  Common project-wide system constants shared by:
  - main firmware
  - sensor examples
  - future station modules

  This file must NOT contain board-specific GPIO mapping.
  PCB-related hardware details belong in pcb/... files.
*/

// ============================================================
// Allowed sample rates (minutes only)
// Do not use arbitrary values.
// ============================================================
#define SAMPLE_RATE_1_MIN      1
#define SAMPLE_RATE_5_MIN      5
#define SAMPLE_RATE_15_MIN     15
#define SAMPLE_RATE_30_MIN     30
#define SAMPLE_RATE_60_MIN     60
#define SAMPLE_RATE_180_MIN    180

// ============================================================
// Common defaults
// ============================================================
#define SENSOR_DEFAULT_MAX_ERRORS       10
#define SENSOR_DEFAULT_BUS_RETRIES      3
#define SENSOR_DEFAULT_DRIVER_RETRIES   3
#define SENSOR_DEFAULT_READ_TIMEOUT_MS  2000
#define SENSOR_DEFAULT_AFTER_REQ_MS     20
#define RS485_DEFAULT_BAUD              9600
#define RS485_DEFAULT_SERIAL_CONFIG     SERIAL_8N1

// ============================================================
// Power policy
// If remaining OFF time is smaller than this window,
// the sensor should stay powered ON.
// ============================================================
#define MIN_USEFUL_POWER_OFF_MS         60000UL