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

#define NEW_ADDRESS       0x02
#define DO_SCAN           false
#define TRY_ADDRESS_CHANGE_AT_BOOT false
#define POLL_INTERVAL_MS  2000