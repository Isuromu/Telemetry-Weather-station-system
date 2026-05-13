#pragma once

/*
  Telemetry station profile: water sensors.

  MQTT:
  - host/user/password are configured in Configuration_Network.h.
  - this station publishes JSON to topic-1.

  Sensor set:
  - water pH
  - water conductivity
  - water suspended solids
*/

#define STATION_ID                          "telemetry-water-1"
#define STATION_NAME                        "Telemetry Water Station"
#define MQTT_STATION_TOPIC                  "topic-1"
#define MQTT_CLIENT_ID                      "mqttx_cdddf63c_water"

#define JXBS_WATER_PH_COUNT                 1
#define JXBS_WATER_CONDUCTIVITY_COUNT       1
#define JXSZ_WATER_SUSPENDED_SOLIDS_COUNT   1

#define UPLOAD_RATE_MIN                     10
