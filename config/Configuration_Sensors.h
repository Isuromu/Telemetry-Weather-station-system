#pragma once
#include <Arduino.h>
#include "Configuration.h"
#include "Configuration_System.h"
#include "Configuration_PCB.h"
#include "Configuration_ModbusAddresses.h"

/*
  Configuration_Sensors.h

  Main station sensor configuration.

  This is intentionally count-based, similar to firmware configuration files
  such as Marlin Configuration.h:
  - count = 0 means that sensor family is not installed in this station.
  - count = 1, 2, 3... creates that many runtime sensor objects at boot.
  - address arrays must contain unique Modbus addresses that match
    Configuration_ModbusAddresses.h.

  The firmware reads only configured sensors, but each cycle keeps a runtime
  flag per configured sensor. If a configured sensor does not answer during
  a cycle, its flag remains false and its data is not added to telemetry.
*/

// ============================================================
// Station sensor counts
// ============================================================

#ifndef RIKA_LEAF_SENSOR_COUNT
#define RIKA_LEAF_SENSOR_COUNT                  0
#endif
#ifndef JXBS_LEAF_SURFACE_HUMIDITY_COUNT
#define JXBS_LEAF_SURFACE_HUMIDITY_COUNT        0
#endif
#ifndef SMALL_LEAF_TEMP_HUMIDITY_COUNT
#define SMALL_LEAF_TEMP_HUMIDITY_COUNT          0
#endif

#ifndef JXCT_TEMPERATURE_HUMIDITY_COUNT
#define JXCT_TEMPERATURE_HUMIDITY_COUNT         0
#endif
#ifndef JXCT_ATMOSPHERIC_PRESSURE_COUNT
#define JXCT_ATMOSPHERIC_PRESSURE_COUNT         0
#endif

#ifndef RIKA_SOIL3IN1_COUNT
#define RIKA_SOIL3IN1_COUNT                     0
#endif
#ifndef JXBS_SOIL7IN1_COUNT
#define JXBS_SOIL7IN1_COUNT                     0
#endif

#ifndef JXCT_WIND_SPEED_COUNT
#define JXCT_WIND_SPEED_COUNT                   0
#endif
#ifndef JXCT_WIND_DIRECTION_COUNT
#define JXCT_WIND_DIRECTION_COUNT               0
#endif

#ifndef JXCT_UV_RAYS_COUNT
#define JXCT_UV_RAYS_COUNT                      0
#endif
#ifndef JXCT_PAR_COUNT
#define JXCT_PAR_COUNT                          0
#endif
#ifndef JXCT_TOTAL_SOLAR_RADIATION_COUNT
#define JXCT_TOTAL_SOLAR_RADIATION_COUNT        0
#endif
#ifndef JXCT_EVAPORATION_COUNT
#define JXCT_EVAPORATION_COUNT                  0
#endif
#ifndef JXBS_OPTICAL_RAIN_GAUGE_COUNT
#define JXBS_OPTICAL_RAIN_GAUGE_COUNT           0
#endif

#ifndef JXBS_WATER_PH_COUNT
#define JXBS_WATER_PH_COUNT                     0
#endif
#ifndef JXBS_WATER_CONDUCTIVITY_COUNT
#define JXBS_WATER_CONDUCTIVITY_COUNT           0
#endif
#ifndef JXSZ_WATER_SUSPENDED_SOLIDS_COUNT
#define JXSZ_WATER_SUSPENDED_SOLIDS_COUNT       0
#endif

#ifndef JXCT_AIR_QUALITY_SHIELD_COUNT
#define JXCT_AIR_QUALITY_SHIELD_COUNT           0
#endif
#ifndef JXBS_GAS_O3_CO_NH3_SHIELD_COUNT
#define JXBS_GAS_O3_CO_NH3_SHIELD_COUNT         0
#endif
#ifndef JXBS_GAS_SO2_NO2_PRESSURE_SHIELD_COUNT
#define JXBS_GAS_SO2_NO2_PRESSURE_SHIELD_COUNT  0
#endif
#ifndef JXBS_PM25_PM10_STANDALONE_COUNT
#define JXBS_PM25_PM10_STANDALONE_COUNT         0
#endif
#ifndef JXBS_GAS_CO_COUNT
#define JXBS_GAS_CO_COUNT                       0
#endif
#ifndef JXBS_GAS_O3_COUNT
#define JXBS_GAS_O3_COUNT                       0
#endif
#ifndef JXBS_GAS_NH3_COUNT
#define JXBS_GAS_NH3_COUNT                      0
#endif
#ifndef JXBS_GAS_SO2_COUNT
#define JXBS_GAS_SO2_COUNT                      0
#endif
#ifndef JXBS_GAS_NO2_COUNT
#define JXBS_GAS_NO2_COUNT                      0
#endif

