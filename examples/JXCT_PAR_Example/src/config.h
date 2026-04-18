#pragma once
#include <Arduino.h>
#include "../../../config/Configuration_System.h"
#include "../../../config/Configuration_PCB.h"

/*
  JXCT_PAR Example - local example config

  Sensor:
  - RS485 Photosynthetically Active Radiation Sensor
  - Register 0x0006 = PAR value
  - Local protocol note treats raw as 1 sensor-unit per LSB.
    If your exact probe manual gives another scale, change PAR_SCALE_DIVISOR.
*/

#define SENSOR_ID                   "jxct_par_00"
#define SENSOR_ADDRESS              0x01
#define SENSOR_DEBUG                true

#define PAR_SCALE_DIVISOR           1.0
#define PAR_MAX_VALUE               2000.0

#define DO_SCAN                     false
#define ADDRESS_CHANGE_AT_BOOT      false
#define ADDRESS_CHANGE_NEW_ADDRESS  0x57
// #define ADDRESS_CHANGE_BUTTON_PIN PCB_SERVICE_BUTTON_PIN
#define ADDRESS_CHANGE_CALL_DELAY_MS 5000UL

#define POLL_INTERVAL_MS            2000
