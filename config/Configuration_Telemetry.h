#pragma once

/*
  Configuration_Telemetry.h

  Controls station read/upload scheduling.

  The station sends one MQTT JSON document per upload cycle.
*/

#ifndef UPLOAD_RATE_MIN
#define UPLOAD_RATE_MIN                     10
#endif

#ifndef TELEMETRY_CYCLE_INTERVAL_MS
#define TELEMETRY_CYCLE_INTERVAL_MS         ((uint32_t)UPLOAD_RATE_MIN * 60000UL)
#endif

#ifndef TELEMETRY_ENABLE_JSON_PAYLOAD
#define TELEMETRY_ENABLE_JSON_PAYLOAD       true
#endif

#ifndef TELEMETRY_PRINT_PAYLOADS
#define TELEMETRY_PRINT_PAYLOADS            true
#endif

// Battery monitor is disabled until the ESP32-S3 PCB divider pin is confirmed.
#define BATTERY_MONITOR_ENABLED              false
#define BATTERY_ADC_PIN                      -1
#define BATTERY_ADC_ATTENUATION_DB           11
#define BATTERY_DIVIDER_R1_OHMS              13200.0
#define BATTERY_DIVIDER_R2_OHMS              3300.0
#define BATTERY_DIODE_DROP_V                 0.0
