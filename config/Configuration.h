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
// Station Profile Selection
// ============================================================
/*
  Select one station profile from platformio.ini:
    -D TELEMETRY_STATION_PROFILE_WATER=1
    -D TELEMETRY_STATION_PROFILE_SOIL_AIR=1
    -D TELEMETRY_STATION_PROFILE_GAS_POLLUTION=1

  If no profile is selected, the water station is used as the simplest
  safe default.
*/

#if !defined(TELEMETRY_STATION_PROFILE_WATER) && \
    !defined(TELEMETRY_STATION_PROFILE_SOIL_AIR) && \
    !defined(TELEMETRY_STATION_PROFILE_GAS_POLLUTION)
#define TELEMETRY_STATION_PROFILE_WATER 1
#endif

#if ((defined(TELEMETRY_STATION_PROFILE_WATER) ? 1 : 0) + \
     (defined(TELEMETRY_STATION_PROFILE_SOIL_AIR) ? 1 : 0) + \
     (defined(TELEMETRY_STATION_PROFILE_GAS_POLLUTION) ? 1 : 0)) > 1
#error "Select only one telemetry station profile"
#endif

#if defined(TELEMETRY_STATION_PROFILE_WATER)
#include "stations/Station_Water.h"
#elif defined(TELEMETRY_STATION_PROFILE_SOIL_AIR)
#include "stations/Station_SoilAir.h"
#elif defined(TELEMETRY_STATION_PROFILE_GAS_POLLUTION)
#include "stations/Station_GasPollution.h"
#else
#error "Unknown telemetry station profile"
#endif

#ifndef STATION_ID
#define STATION_ID "WS-DEV-0001"
#endif

#ifndef STATION_NAME
#define STATION_NAME "Telemetry Weather Station"
#endif

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
