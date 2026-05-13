#pragma once
#include <Arduino.h>
#include "../../../config/Configuration_System.h"
#include "../../../config/Configuration_PCB.h"
#include "../../../config/Configuration_ModbusAddresses.h"

/*
  JXCT_TotalSolarRadiation Example - local example config

  Sensor:
  - RS485 Total Solar Radiation Sensor
  - Register 0x0000 = total solar radiation, raw W/m2
  - Address register 0x0100, baud register 0x0101
*/

#define SENSOR_ID                   "jxct_total_solar_radiation_00"
#define SENSOR_ADDRESS              0x01
#define SENSOR_DEBUG                true

#define SOLAR_MAX_W_M2              1500.0

#define DO_SCAN                     false
#define ADDRESS_CHANGE_AT_BOOT      false
#define ADDRESS_CHANGE_NEW_ADDRESS  ADDR_TOTAL_SOLAR_RADIATION_00
// #define ADDRESS_CHANGE_BUTTON_PIN PCB_SERVICE_BUTTON_PIN
#define ADDRESS_CHANGE_CALL_DELAY_MS 5000UL

#define POLL_INTERVAL_MS            2000
