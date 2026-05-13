#pragma once
#include <Arduino.h>
#include "../../../config/Configuration_System.h"
#include "../../../config/Configuration_PCB.h"
#include "../../../config/Configuration_ModbusAddresses.h"

/*
  JXCT_WindDirection Example - local example config

  Sensor:
  - RS485 Wind Direction Sensor
  - Register 0x0000 = wind direction, raw degrees, 0..360
  - Address register 0x0100, baud register 0x0101
*/

#define SENSOR_ID                   "jxct_wind_direction_00"
#define SENSOR_ADDRESS              0x01
#define SENSOR_DEBUG                true

#define DO_SCAN                     false
#define ADDRESS_CHANGE_AT_BOOT      false
#define ADDRESS_CHANGE_NEW_ADDRESS  ADDR_WIND_DIRECTION_00
// #define ADDRESS_CHANGE_BUTTON_PIN PCB_SERVICE_BUTTON_PIN
#define ADDRESS_CHANGE_CALL_DELAY_MS 5000UL

#define POLL_INTERVAL_MS            2000
