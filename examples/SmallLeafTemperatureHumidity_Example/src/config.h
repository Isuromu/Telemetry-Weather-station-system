#pragma once
#include <Arduino.h>
#include "../../../config/Configuration_System.h"
#include "../../../config/Configuration_PCB.h"
#include "../../../config/Configuration_ModbusAddresses.h"

/*
  SmallLeafTemperatureHumidity Example - local example config
*/

#define SENSOR_ID                   "small_leaf_00"
// Current Modbus node address. Factory/default units commonly answer at 0x01.
#define SENSOR_ADDRESS              0x01
#define SENSOR_DEBUG                true

// Scan address range on boot and print first responsive sensor.
#define DO_SCAN                     true

// If true, setup() calls changeAddress() once near boot.
// Address change writes register 0x0030. Keep only this one sensor connected.
#define ADDRESS_CHANGE_AT_BOOT      true
// Target station address for this small leaf sensor.
#define ADDRESS_CHANGE_NEW_ADDRESS  ADDR_SMALL_LEAF_TEMP_HUMIDITY_00
// #define ADDRESS_CHANGE_BUTTON_PIN PCB_SERVICE_BUTTON_PIN
#define ADDRESS_CHANGE_CALL_DELAY_MS 5000UL

// Delay between loop() polling cycles.
#define POLL_INTERVAL_MS            2000
