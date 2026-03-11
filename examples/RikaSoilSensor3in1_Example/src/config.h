#pragma once
#include <Arduino.h>
#include "../../../config/Configuration_System.h"
#include "../../../config/Configuration_PCB.h"

/*
  Rika Soil Sensor 3-in-1 Example - local example config
*/

#define SENSOR_ID                   "soil_test_00"
#define SENSOR_ADDRESS              0x01
#define SENSOR_DEBUG                false

#define NEW_ADDRESS                 0x10
#define DO_SCAN                     false
#define SET_SOIL_TYPE_ON_BOOT       false
#define BOOT_SOIL_TYPE              3
#define READ_SOIL_TYPE_IN_LOOP      true
#define READ_EPSILON_IN_LOOP        true
#define READ_COMP_COEFFS_IN_LOOP    true
#define POLL_INTERVAL_MS            2000

#define REG_EC_TEMP_COEFF           0x0022
#define REG_SALINITY_COEFF          0x0023
#define REG_TDS_COEFF               0x0024