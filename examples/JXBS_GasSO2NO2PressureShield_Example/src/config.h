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

  SO2/NO2 scale note:
  The local SO2/NO2 PDFs are analog-output manuals, not RS485 Modbus manuals.
  They list two probe ranges:
  - 20 ppm probe:   0.01 ppm resolution, use divisor 100 if raw follows resolution units.
  - 2000 ppm probe: 0.1 ppm resolution, use divisor 10 if raw follows resolution units.

  The defaults below select the 2000 ppm variant. Confirm against the physical
  shield label or real RS485 logs before treating the ppm values as final.

  The driver reads measurements in one Modbus request: 0x0006..0x0013.
  The diagnostic dump below reads 0x0000..0x0013 in one Modbus request so the
  raw log contains NO2, SO2, and atmospheric pressure together.
*/

#define SENSOR_ID                   "jxbs_gas_so2_no2_pressure_shield_00"
#define SENSOR_ADDRESS              ADDR_GAS_SO2_NO2_PRESSURE_SHIELD_00
#define SENSOR_DEBUG                false

#define NO2_SCALE_DIVISOR           10.0
#define SO2_SCALE_DIVISOR           10.0
#define NO2_MAX_PPM                 2000.0
#define SO2_MAX_PPM                 2000.0

// Diagnostic: one Modbus request covering gas and pressure registers.
#define READ_SENSOR_REGISTER_RANGE  false
#define REGISTER_DUMP_START         0x0000
#define REGISTER_DUMP_END           0x0013
// Keep false for exactly one Modbus request per loop in this diagnostic example.
#define RUN_DRIVER_READ_AFTER_DUMP  false

#if REGISTER_DUMP_END < REGISTER_DUMP_START
#error "REGISTER_DUMP_END must be >= REGISTER_DUMP_START"
#endif

#define REGISTER_DUMP_COUNT         (REGISTER_DUMP_END - REGISTER_DUMP_START + 1)

#if REGISTER_DUMP_COUNT > 125
#error "REGISTER_DUMP_COUNT must be <= 125 for one Modbus 0x03 request"
#endif

#define DO_SCAN                     false
#define ADDRESS_CHANGE_AT_BOOT      false
#define ADDRESS_CHANGE_NEW_ADDRESS  ADDR_GAS_SO2_NO2_PRESSURE_SHIELD_00
// #define ADDRESS_CHANGE_BUTTON_PIN PCB_SERVICE_BUTTON_PIN
#define ADDRESS_CHANGE_CALL_DELAY_MS 5000UL

#define POLL_INTERVAL_MS            2000
