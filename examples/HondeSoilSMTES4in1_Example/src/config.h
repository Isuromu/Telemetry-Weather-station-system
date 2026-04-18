#pragma once
#include <Arduino.h>
#include "../../../config/Configuration_System.h"
#include "../../../config/Configuration_PCB.h"

/*
  HondeSoilSMTES4in1 Example - local example config

  Scope:
  - This file only affects examples/HondeSoilSMTES4in1_Example.
  - RS485 transport defaults and board pin mappings come from the shared
    Configuration_System.h and Configuration_PCB.h files.

  Manual protocol notes:
  - Model family: RD-SMTES soil moisture/temperature/EC/salinity 4-in-1.
  - Default serial format: 9600 baud, 8 data bits, no parity, 1 stop bit.
  - Manual examples use Modbus address 0x01; address 0xFE is the universal
    query address when the current sensor address is unknown.
*/

#define SENSOR_ID                   "honde_soil4_00"
#define SENSOR_ADDRESS              0x01
#define SENSOR_DEBUG                false

// Scan address range on boot and print first responsive sensor.
#define DO_SCAN                     false

// If true, setup() calls changeAddress() once near boot.
// Honde RD-SMTES address change writes register 0x0200. Keep only the target
// sensor connected while changing addresses.
#define ADDRESS_CHANGE_AT_BOOT      false
// Target Modbus address written when ADDRESS_CHANGE_AT_BOOT is true.
#define ADDRESS_CHANGE_NEW_ADDRESS  0x02
// Optional safety gate. Uncomment to wait up to 5 seconds for this button.
// If this macro is undefined, no button GPIO is configured or read.
// #define ADDRESS_CHANGE_BUTTON_PIN PCB_SERVICE_BUTTON_PIN
// Delay before the actual persistent address write. Gives time to open Serial
// Monitor after reset/upload and see the changeAddress() call.
// #define ADDRESS_CHANGE_CALL_DELAY_MS 5000UL

// Delay between loop() polling cycles.
#define POLL_INTERVAL_MS            2000
