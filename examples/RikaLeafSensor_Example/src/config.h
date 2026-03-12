#pragma once
#include <Arduino.h>
#include "../../../config/Configuration_System.h"
#include "../../../config/Configuration_PCB.h"

/*
  Rika Leaf Sensor Example - local example config

  Scope:
  - This file only affects examples/RikaLeafSensor_Example.
  - Global defaults (timeouts/retries/board pins) still come from:
      Configuration_System.h
      Configuration_PCB.h

  Usage:
  - Update SENSOR_ADDRESS to the actual Modbus node address in your setup.
  - Set DO_SCAN=true during first bring-up if the sensor address is unknown.
  - Keep SENSOR_DEBUG=false unless troubleshooting protocol frames.
*/

// Logical ID string printed in diagnostics/log output.
#define SENSOR_ID         "leaf_test_00"
// Current Modbus node address of the connected leaf sensor.
#define SENSOR_ADDRESS    0x80
// Enables driver/bus debug traces when PrintController allows output.
#define SENSOR_DEBUG      false

// Reserved for manual address programming flows (not used in this example loop).
#define NEW_ADDRESS       0x01
// If true, setup() scans addresses 1..247 and prints first responder.
#define DO_SCAN           false
// Delay between loop() polling cycles.
#define POLL_INTERVAL_MS  2000
