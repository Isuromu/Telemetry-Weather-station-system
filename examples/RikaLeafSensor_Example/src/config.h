#pragma once
#include <Arduino.h>
#include "../../../config/Configuration_System.h"
#include "../../../config/Configuration_PCB.h"

/*
  Rika Leaf Sensor Example - local example config

  Scope:
  - This file only affects examples/RikaLeafSensor_Example.
  - Global defaults (timeouts/retries/board pins) still come from:
      Configuration_System.h
      Configuration_PCB.h

  Usage:
  - Update SENSOR_ADDRESS to the actual Modbus node address in your setup.
  - Set DO_SCAN=true during first bring-up if the sensor address is unknown.
  - Keep SENSOR_DEBUG=false unless troubleshooting protocol frames.
  - Normal reading wiring: white wire to V- / GND.
  - Address-change wiring: white wire to V+ before boot/address write.
*/

// Logical ID string printed in diagnostics/log output.
#define SENSOR_ID         "leaf_test_00"
// Current Modbus node address of the connected leaf sensor.
#define SENSOR_ADDRESS    0x80
// Enables driver/bus debug traces when PrintController allows output.
#define SENSOR_DEBUG      true

// If true, setup() scans addresses 1..247 and prints first responder.
#define DO_SCAN           false

// If true, setup() calls changeAddress() once near boot.
// Rika leaf address change uses register 0x0200 and address 0xFF internally.
// Hardware condition: connect the sensor white wire to V+ for address change.
// Restore the white wire to V- / GND for normal reads after programming.
// Keep only the target sensor connected while changing addresses.
#define ADDRESS_CHANGE_AT_BOOT false
// Target Modbus address written when ADDRESS_CHANGE_AT_BOOT is true.
#define ADDRESS_CHANGE_NEW_ADDRESS 0x01
// Optional safety gate. Uncomment to wait up to 5 seconds for this button.
// If this macro is undefined, no button GPIO is configured or read.
// #define ADDRESS_CHANGE_BUTTON_PIN PCB_SERVICE_BUTTON_PIN

// Delay between loop() polling cycles.
#define POLL_INTERVAL_MS  2000
