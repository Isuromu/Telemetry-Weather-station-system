#pragma once
#include <Arduino.h>
#include "../../../config/Configuration_System.h"
#include "../../../config/Configuration_PCB.h"

/*
  Rika Soil Sensor 3-in-1 Example - local example config

  Scope:
  - This file only affects examples/RikaSoilSensor3in1_Example.
  - Transport defaults, retry defaults, and pin mappings come from
    Configuration_System.h and Configuration_PCB.h.

  Typical workflow:
  - Set SENSOR_ADDRESS to the known Modbus address.
  - Use DO_SCAN=true if the address is unknown.
  - Enable optional loop reads only when needed to reduce bus traffic.
*/

// Logical ID used in logs and telemetry tagging.
#define SENSOR_ID                   "soil_test_00"
// Modbus node address currently configured in the sensor.
#define SENSOR_ADDRESS              0x01
// Enables additional driver/bus debug traces.
#define SENSOR_DEBUG                false

// Reserved for address-change flows (not used directly in this example loop).
#define NEW_ADDRESS                 0x10
// Scan address range on boot and print first responsive sensor.
#define DO_SCAN                     false
// If true, write BOOT_SOIL_TYPE once during setup().
#define SET_SOIL_TYPE_ON_BOOT       false
// Soil type raw value expected by register 0x0020 (0..3 in current driver enum).
#define BOOT_SOIL_TYPE              3

// Optional extra reads in loop().
#define READ_SOIL_TYPE_IN_LOOP      true
#define READ_EPSILON_IN_LOOP        true
#define READ_COMP_COEFFS_IN_LOOP    true

// Delay between loop() polling cycles.
#define POLL_INTERVAL_MS            2000

// Compensation coefficient holding-register addresses.
#define REG_EC_TEMP_COEFF           0x0022
#define REG_SALINITY_COEFF          0x0023
#define REG_TDS_COEFF               0x0024
