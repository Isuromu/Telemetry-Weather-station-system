#pragma once
#include <Arduino.h>

/*
  Configuration_PCB.h

  Selects the hardware profile used by the firmware.

  Default for this project:
    PCB_TELEMETRY_ESP32S3_V2

  Important difference from Amudario Firmware:
    Amudario ESP32-S3 V2 RS485: RX=17, TX=18
    Telemetry ESP32-S3 V2 RS485: RX=18, TX=17
*/

#if defined(TELEMETRY_SELECTED_PCB_TELEMETRY_ESP32S3_V2) && \
    defined(TELEMETRY_SELECTED_PCB_AMUDARIO_ESP32S3_V2)
#error "Select only one PCB profile."
#endif

#if defined(TELEMETRY_SELECTED_PCB_AMUDARIO_ESP32S3_V2)
#include "../pcb/PCB_ESP32S3_V2.h"
#else
#include "../pcb/PCB_TELEMETRY_ESP32S3_V2.h"
#endif

// Service logs always use UART0. Native USB CDC is not used for station logs.
#ifndef PCB_DEBUG_SERIAL_PORT
#define PCB_DEBUG_SERIAL_PORT Serial0
#endif
