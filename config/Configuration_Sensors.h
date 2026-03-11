#pragma once
#include <Arduino.h>
#include "Configuration.h"
#include "Configuration_System.h"
#include "Configuration_PCB.h"

/*
  Configuration_Sensors.h

  This file belongs to the PRIMARY station firmware only.

  Here you select which sensor instances exist in the station build,
  what their IDs are, what addresses they use, which power line and
  interface they are connected to, and what timing/debug parameters
  they use.

  Sensor examples DO NOT use this file directly.
  Examples have their own local config.h files.
*/

// ============================================================
// Rika Leaf Sensor
// Measures:
//   - Leaf Wetness
//   - Leaf Temperature
// Interface:
//   - RS485
// Recommended power:
//   - 12V line
// ============================================================

#define RIKA_LEAF_00_ENABLED

#ifdef RIKA_LEAF_00_ENABLED
  #define RIKA_LEAF_00_ID              "leaf_00"
  #define RIKA_LEAF_00_ADDRESS         0x80
  #define RIKA_LEAF_00_RS485_PORT      RS485_PORT_INDEX_0
  #define RIKA_LEAF_00_POWERLINE       POWERLINE_INDEX_0
  #define RIKA_LEAF_00_SAMPLE_RATE     SAMPLE_RATE_5_MIN
  #define RIKA_LEAF_00_WARMUP_MS       1000UL
  #define RIKA_LEAF_00_DEBUG           true
#endif

// ============================================================
// Rika Soil Sensor 3-in-1
// Measures:
//   - Soil Temperature
//   - Soil VWC
//   - Soil EC
// Extra methods:
//   - Read Soil Type
//   - Set Soil Type
//   - Read Epsilon
//   - Read / Set Compensation Coefficients
// Interface:
//   - RS485
// Recommended power:
//   - 12V line
// ============================================================

#define RIKA_SOIL3IN1_00_ENABLED

#ifdef RIKA_SOIL3IN1_00_ENABLED
  #define RIKA_SOIL3IN1_00_ID              "soil_00"
  #define RIKA_SOIL3IN1_00_ADDRESS         0x10
  #define RIKA_SOIL3IN1_00_RS485_PORT      RS485_PORT_INDEX_0
  #define RIKA_SOIL3IN1_00_POWERLINE       POWERLINE_INDEX_0
  #define RIKA_SOIL3IN1_00_SAMPLE_RATE     SAMPLE_RATE_15_MIN
  #define RIKA_SOIL3IN1_00_WARMUP_MS       1000UL
  #define RIKA_SOIL3IN1_00_DEBUG           true
#endif