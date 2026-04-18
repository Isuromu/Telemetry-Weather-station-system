#pragma once
#include <Arduino.h>
#include "../../../config/Configuration_System.h"
#include "../../../config/Configuration_PCB.h"

/*
  LeafSensorCompare Example - local example config

  Purpose:
  - Read the Honde RD-LTH-O-01 leaf temperature/humidity sensor first.
  - Then read the JXBS leaf surface humidity sensor.
  - Print both raw driver debug logs and a short comparison summary.

  Addresses are copied from the current single-sensor example configs:
  - examples/HondeLeafTemperatureHumidity_Example/src/config.h
  - examples/JXBS_LeafSurfaceHumidity_Example/src/config.h
*/

#define HONDE_LEAF_SENSOR_ID          "honde_leaf_00"
#define HONDE_LEAF_SENSOR_ADDRESS     0x02
#define HONDE_LEAF_SENSOR_DEBUG       true

#define JXBS_LEAF_SENSOR_ID           "jxbs_leaf_00"
#define JXBS_LEAF_SENSOR_ADDRESS      0x06
#define JXBS_LEAF_SENSOR_DEBUG        true

// Delay between Honde and JXBS reads. This keeps the serial log easier to read.
#define BETWEEN_SENSOR_READ_DELAY_MS   250UL

// Delay between full comparison cycles.
#define POLL_INTERVAL_MS              3000UL
