#pragma once

/*
  Telemetry station profile: gas and pollution sensors.

  MQTT:
  - host/user/password are configured in Configuration_Network.h.
  - this station publishes JSON to topic-3.

  Sensor set:
  - air quality shield: air T/H, PM2.5, PM10, TVOC
  - CO/O3/NH3 gas shield
  - SO2/NO2/pressure gas shield
*/

#define STATION_ID                          "telemetry-gas-1"
#define STATION_NAME                        "Telemetry Gas Pollution Station"
#define MQTT_STATION_TOPIC                  "topic-3"
#define MQTT_CLIENT_ID                      "mqttx_cdddf63c_gas"

#define JXCT_AIR_QUALITY_SHIELD_COUNT       1
#define JXBS_GAS_O3_CO_NH3_SHIELD_COUNT     1
#define JXBS_GAS_SO2_NO2_PRESSURE_SHIELD_COUNT 1

#define UPLOAD_RATE_MIN                     10
