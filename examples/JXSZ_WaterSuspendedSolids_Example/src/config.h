#pragma once
#include <Arduino.h>
#include "../../../config/Configuration_System.h"
#include "../../../config/Configuration_PCB.h"
#include "../../../config/Configuration_ModbusAddresses.h"

/*
  JXSZ_WaterSuspendedSolids Example - local example config

  Sensor:
  - JXSZ-1001-SS water suspended solids sensor
  - RS485 Modbus RTU

  Manual protocol notes:
  - Source: docs/datasheets/JXSZ-1001-Water Suspended.pdf
  - Default serial format: 9600 baud, 8 data bits, no parity, 1 stop bit.
  - Register 0x0001 = water temperature, signed raw / 10 C.
  - Register 0x0002 = suspended solids raw / scale divisor, mg/L.
  - Register 0x0100 = device address.

  Scaling note:
  - The manual table text and worked example disagree.
  - The worked example maps raw 189 to 18.9, so this config uses /10.
  - If your unit should report 0.01 mg/L resolution, change
    SUSPENDED_SOLIDS_SCALE_DIVISOR to 100.0.

  Handling and measurement notes:
  - Keep the optical/measuring window clean and avoid scratching it.
  - Fully immerse the sensing section in the water sample.
  - Avoid bubbles, sediment stuck on the window, and direct contact with tank walls.
  - Wait for the value to stabilize before recording the measurement.
  - Rinse with clean water after use and store clean, dry, and protected.
*/

#define SENSOR_ID                           "jxsz_water_suspended_solids_00"
#define SENSOR_ADDRESS                      ADDR_WATER_SUSPENDED_SOLIDS_00
#define SENSOR_DEBUG                        true

// Default from the manual worked example: raw 189 -> 18.9 mg/L.
#define SUSPENDED_SOLIDS_SCALE_DIVISOR      10.0

// Manual range note: 0..20000 mg/L. Adjust if your exact probe range differs.
#define SUSPENDED_SOLIDS_MAX_MG_L           20000.0

// Scan address range on boot and print first responsive sensor.
#define DO_SCAN                             false

// If true, setup() calls changeAddress() once near boot.
// JXSZ address change writes register 0x0100. Keep only the target sensor
// connected while changing addresses. No broadcast address is documented in
// the available protocol note, so assume you must know the current address.
#define ADDRESS_CHANGE_AT_BOOT              false
// Target Modbus address written when ADDRESS_CHANGE_AT_BOOT is true.
#define ADDRESS_CHANGE_NEW_ADDRESS          ADDR_WATER_SUSPENDED_SOLIDS_00
// Optional safety gate. Uncomment to wait up to 5 seconds for this button.
// If this macro is undefined, no button GPIO is configured or read.
// #define ADDRESS_CHANGE_BUTTON_PIN PCB_SERVICE_BUTTON_PIN
// Delay before the actual persistent address write. Gives time to open Serial
// Monitor after reset/upload and see the changeAddress() call.
#define ADDRESS_CHANGE_CALL_DELAY_MS        5000UL

// Optional single-register reads in loop, after the combined read.
#define READ_TEMPERATURE_ONLY_IN_LOOP       false
#define READ_SUSPENDED_SOLIDS_ONLY_IN_LOOP  false

// Delay between loop() polling cycles.
#define POLL_INTERVAL_MS                    2000
