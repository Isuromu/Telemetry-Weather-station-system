#pragma once
#include <Arduino.h>
#include "../../../config/Configuration_System.h"
#include "../../../config/Configuration_PCB.h"
#include "../../../config/Configuration_ModbusAddresses.h"

/*
  JXBS_PM25PM10Standalone Example - local example config

  Sensor:
  - Standalone RS485 PM2.5/PM10 sensor
  - PM2.5 register: 0x0004, raw ug/m3
  - PM10 register:  0x0009, raw ug/m3

  Do not use this if PM2.5/PM10 are inside the combined air-quality shield.
*/

#define SENSOR_ID                   "jxbs_pm25_pm10_00"
#define SENSOR_ADDRESS              0x01
#define SENSOR_DEBUG                true

#define PM_MAX_UG_M3                300.0

#define DO_SCAN                     false
#define ADDRESS_CHANGE_AT_BOOT      false
#define ADDRESS_CHANGE_NEW_ADDRESS  ADDR_PM25_PM10_00
// #define ADDRESS_CHANGE_BUTTON_PIN PCB_SERVICE_BUTTON_PIN
#define ADDRESS_CHANGE_CALL_DELAY_MS 5000UL

#define POLL_INTERVAL_MS            2000