// ============================================================
// Firmware-side maximums
// Keep these equal to or smaller than the address plan capacity.
// ============================================================

#define STATION_MAX_RIKA_LEAF_SENSORS           2
#define STATION_MAX_JXBS_LEAF_SENSORS           2
#define STATION_MAX_SMALL_LEAF_SENSORS          2
#define STATION_MAX_AIR_TEMP_HUMIDITY_SENSORS   2
#define STATION_MAX_ATMOSPHERIC_PRESSURE_SENSORS 2
#define STATION_MAX_RIKA_SOIL3IN1_SENSORS       5
#define STATION_MAX_JXBS_SOIL7IN1_SENSORS       5
#define STATION_MAX_WIND_SPEED_SENSORS          2
#define STATION_MAX_WIND_DIRECTION_SENSORS      2
#define STATION_MAX_RADIATION_SENSORS           2
#define STATION_MAX_WATER_SENSORS               2
#define STATION_MAX_AIR_QUALITY_SHIELDS         2
#define STATION_MAX_GAS_SHIELDS                 2
#define STATION_MAX_SINGLE_GAS_SENSORS          2
#define STATION_MAX_PM_STANDALONE_SENSORS       2
#define STATION_MAX_OPTICAL_RAIN_GAUGES         2

// ============================================================
// Address tables
// The main firmware uses the first N entries according to *_COUNT.
// ============================================================

constexpr uint8_t RIKA_LEAF_SENSOR_ADDRESSES[STATION_MAX_RIKA_LEAF_SENSORS] = {
  ADDR_LEAF_00,
  ADDR_LEAF_01
};

constexpr uint8_t JXBS_LEAF_SURFACE_HUMIDITY_ADDRESSES[STATION_MAX_JXBS_LEAF_SENSORS] = {
  ADDR_LEAF_SURFACE_HUMIDITY_00,
  ADDR_LEAF_02
};

constexpr uint8_t SMALL_LEAF_TEMP_HUMIDITY_ADDRESSES[STATION_MAX_SMALL_LEAF_SENSORS] = {
  ADDR_SMALL_LEAF_TEMP_HUMIDITY_00,
  ADDR_LEAF_03
};

constexpr uint8_t JXCT_TEMPERATURE_HUMIDITY_ADDRESSES[STATION_MAX_AIR_TEMP_HUMIDITY_SENSORS] = {
  ADDR_AIR_TEMP_HUMIDITY_00,
  ADDR_AIR_TEMP_HUMIDITY_01
};

constexpr uint8_t JXCT_ATMOSPHERIC_PRESSURE_ADDRESSES[STATION_MAX_ATMOSPHERIC_PRESSURE_SENSORS] = {
  ADDR_ATMOSPHERIC_PRESSURE_00,
  ADDR_ATMOSPHERIC_PRESSURE_01
};

constexpr uint8_t RIKA_SOIL3IN1_ADDRESSES[STATION_MAX_RIKA_SOIL3IN1_SENSORS] = {
  ADDR_SOIL_00,
  ADDR_SOIL_01,
  ADDR_SOIL_02,
  ADDR_SOIL_03,
  ADDR_SOIL_04
};

constexpr uint8_t JXBS_SOIL7IN1_ADDRESSES[STATION_MAX_JXBS_SOIL7IN1_SENSORS] = {
  ADDR_SOIL_00,
  ADDR_SOIL_01,
  ADDR_SOIL_02,
  ADDR_SOIL_03,
  ADDR_SOIL_04
};

constexpr uint8_t JXCT_WIND_SPEED_ADDRESSES[STATION_MAX_WIND_SPEED_SENSORS] = {
  ADDR_WIND_SPEED_00,
  ADDR_WIND_SPEED_01
};

constexpr uint8_t JXCT_WIND_DIRECTION_ADDRESSES[STATION_MAX_WIND_DIRECTION_SENSORS] = {
  ADDR_WIND_DIRECTION_00,
  ADDR_WIND_DIRECTION_01
};

constexpr uint8_t JXCT_UV_RAYS_ADDRESSES[STATION_MAX_RADIATION_SENSORS] = {
  ADDR_UV_RAYS_00,
  ADDR_RADIATION_RESERVED_05
};

constexpr uint8_t JXCT_PAR_ADDRESSES[STATION_MAX_RADIATION_SENSORS] = {
  ADDR_PAR_00,
  ADDR_RADIATION_RESERVED_06
};

