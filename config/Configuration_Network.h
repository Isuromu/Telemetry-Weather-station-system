#pragma once

/*
  Configuration_Network.h

  Wi-Fi, time, and MQTT settings for the primary station firmware.
  Keep deployment secrets in this file or inject them with build flags.
*/

#define WIFI_ENABLED                         true
#define WIFI_SSID                            "CHANGE_ME_WIFI"
#define WIFI_PASSWORD                        "CHANGE_ME_PASSWORD"
#define WIFI_CONNECT_TIMEOUT_MS              20000UL

// NTP updates ESP32 system time in UTC. The firmware sends both Unix time
// and a human-readable UTC date-time string.
#define TIME_SYNC_ENABLED                    true
#define TIME_NTP_SERVER_1                    "pool.ntp.org"
#define TIME_NTP_SERVER_2                    "time.google.com"
#define TIME_SYNC_INTERVAL_MS                86400000UL
#define TIME_SYNC_TIMEOUT_MS                 15000UL

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

#ifndef MQTT_JSON_TOPIC
#define MQTT_JSON_TOPIC                      MQTT_STATION_TOPIC
#endif

#ifndef MQTT_STRING_TOPIC
#define MQTT_STRING_TOPIC                    MQTT_STATION_TOPIC
#endif
