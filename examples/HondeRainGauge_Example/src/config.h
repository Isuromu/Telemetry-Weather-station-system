#pragma once
#include <Arduino.h>
#include "../../../config/Configuration_System.h"
#include "../../../config/Configuration_PCB.h"

/*
  HondeRainGauge Example - local example config

  Counting modes:
  - RAIN_GAUGE_COUNTING_MODE_EXTERNAL_COUNTER
  - RAIN_GAUGE_COUNTING_MODE_MCU_INTERRUPT
  - RAIN_GAUGE_COUNTING_MODE_RS485_SENSOR

  Honde pulse factor:
  - 1 pulse = 0.1 mm
*/

#define SENSOR_ID                         "honde_rain_00"
#define SENSOR_ADDRESS                    0x01
#define SENSOR_DEBUG                      false
// Honde manual supports 2400/4800/9600. Factory default is often 4800.
#define SENSOR_RS485_BAUD                 4800

#define RAIN_COUNTING_MODE                RAIN_GAUGE_COUNTING_MODE_EXTERNAL_COUNTER
#define RAIN_SAMPLE_INTERVAL_MS           60000UL

#define RAIN_PCF8574_ADDRESS              0x20
#define RAIN_COUNTER_RESET_PIN            PCB_RAIN_COUNTER_RESET_PIN
#define RAIN_BYPASS_INTERRUPT_PIN         PCB_RAIN_BYPASS_INTERRUPT_PIN
#define RAIN_INTERRUPT_DEBOUNCE_MS        100

// Used only in RS485 sensor mode.
#define ADDRESS_CHANGE_AT_BOOT            false
#define ADDRESS_CHANGE_NEW_ADDRESS        0x02
// #define ADDRESS_CHANGE_BUTTON_PIN PCB_SERVICE_BUTTON_PIN
// Delay before the actual persistent address write. Gives time to open Serial
// Monitor after reset/upload and see the changeAddress() call.
// #define ADDRESS_CHANGE_CALL_DELAY_MS 5000UL
