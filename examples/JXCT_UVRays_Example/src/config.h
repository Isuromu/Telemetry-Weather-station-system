#pragma once
#include <Arduino.h>
#include "../../../config/Configuration_System.h"
#include "../../../config/Configuration_PCB.h"

/*
  JXCT_UVRays Example - local example config

  Sensor:
  - RS485 UV Rays Sensor
  - Humidity register 0x0000, raw / 10 %RH
  - Temperature register 0x0001, signed raw / 10 C
  - UV register 0x0008, raw / 10 W/m2
  - Address register 0x0100, baud register 0x0101
*/

#define SENSOR_ID                   "jxct_uv_rays_00"
#define SENSOR_ADDRESS              0x01
#define SENSOR_DEBUG                true

#define UV_MAX_W_M2                 150.0

#define DO_SCAN                     false
#define ADDRESS_CHANGE_AT_BOOT      false
#define ADDRESS_CHANGE_NEW_ADDRESS  0x56
// #define ADDRESS_CHANGE_BUTTON_PIN PCB_SERVICE_BUTTON_PIN
#define ADDRESS_CHANGE_CALL_DELAY_MS 5000UL

#define POLL_INTERVAL_MS            2000
