#pragma once
#include <Arduino.h>
#include "../../../config/Configuration_System.h"
#include "../../../config/Configuration_PCB.h"
#include "../../../config/Configuration_ModbusAddresses.h"

/*
  JXBS_GasO3CONH3Shield Example - local example config

  One gas shield:
  - CO register:  0x0006, raw / 10 ppm
  - O3 register:  0x0007, raw / 100 ppm
  - NH3 register: 0x0008, raw / 10 ppm

  These are shield registers from the station wiring map. Standalone O3/NH3
  manuals may show gas concentration at 0x0006; do not use those standalone
  offsets for this combined shield.
*/

#define SENSOR_ID                   "jxbs_gas_o3_co_nh3_shield_00"
#define SENSOR_ADDRESS              0x01
#define SENSOR_DEBUG                true

#define CO_MAX_PPM                  2000.0
#define O3_MAX_PPM                  100.0
#define NH3_MAX_PPM                 5000.0

#define DO_SCAN                     true
#define ADDRESS_CHANGE_AT_BOOT      false
#define ADDRESS_CHANGE_NEW_ADDRESS  ADDR_GAS_O3_CO_NH3_SHIELD_00
// #define ADDRESS_CHANGE_BUTTON_PIN PCB_SERVICE_BUTTON_PIN
#define ADDRESS_CHANGE_CALL_DELAY_MS 5000UL

#define POLL_INTERVAL_MS            2000
