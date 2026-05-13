#pragma once
#include <Arduino.h>
#include "../../../config/Configuration_System.h"
#include "../../../config/Configuration_PCB.h"
#include "../../../config/Configuration_ModbusAddresses.h"

/*
  JXBS_GasSO2NO2PressureShield Example - local example config

  One gas + pressure shield:
  - NO2 register: 0x0006
  - SO2 register: 0x0007
  - Atmospheric pressure registers: 0x0012 high word, 0x0013 low word
  - Pressure scale: u32 raw / 100 mbar

  Accuracy note:
  Defaults use raw / 10 ppm for NO2 and SO2, which is a common scale for these gases.
  Check that they match the broader JXCT/JXBS gas-family convention.
*/

#define SENSOR_ID                   "jxbs_gas_so2_no2_pressure_shield_00"
#define SENSOR_ADDRESS              0x01
#define SENSOR_DEBUG                true

#define NO2_SCALE_DIVISOR           10.0
#define SO2_SCALE_DIVISOR           10.0
#define NO2_MAX_PPM                 2000.0
#define SO2_MAX_PPM                 2000.0

#define DO_SCAN                     false
#define ADDRESS_CHANGE_AT_BOOT      false
#define ADDRESS_CHANGE_NEW_ADDRESS  ADDR_GAS_SO2_NO2_PRESSURE_SHIELD_00
// #define ADDRESS_CHANGE_BUTTON_PIN PCB_SERVICE_BUTTON_PIN
#define ADDRESS_CHANGE_CALL_DELAY_MS 5000UL

#define POLL_INTERVAL_MS            2000
