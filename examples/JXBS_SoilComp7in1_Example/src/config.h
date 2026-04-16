#pragma once
#include <Arduino.h>
#include "../../../config/Configuration_System.h"
#include "../../../config/Configuration_PCB.h"

/*
  JXBS_SoilComp7in1 Example - local example config
*/

#define SENSOR_ID         "jxbs_soil7_00"
#define SENSOR_ADDRESS    0x01
#define SENSOR_DEBUG      false

#define DO_SCAN           false

// If true, setup() calls changeAddress() once near boot.
// JXBS address change writes register 0x0100. Keep only the target sensor
// connected while changing addresses.
#define ADDRESS_CHANGE_AT_BOOT false
// Target Modbus address written when ADDRESS_CHANGE_AT_BOOT is true.
#define ADDRESS_CHANGE_NEW_ADDRESS 0x02
// Optional safety gate. Uncomment to wait up to 5 seconds for this button.
// If this macro is undefined, no button GPIO is configured or read.
// #define ADDRESS_CHANGE_BUTTON_PIN PCB_SERVICE_BUTTON_PIN

#define POLL_INTERVAL_MS  2000