constexpr uint8_t JXCT_TOTAL_SOLAR_RADIATION_ADDRESSES[STATION_MAX_RADIATION_SENSORS] = {
  ADDR_TOTAL_SOLAR_RADIATION_00,
  ADDR_RADIATION_RESERVED_07
};

constexpr uint8_t JXCT_EVAPORATION_ADDRESSES[STATION_MAX_RADIATION_SENSORS] = {
  ADDR_EVAPORATION_00,
  ADDR_RADIATION_RESERVED_08
};

constexpr uint8_t JXBS_WATER_PH_ADDRESSES[STATION_MAX_WATER_SENSORS] = {
  ADDR_WATER_PH_00,
  ADDR_WATER_PH_01
};

constexpr uint8_t JXBS_WATER_CONDUCTIVITY_ADDRESSES[STATION_MAX_WATER_SENSORS] = {
  ADDR_WATER_EC_00,
  ADDR_WATER_EC_01
};

constexpr uint8_t JXSZ_WATER_SUSPENDED_SOLIDS_ADDRESSES[STATION_MAX_WATER_SENSORS] = {
  ADDR_WATER_SUSPENDED_SOLIDS_00,
  ADDR_WATER_SUSPENDED_SOLIDS_01
};

constexpr uint8_t JXBS_OPTICAL_RAIN_GAUGE_ADDRESSES[STATION_MAX_OPTICAL_RAIN_GAUGES] = {
  ADDR_OPTICAL_RAIN_00,
  ADDR_RAIN_RESERVED_01
};

constexpr uint8_t JXCT_AIR_QUALITY_SHIELD_ADDRESSES[STATION_MAX_AIR_QUALITY_SHIELDS] = {
  ADDR_AIR_QUALITY_SHIELD_00,
  ADDR_PM25_PM10_01
};

constexpr uint8_t JXBS_GAS_O3_CO_NH3_SHIELD_ADDRESSES[STATION_MAX_GAS_SHIELDS] = {
  ADDR_GAS_O3_CO_NH3_SHIELD_00,
  ADDR_GAS_AIR_QUALITY_RESERVED_08
};

constexpr uint8_t JXBS_GAS_SO2_NO2_PRESSURE_SHIELD_ADDRESSES[STATION_MAX_GAS_SHIELDS] = {
  ADDR_GAS_SO2_NO2_PRESSURE_SHIELD_00,
  ADDR_GAS_AIR_QUALITY_RESERVED_09
};

constexpr uint8_t JXBS_PM25_PM10_STANDALONE_ADDRESSES[STATION_MAX_PM_STANDALONE_SENSORS] = {
  ADDR_PM25_PM10_00,
  ADDR_PM25_PM10_01
};

constexpr uint8_t JXBS_GAS_CO_ADDRESSES[STATION_MAX_SINGLE_GAS_SENSORS] = {
  ADDR_CO_00,
  ADDR_GAS_AIR_QUALITY_RESERVED_08
};

constexpr uint8_t JXBS_GAS_O3_ADDRESSES[STATION_MAX_SINGLE_GAS_SENSORS] = {
  ADDR_O3_00,
  ADDR_GAS_AIR_QUALITY_RESERVED_09
};

constexpr uint8_t JXBS_GAS_NH3_ADDRESSES[STATION_MAX_SINGLE_GAS_SENSORS] = {
  ADDR_NH3_00,
  ADDR_GAS_AIR_QUALITY_RESERVED_08
};

constexpr uint8_t JXBS_GAS_SO2_ADDRESSES[STATION_MAX_SINGLE_GAS_SENSORS] = {
  ADDR_SO2_00,
  ADDR_GAS_AIR_QUALITY_RESERVED_09
};

constexpr uint8_t JXBS_GAS_NO2_ADDRESSES[STATION_MAX_SINGLE_GAS_SENSORS] = {
  ADDR_NO2_00,
  ADDR_GAS_AIR_QUALITY_RESERVED_08
};

// ============================================================
// Common station sensor runtime parameters
// ============================================================

#define STATION_SENSOR_POWERLINE                POWERLINE_INDEX_0
#define STATION_SENSOR_RS485_PORT               RS485_PORT_INDEX_0
#define STATION_SENSOR_WARMUP_MS                1000UL
#define STATION_SENSOR_DEBUG                    true

// This simplified station reads every configured sensor once per upload cycle.
// SensorDriver still receives a rate value for its power policy, but there is
// no separate station-level sample schedule.
#define STATION_FAST_SAMPLE_RATE                UPLOAD_RATE_MIN
#define STATION_NORMAL_SAMPLE_RATE              UPLOAD_RATE_MIN

