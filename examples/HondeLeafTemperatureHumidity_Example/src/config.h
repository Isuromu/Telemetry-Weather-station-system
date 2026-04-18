#pragma once
#include <Arduino.h>
#include "../../../config/Configuration_System.h"
#include "../../../config/Configuration_PCB.h"

/*
  HondeLeafTemperatureHumidity Example - local example config

  Scope:
  - This file only affects examples/HondeLeafTemperatureHumidity_Example.
  - RS485 transport defaults and board pin mappings come from the shared
    Configuration_System.h and Configuration_PCB.h files.

  Manual protocol notes:
  - Model family: RD-LTH-O-01 leaf temperature and humidity sensor.
  - Default serial format: 9600 baud, 8 data bits, no parity, 1 stop bit.
  - Default Modbus address is 0x01.
  - For address-change only, the datasheet allows broadcast address 0xFE.
    If the current address is unknown, set SENSOR_ADDRESS to 0xFE, keep only
    this one sensor on the RS485 bus, enable ADDRESS_CHANGE_AT_BOOT, then write
    the wanted ADDRESS_CHANGE_NEW_ADDRESS.
*/

#define SENSOR_ID                   "honde_leaf_00"
#define SENSOR_ADDRESS              0x02
#define SENSOR_DEBUG                true

// Scan address range on boot and print first responsive sensor.
#define DO_SCAN                     false

// If true, setup() calls changeAddress() once near boot.
// Honde leaf address change writes register 0x0030. Keep only the target sensor
// connected while changing addresses. The sensor replies from the new address,
// so the driver validates the response against ADDRESS_CHANGE_NEW_ADDRESS.
#define ADDRESS_CHANGE_AT_BOOT      false
// Target Modbus address written when ADDRESS_CHANGE_AT_BOOT is true.
#define ADDRESS_CHANGE_NEW_ADDRESS  0x02
// Optional safety gate. Uncomment to wait up to 5 seconds for this button.
// If this macro is undefined, no button GPIO is configured or read.
// #define ADDRESS_CHANGE_BUTTON_PIN PCB_SERVICE_BUTTON_PIN
// Delay before the actual persistent address write. Gives time to open Serial
// Monitor after reset/upload and see the changeAddress() call.
#define ADDRESS_CHANGE_CALL_DELAY_MS 5000UL

// Delay between loop() polling cycles.
#define POLL_INTERVAL_MS            2000
