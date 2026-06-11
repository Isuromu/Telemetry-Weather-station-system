#pragma once

/*
  Telemetry station profile: gas and pollution sensors.

  MQTT:
  - host/user/password are configured in Configuration_Network.h.
  - this station publishes JSON to topic-3.

  Sensor set:
  - air-quality shield:
    humidity=0x00, temperature=0x01, PM2.5=0x04, TVOC=0x06, PM10=0x09
  - O3/CO/NH3 gas shield:
    label map: CO=0x06, O3=0x07, NH3=0x08.
    CO=0x06 was confirmed by smoke testing; O3/NH3 are accepted from label.
    This shield does not provide RH/temperature registers.
  - SO2/NO2/pressure gas shield:
    NO2=0x06, SO2=0x07, atmospheric pressure=0x12/0x13

  The numbers above are hexadecimal register addresses. They are not Modbus
  slave addresses.
*/

#define STATION_ID                          "telemetry-gas-1"
#define STATION_NAME                        "Telemetry Gas Pollution Station"
#define MQTT_STATION_TOPIC                  "topic-3"
#define MQTT_CLIENT_ID                      "mqttx_cdddf63c_gas"

#define JXCT_AIR_QUALITY_SHIELD_COUNT       1
#define JXBS_GAS_O3_CO_NH3_SHIELD_COUNT     1
#define JXBS_GAS_SO2_NO2_PRESSURE_SHIELD_COUNT 1

#define UPLOAD_RATE_MIN                     10
