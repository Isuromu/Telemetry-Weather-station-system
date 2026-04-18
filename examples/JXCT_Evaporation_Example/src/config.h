#pragma once
#include <Arduino.h>
#include "../../../config/Configuration_System.h"
#include "../../../config/Configuration_PCB.h"

/*
  JXCT_Evaporation Example - local example config

  Sensor:
  - RS485 Evaporation Capacity / Evaporation Sensor
  - Register 0x0006 = evaporation-related value, raw sensor units
  - Register 0x0102 = tare / clear command, write 0x0001
  - Address register 0x0100, baud register 0x0101

  The manual naming varies between evaporation height and water weight.
  Keep EVAPORATION_SCALE_DIVISOR at 1.0 until your exact unit is confirmed.
*/

#define SENSOR_ID                   "jxct_evaporation_00"
#define SENSOR_ADDRESS              0x01
#define SENSOR_DEBUG                true

#define EVAPORATION_SCALE_DIVISOR   1.0
#define EVAPORATION_MAX_VALUE       200.0

#define DO_SCAN                     false
#define ADDRESS_CHANGE_AT_BOOT      false
#define ADDRESS_CHANGE_NEW_ADDRESS  0x59
// #define ADDRESS_CHANGE_BUTTON_PIN PCB_SERVICE_BUTTON_PIN
#define ADDRESS_CHANGE_CALL_DELAY_MS 5000UL

// If true, setup() writes 0x0001 to register 0x0102 once.
#define TARE_AT_BOOT                false

#define POLL_INTERVAL_MS            2000
