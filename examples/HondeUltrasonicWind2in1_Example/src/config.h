#pragma once
#include <Arduino.h>
#include "../../../config/Configuration_System.h"
#include "../../../config/Configuration_PCB.h"

/*
  HondeUltrasonicWind2in1 Example - local example config

  Scope:
  - This file only affects examples/HondeUltrasonicWind2in1_Example.
  - RS485 transport defaults and board pin mappings come from the shared
    Configuration_System.h and Configuration_PCB.h files.

  Manual protocol notes:
  - Default serial format: 9600 baud, 8 data bits, no parity, 1 stop bit.
  - Default Modbus address is commonly 0x01.
  - This example reads only wind direction and wind speed.
*/

#define SENSOR_ID                   "honde_wind2_00"
#define SENSOR_ADDRESS              0x01
#define SENSOR_DEBUG                true 

// Scan address range on boot and print first responsive sensor.
#define DO_SCAN                     false

// If true, setup() calls changeAddress() once near boot.
// This sensor uses the manufacturer's ASCII setup mode, not a Modbus register.
// Keep only the target sensor connected while changing addresses.
#define ADDRESS_CHANGE_AT_BOOT      false
// Target Modbus address written when ADDRESS_CHANGE_AT_BOOT is true.
#define ADDRESS_CHANGE_NEW_ADDRESS  0x02
// Optional safety gate. Uncomment to wait up to 5 seconds for this button.
// If this macro is undefined, no button GPIO is configured or read.
// #define ADDRESS_CHANGE_BUTTON_PIN PCB_SERVICE_BUTTON_PIN

// Delay between loop() polling cycles.
#define POLL_INTERVAL_MS            2000
