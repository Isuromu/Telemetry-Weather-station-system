#pragma once

/*
  Telemetry station profile: soil and weather/air-related sensors.

  MQTT:
  - host/user/password are configured in Configuration_Network.h.
  - this station publishes JSON to topic-2.

  Sensor set:
  - several soil sensors
  - leaf surface sensor
  - wind speed and wind direction
  - UV, PAR, total solar radiation
  - evaporation
*/

#define STATION_ID                          "telemetry-soil-air-1"
#define STATION_NAME                        "Telemetry Soil Air Station"
#define MQTT_STATION_TOPIC                  "topic-2"
#define MQTT_CLIENT_ID                      "mqttx_cdddf63c_soil_air"

#define JXBS_SOIL7IN1_COUNT                 3
#define JXBS_LEAF_SURFACE_HUMIDITY_COUNT    1
#define JXCT_WIND_SPEED_COUNT               1
#define JXCT_WIND_DIRECTION_COUNT           1
#define JXCT_UV_RAYS_COUNT                  1
#define JXCT_PAR_COUNT                      1
#define JXCT_TOTAL_SOLAR_RADIATION_COUNT    1
#define JXCT_EVAPORATION_COUNT              1

#define UPLOAD_RATE_MIN                     10
