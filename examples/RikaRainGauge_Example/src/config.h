#pragma once
#include <Arduino.h>
#include "../../../config/Configuration_System.h"
#include "../../../config/Configuration_PCB.h"

/*
  RikaRainGauge Example - local example config

  Counting modes:
  - RAIN_GAUGE_COUNTING_MODE_EXTERNAL_COUNTER
  - RAIN_GAUGE_COUNTING_MODE_MCU_INTERRUPT
  - RAIN_GAUGE_COUNTING_MODE_RS485_SENSOR

  Rika pulse factor:
  - 1 pulse = 0.2 mm
*/

#define SENSOR_ID                         "rika_rain_00"
#define SENSOR_ADDRESS                    0x50
#define SENSOR_DEBUG                      false
#define SENSOR_RS485_BAUD                 9600

#define RAIN_COUNTING_MODE                RAIN_GAUGE_COUNTING_MODE_EXTERNAL_COUNTER
#define RAIN_SAMPLE_INTERVAL_MS           60000UL

#define RAIN_PCF8574_ADDRESS              0x20
#define RAIN_COUNTER_RESET_PIN            PCB_RAIN_COUNTER_RESET_PIN
#define RAIN_BYPASS_INTERRUPT_PIN         PCB_RAIN_BYPASS_INTERRUPT_PIN
#define RAIN_INTERRUPT_DEBOUNCE_MS        100

// Used only in RS485 sensor mode.
#define ADDRESS_CHANGE_AT_BOOT            false
#define ADDRESS_CHANGE_NEW_ADDRESS        0x50
// #define ADDRESS_CHANGE_BUTTON_PIN PCB_SERVICE_BUTTON_PIN
// Delay before the actual persistent address write. Gives time to open Serial
// Monitor after reset/upload and see the changeAddress() call.
// #define ADDRESS_CHANGE_CALL_DELAY_MS 5000UL
