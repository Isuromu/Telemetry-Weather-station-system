#pragma once
#include <Arduino.h>
#include "../../../config/Configuration_System.h"
#include "../../../config/Configuration_PCB.h"

/*
  JXCT_AirQualityShield Example - local example config

  One RS485 shield / board:
  - Humidity register:    0x0000, raw / 10 %RH
  - Temperature register: 0x0001, signed raw / 10 C
  - PM2.5 register:      0x0004, raw ug/m3
  - TVOC register:       0x0006, raw ppb
  - PM10 register:       0x0009, raw ug/m3

  Source docs:
  - docs/datasheets/RS485-Temperature and Humidity.pdf
  - docs/datasheets/RS485 TVOC.pdf
  - docs/datasheets/JXBS-3001-PM2.5 10 -RS485 - Shutter Box Type.docx
*/

#define SENSOR_ID                   "jxct_air_quality_shield_00"
#define SENSOR_ADDRESS              0x01
#define SENSOR_DEBUG                true

#define AIR_PM_MAX_UG_M3            300.0
#define AIR_TVOC_MAX_PPB            1000.0

#define DO_SCAN                     false
#define ADDRESS_CHANGE_AT_BOOT      false
#define ADDRESS_CHANGE_NEW_ADDRESS  0x53
// #define ADDRESS_CHANGE_BUTTON_PIN PCB_SERVICE_BUTTON_PIN
#define ADDRESS_CHANGE_CALL_DELAY_MS 5000UL

#define POLL_INTERVAL_MS            2000
