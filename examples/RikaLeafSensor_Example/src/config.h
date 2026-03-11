#pragma once
#include <Arduino.h>
#include "../../../config/Configuration_System.h"
#include "../../../config/Configuration_PCB.h"

/*
  Rika Leaf Sensor Example - local example config

  This file belongs only to the leaf sensor example.
  It reuses common system/PCB definitions, but keeps its own
  per-example sensor settings.
*/

#define SENSOR_ID         "leaf_test_00"
#define SENSOR_ADDRESS    0x80
#define SENSOR_DEBUG      true

#define NEW_ADDRESS       0x01
#define DO_SCAN           false
#define POLL_INTERVAL_MS  2000