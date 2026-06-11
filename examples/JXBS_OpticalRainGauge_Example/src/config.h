#pragma once
#include <Arduino.h>
#include "../../../config/Configuration_System.h"
#include "../../../config/Configuration_PCB.h"
#include "../../../config/Configuration_ModbusAddresses.h"

/*
  JXBS_OpticalRainGauge Example - local example config

  Sensor:
  - RS485 optical rain gauge
  - Rainfall accumulated register: 0x0003, raw / 10 mm
  - Separate pulse lead: optional 0-5V pulse output. Official JXBS-3001-GXYL
    wiring tables list this as the white wire; some delivered harnesses may
    expose it as a separate red pigtail. Do not connect this lead to RS485 A/B
    or to the 12-24V supply unless the exact unit label/manual says otherwise.
  - Clear accumulated rainfall candidates:
      0x0105 write 0
      0x0101 write 0, if 0x0105 fails on the delivered unit
*/

#define SENSOR_ID                   "jxbs_optical_rain_00"
#define SENSOR_ADDRESS              0x01
#define SENSOR_DEBUG                true

#define RAIN_MAX_MM                 10000.0

#define DO_SCAN                     false
#define ADDRESS_CHANGE_AT_BOOT      false
#define ADDRESS_CHANGE_NEW_ADDRESS  ADDR_OPTICAL_RAIN_00
// #define ADDRESS_CHANGE_BUTTON_PIN PCB_SERVICE_BUTTON_PIN
#define ADDRESS_CHANGE_CALL_DELAY_MS 5000UL

#define CLEAR_RAIN_AT_BOOT          false
#define CLEAR_RAIN_REGISTER         0x0105

#define POLL_INTERVAL_MS            2000
