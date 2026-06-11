#pragma once
#include <Arduino.h>
#include "../../../config/Configuration_System.h"
#include "../../../config/Configuration_PCB.h"
#include "../../../config/Configuration_ModbusAddresses.h"

/*
  JXBS_GasO3CONH3Shield Example - local example config

  Shield label register map:
  - CO  register: 0x0006
  - O3  register: 0x0007
  - NH3 register: 0x0008

  Field validation:
  - CO at 0x0006 was confirmed by a smoke/burnt-paper test.
  - O3/NH3 are accepted from the shield label because CO confirmed the map.
  - 0x000E can correlate with smoke, but it is not the direct CO ppm register.

  This shield does not expose RH/temperature values. Do not apply standalone
  single-gas transmitter layouts to this board.
*/

#define SENSOR_ID                   "jxbs_gas_o3_co_nh3_shield_00"
#define SENSOR_ADDRESS              ADDR_GAS_O3_CO_NH3_SHIELD_00
#define SENSOR_DEBUG                true

#define CO_MAX_PPM                  2000.0
#define O3_MAX_PPM                  100.0
#define NH3_MAX_PPM                 5000.0

// Diagnostic: read and print raw shield gas registers 0x0006..0x0008 every poll.
#define DUMP_SHIELD_GAS_REGISTERS   true

#define DO_SCAN                     false
#define ADDRESS_CHANGE_AT_BOOT      false
#define ADDRESS_CHANGE_NEW_ADDRESS  ADDR_GAS_O3_CO_NH3_SHIELD_00
// #define ADDRESS_CHANGE_BUTTON_PIN PCB_SERVICE_BUTTON_PIN
#define ADDRESS_CHANGE_CALL_DELAY_MS 5000UL

#define POLL_INTERVAL_MS            2000
