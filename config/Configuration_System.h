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
// Legacy interval constants
// New station firmware reads every configured sensor once per upload cycle.
// These names are kept so old examples and technician tools still compile.
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
// Simple firmware watchdog
// The main firmware feeds the watchdog between clear work blocks.
// If one block hangs longer than this timeout, ESP32 reboots and the
// next JSON upload includes WATCHDOG_RESET in logs.
// ============================================================
#ifndef WATCHDOG_ENABLED
#define WATCHDOG_ENABLED                true
#endif

#ifndef WATCHDOG_TIMEOUT_SECONDS
#define WATCHDOG_TIMEOUT_SECONDS        60
#endif

// Keep early USB-serial logs visible after reset/upload, but never wait forever
// for a monitor in field operation.
#ifndef DEBUG_SERIAL_WAIT_MS
#define DEBUG_SERIAL_WAIT_MS            0UL
#endif

// If a non-idle firmware stage keeps feeding the watchdog for too long, restart
// anyway. This catches "alive but not progressing" waits.
#ifndef STATION_STAGE_HARD_TIMEOUT_MS
#define STATION_STAGE_HARD_TIMEOUT_MS   45000UL
#endif

#ifndef LOOP_HEARTBEAT_INTERVAL_MS
#define LOOP_HEARTBEAT_INTERVAL_MS      10000UL
#endif

// ============================================================
// Power policy
// If remaining OFF time is smaller than this window,
// the sensor should stay powered ON.
// ============================================================
#define MIN_USEFUL_POWER_OFF_MS         60000UL