#define STATION_WATER_EC_SCALE_DIVISOR          100.0
#define STATION_WATER_EC_MAX_US_CM              200000.0
#define STATION_SUSPENDED_SOLIDS_SCALE_DIVISOR  10.0
#define STATION_SUSPENDED_SOLIDS_MAX_MG_L       20000.0

// ============================================================
// Configuration checks
// ============================================================

static_assert(RIKA_LEAF_SENSOR_COUNT <= STATION_MAX_RIKA_LEAF_SENSORS, "Too many Rika leaf sensors configured");
static_assert(JXBS_LEAF_SURFACE_HUMIDITY_COUNT <= STATION_MAX_JXBS_LEAF_SENSORS, "Too many JXBS leaf sensors configured");
static_assert(SMALL_LEAF_TEMP_HUMIDITY_COUNT <= STATION_MAX_SMALL_LEAF_SENSORS, "Too many small leaf sensors configured");
static_assert(JXCT_TEMPERATURE_HUMIDITY_COUNT <= STATION_MAX_AIR_TEMP_HUMIDITY_SENSORS, "Too many temperature/humidity sensors configured");
static_assert(JXCT_ATMOSPHERIC_PRESSURE_COUNT <= STATION_MAX_ATMOSPHERIC_PRESSURE_SENSORS, "Too many atmospheric pressure sensors configured");
static_assert(RIKA_SOIL3IN1_COUNT <= STATION_MAX_RIKA_SOIL3IN1_SENSORS, "Too many Rika soil sensors configured");
static_assert(JXBS_SOIL7IN1_COUNT <= STATION_MAX_JXBS_SOIL7IN1_SENSORS, "Too many JXBS soil sensors configured");
static_assert(JXCT_WIND_SPEED_COUNT <= STATION_MAX_WIND_SPEED_SENSORS, "Too many wind speed sensors configured");
static_assert(JXCT_WIND_DIRECTION_COUNT <= STATION_MAX_WIND_DIRECTION_SENSORS, "Too many wind direction sensors configured");
static_assert(JXCT_UV_RAYS_COUNT <= STATION_MAX_RADIATION_SENSORS, "Too many UV sensors configured");
static_assert(JXCT_PAR_COUNT <= STATION_MAX_RADIATION_SENSORS, "Too many PAR sensors configured");
static_assert(JXCT_TOTAL_SOLAR_RADIATION_COUNT <= STATION_MAX_RADIATION_SENSORS, "Too many solar radiation sensors configured");
static_assert(JXCT_EVAPORATION_COUNT <= STATION_MAX_RADIATION_SENSORS, "Too many evaporation sensors configured");
static_assert(JXBS_OPTICAL_RAIN_GAUGE_COUNT <= STATION_MAX_OPTICAL_RAIN_GAUGES, "Too many optical rain gauges configured");
static_assert(JXBS_WATER_PH_COUNT <= STATION_MAX_WATER_SENSORS, "Too many water pH sensors configured");
static_assert(JXBS_WATER_CONDUCTIVITY_COUNT <= STATION_MAX_WATER_SENSORS, "Too many water EC sensors configured");
static_assert(JXSZ_WATER_SUSPENDED_SOLIDS_COUNT <= STATION_MAX_WATER_SENSORS, "Too many suspended solids sensors configured");
static_assert(JXCT_AIR_QUALITY_SHIELD_COUNT <= STATION_MAX_AIR_QUALITY_SHIELDS, "Too many air quality shields configured");
static_assert(JXBS_GAS_O3_CO_NH3_SHIELD_COUNT <= STATION_MAX_GAS_SHIELDS, "Too many O3/CO/NH3 gas shields configured");
static_assert(JXBS_GAS_SO2_NO2_PRESSURE_SHIELD_COUNT <= STATION_MAX_GAS_SHIELDS, "Too many SO2/NO2/pressure gas shields configured");
static_assert(JXBS_PM25_PM10_STANDALONE_COUNT <= STATION_MAX_PM_STANDALONE_SENSORS, "Too many standalone PM sensors configured");
static_assert(JXBS_GAS_CO_COUNT <= STATION_MAX_SINGLE_GAS_SENSORS, "Too many CO sensors configured");
static_assert(JXBS_GAS_O3_COUNT <= STATION_MAX_SINGLE_GAS_SENSORS, "Too many O3 sensors configured");
static_assert(JXBS_GAS_NH3_COUNT <= STATION_MAX_SINGLE_GAS_SENSORS, "Too many NH3 sensors configured");
static_assert(JXBS_GAS_SO2_COUNT <= STATION_MAX_SINGLE_GAS_SENSORS, "Too many SO2 sensors configured");
static_assert(JXBS_GAS_NO2_COUNT <= STATION_MAX_SINGLE_GAS_SENSORS, "Too many NO2 sensors configured");
