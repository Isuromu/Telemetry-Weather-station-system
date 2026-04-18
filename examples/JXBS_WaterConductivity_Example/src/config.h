#pragma once
#include <Arduino.h>
#include "../../../config/Configuration_System.h"
#include "../../../config/Configuration_PCB.h"

/*
  JXBS Water Conductivity Example - local example config

  This example targets the RS485 transmitter/controller assembly used with a
  JXEC-T conductivity probe. A bare metal probe without the transmitter box
  will not respond as a Modbus slave.
*/

#define SENSOR_ID                   "water_conductivity_00"
#define SENSOR_ADDRESS              0x01
#define SENSOR_DEBUG                true

// K=1 examples from the vendor documentation decode conductivity as raw / 100.
// Change this if your controller/probe range uses another scaling.
#define CONDUCTIVITY_SCALE_DIVISOR  100.0

// Conservative upper bound. Adjust to your probe range/cell constant.
#define CONDUCTIVITY_MAX_US_CM      200000.0

// Scan address range on boot and print first responsive sensor.
#define DO_SCAN                     false

// If true, setup() calls changeAddress() once near boot.
// Keep only the target controller connected while changing addresses.
#define ADDRESS_CHANGE_AT_BOOT      true
#define ADDRESS_CHANGE_NEW_ADDRESS  0x51
// #define ADDRESS_CHANGE_BUTTON_PIN PCB_SERVICE_BUTTON_PIN
// #define ADDRESS_CHANGE_CALL_DELAY_MS 5000UL

// Optional single-register reads in loop, after the combined read.
#define READ_TEMPERATURE_ONLY_IN_LOOP    false
#define READ_CONDUCTIVITY_ONLY_IN_LOOP   false

#define POLL_INTERVAL_MS            2000
