#pragma once

/*
  Configuration_Network.h

  Wi-Fi, time, and MQTT settings for the primary station firmware.
  Keep deployment secrets in this file or inject them with build flags.
*/

#define WIFI_ENABLED                         true
#define WIFI_SSID                            "Galaxy A73"
#define WIFI_PASSWORD                        "qlup4530"
#define WIFI_CONNECT_TIMEOUT_MS              20000UL

// Optional transistor/rail that powers the external network device.
// During phone-hotspot testing keep this disabled. When the real router/modem
// pin is confirmed, set NETWORK_POWER_CONTROL_ENABLED true and put the GPIO in
// NETWORK_POWER_PIN.
#ifndef NETWORK_POWER_CONTROL_ENABLED
#define NETWORK_POWER_CONTROL_ENABLED        false
#endif

#ifndef NETWORK_POWER_PIN
#define NETWORK_POWER_PIN                    -1
#endif

#ifndef NETWORK_POWER_ACTIVE_HIGH
#define NETWORK_POWER_ACTIVE_HIGH            true
#endif

#ifndef NETWORK_POWER_WARMUP_MS
#define NETWORK_POWER_WARMUP_MS              0UL
#endif

// NTP updates ESP32 system time in UTC. The firmware sends both Unix time
// and a human-readable UTC date-time string.
#define TIME_SYNC_ENABLED                    true
#define TIME_NTP_SERVER_1                    "pool.ntp.org"
#define TIME_NTP_SERVER_2                    "time.google.com"
#define TIME_SYNC_INTERVAL_MS                86400000UL
#define TIME_SYNC_TIMEOUT_MS                 15000UL

// DS3231 is the station wall clock. The ESP32 reads it after every boot/wake.
// ESP32 wakes itself with a deep-sleep timer; DS3231 alarms are not used.
#ifndef RTC_DS3231_ENABLED
#define RTC_DS3231_ENABLED                   true
#endif

#ifndef RTC_NETWORK_SYNC_INTERVAL_MS
#define RTC_NETWORK_SYNC_INTERVAL_MS         TIME_SYNC_INTERVAL_MS
#endif

#ifndef SLEEP_AFTER_CYCLE_ENABLED
#define SLEEP_AFTER_CYCLE_ENABLED            true
#endif

#ifndef SLEEP_MIN_SECONDS
#define SLEEP_MIN_SECONDS                    15UL
#endif

#ifndef SLEEP_MAX_SECONDS
#define SLEEP_MAX_SECONDS                    86400UL
#endif

#ifndef MQTT_ENABLED
#define MQTT_ENABLED                         true
#endif

// PubSubClient expects only the host name, without the mqtt:// scheme.
#ifndef MQTT_HOST
#define MQTT_HOST                            "broker.emqx.io"
#endif

#ifndef MQTT_PORT
#define MQTT_PORT                            1883
#endif

#ifndef MQTT_CLIENT_ID
#define MQTT_CLIENT_ID                       "mqttx_cdddf63c"
#endif

// Three real stations must not connect with the exact same MQTT client ID at
// the same time. The station profiles use this value as a base and add suffixes.

#ifndef MQTT_USERNAME
#define MQTT_USERNAME                        "pmqtt"
#endif

#ifndef MQTT_PASSWORD
#define MQTT_PASSWORD                        "pmqtt"
#endif

#ifndef MQTT_CONNECT_TIMEOUT_MS
#define MQTT_CONNECT_TIMEOUT_MS              10000UL
#endif

#ifndef MQTT_BUFFER_SIZE
#define MQTT_BUFFER_SIZE                     4096
#endif

#ifndef MQTT_JSON_TOPIC
#define MQTT_JSON_TOPIC                      MQTT_STATION_TOPIC
#endif

// ============================================================
// HTTP OTA
// ============================================================
/*
  The station checks OTA after each successful MQTT upload.

  Version endpoint:
    - plain text: 1.0.1
    - or small JSON: {"version":"1.0.1","firmware_url":"http://server/firmware.bin"}

  If firmware_url is not provided by the version endpoint, OTA_FIRMWARE_URL is
  used. After applying a firmware image the station reboots, then confirms the
  pending version to OTA_CONFIRM_URL on the next boot.
*/

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION                     "1.0.0"
#endif

#ifndef OTA_UPDATE_ENABLED
#define OTA_UPDATE_ENABLED                   true
#endif

#ifndef OTA_VERSION_URL
#define OTA_VERSION_URL                      ""
#endif

#ifndef OTA_FIRMWARE_URL
#define OTA_FIRMWARE_URL                     ""
#endif

#ifndef OTA_CONFIRM_URL
#define OTA_CONFIRM_URL                      ""
#endif

#ifndef OTA_HTTP_TIMEOUT_MS
#define OTA_HTTP_TIMEOUT_MS                  20000UL
#endif
