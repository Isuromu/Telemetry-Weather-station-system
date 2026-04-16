#pragma once
#include <Arduino.h>
#include "../../../config/Configuration_System.h"
#include "../../../config/Configuration_PCB.h"

/*
  GenericRainGauge Example - local example config

  Counting modes:
  - RAIN_GAUGE_COUNTING_MODE_EXTERNAL_COUNTER:
      hardware debouncer + 4040 binary counter, read P0..P7 through PCF8574.
  - RAIN_GAUGE_COUNTING_MODE_MCU_INTERRUPT:
      MCU counts rain gauge pulses directly from RAIN_BYPASS_INTERRUPT.

  Generic pulse factor:
  - 1 pulse = 0.2794 mm
*/

#define SENSOR_ID                         "generic_rain_00"
#define SENSOR_DEBUG                      false

#define RAIN_COUNTING_MODE                RAIN_GAUGE_COUNTING_MODE_EXTERNAL_COUNTER
#define RAIN_SAMPLE_INTERVAL_MS           60000UL

#define RAIN_PCF8574_ADDRESS              0x20
#define RAIN_COUNTER_RESET_PIN            PCB_RAIN_COUNTER_RESET_PIN
#define RAIN_BYPASS_INTERRUPT_PIN         PCB_RAIN_BYPASS_INTERRUPT_PIN
#define RAIN_INTERRUPT_DEBOUNCE_MS        100
