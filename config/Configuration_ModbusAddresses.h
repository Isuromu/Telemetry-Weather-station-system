#pragma once

/*
  Configuration_ModbusAddresses.h

  Canonical Modbus RTU address plan for the Telemetry Weather Station project.

  Rules:
  - Addresses are decimal Modbus slave addresses.
  - One physical RS485 device or shield = one Modbus address.
  - Internal register addresses are not device addresses.
  - Combined shields keep one physical address even when they expose several
    telemetry values.
*/

// ============================================================
// Address families
// ============================================================

#define MODBUS_ADDR_AIR_ENV_START              1
#define MODBUS_ADDR_AIR_ENV_END                9

#define MODBUS_ADDR_SOIL_START                 10
#define MODBUS_ADDR_SOIL_END                   19

#define MODBUS_ADDR_LEAF_START                 20
#define MODBUS_ADDR_LEAF_END                   29

#define MODBUS_ADDR_WIND_SPEED_START           30
#define MODBUS_ADDR_WIND_SPEED_END             34

#define MODBUS_ADDR_WIND_DIRECTION_START       35
#define MODBUS_ADDR_WIND_DIRECTION_END         39

#define MODBUS_ADDR_RADIATION_START            40
#define MODBUS_ADDR_RADIATION_END              49

#define MODBUS_ADDR_WATER_START                50
#define MODBUS_ADDR_WATER_END                  59

#define MODBUS_ADDR_GAS_AIR_QUALITY_START      60
#define MODBUS_ADDR_GAS_AIR_QUALITY_END        69

#define MODBUS_ADDR_PARTICULATE_START          70
#define MODBUS_ADDR_PARTICULATE_END            74

#define MODBUS_ADDR_RAIN_START                 75
#define MODBUS_ADDR_RAIN_END                   79

#define MODBUS_ADDR_POWER_CONTROLLER_START     80
#define MODBUS_ADDR_POWER_CONTROLLER_END       89

// ============================================================
// Planned maximum devices per family
// ============================================================

#define MAX_AIR_TEMP_HUMIDITY_SENSORS          5
#define MAX_SOIL_SENSORS                       10
#define MAX_LEAF_SENSORS                       10
#define MAX_WIND_SPEED_SENSORS                 5
#define MAX_WIND_DIRECTION_SENSORS             5
#define MAX_RADIATION_EVAPORATION_SENSORS      10
#define MAX_WATER_SENSORS                      10
#define MAX_GAS_AIR_QUALITY_DEVICES            10
#define MAX_PARTICULATE_DEVICES                5

// ============================================================
// Air environment: 1..9
// ============================================================

#define ADDR_INSTRUMENT_SHELTER_THL_00         1
#define ADDR_AIR_TEMP_HUMIDITY_00              1
#define ADDR_AIR_TEMP_HUMIDITY_01              2
#define ADDR_AIR_TEMP_HUMIDITY_02              3
#define ADDR_AIR_TEMP_HUMIDITY_03              4
#define ADDR_AIR_TEMP_HUMIDITY_04              5

// ============================================================
// Soil: 10..19
// ============================================================

#define ADDR_SOIL_00                           10
#define ADDR_SOIL_01                           11
#define ADDR_SOIL_02                           12
#define ADDR_SOIL_03                           13
#define ADDR_SOIL_04                           14
#define ADDR_SOIL_05                           15
#define ADDR_SOIL_06                           16
#define ADDR_SOIL_07                           17
#define ADDR_SOIL_08                           18
#define ADDR_SOIL_09                           19

// ============================================================
// Leaf surface: 20..29
// ============================================================

#define ADDR_LEAF_00                           20
#define ADDR_LEAF_01                           21
#define ADDR_LEAF_02                           22
#define ADDR_LEAF_03                           23
#define ADDR_LEAF_04                           24
#define ADDR_LEAF_05                           25
#define ADDR_LEAF_06                           26
#define ADDR_LEAF_07                           27
#define ADDR_LEAF_08                           28
#define ADDR_LEAF_09                           29

#define ADDR_LEAF_SURFACE_HUMIDITY_00          ADDR_LEAF_00
#define ADDR_SMALL_LEAF_TEMP_HUMIDITY_00       ADDR_LEAF_01

// ============================================================
// Wind: 30..39
// ============================================================

#define ADDR_WIND_SPEED_00                     30
#define ADDR_WIND_SPEED_01                     31
#define ADDR_WIND_SPEED_02                     32
#define ADDR_WIND_SPEED_03                     33
#define ADDR_WIND_SPEED_04                     34

#define ADDR_WIND_DIRECTION_00                 35
#define ADDR_WIND_DIRECTION_01                 36
#define ADDR_WIND_DIRECTION_02                 37
#define ADDR_WIND_DIRECTION_03                 38
#define ADDR_WIND_DIRECTION_04                 39

// ============================================================
// Radiation, light, and evaporation: 40..49
// ============================================================

#define ADDR_UV_RAYS_00                        40
#define ADDR_PAR_00                            41
#define ADDR_TOTAL_SOLAR_RADIATION_00          42
#define ADDR_EVAPORATION_00                    43
#define ADDR_ILLUMINANCE_00                    44
#define ADDR_RADIATION_RESERVED_05             45
#define ADDR_RADIATION_RESERVED_06             46
#define ADDR_RADIATION_RESERVED_07             47
#define ADDR_RADIATION_RESERVED_08             48
#define ADDR_RADIATION_RESERVED_09             49

// ============================================================
// Water: 50..59
// ============================================================

#define ADDR_WATER_PH_00                       50
#define ADDR_WATER_EC_00                       51
#define ADDR_WATER_SUSPENDED_SOLIDS_00         52
#define ADDR_WATER_PH_01                       53
#define ADDR_WATER_EC_01                       54
#define ADDR_WATER_SUSPENDED_SOLIDS_01         55
#define ADDR_WATER_RESERVED_06                 56
#define ADDR_WATER_RESERVED_07                 57
#define ADDR_WATER_RESERVED_08                 58
#define ADDR_WATER_RESERVED_09                 59

// ============================================================
// Combined air-quality and gas shields: 60..69
// ============================================================

#define ADDR_AIR_QUALITY_SHIELD_00             60
#define ADDR_GAS_O3_CO_NH3_SHIELD_00           61
#define ADDR_TVOC_00                           60
#define ADDR_O3_00                             61
#define ADDR_CO_00                             62
#define ADDR_NH3_00                            63
#define ADDR_GAS_SO2_NO2_PRESSURE_SHIELD_00    64
#define ADDR_SO2_00                            64
#define ADDR_NO2_00                            65
#define ADDR_GAS_AIR_QUALITY_RESERVED_08       68
#define ADDR_GAS_AIR_QUALITY_RESERVED_09       69

// ============================================================
// Standalone particulate devices: 70..74
// Use only if PM2.5/PM10 is not inside ADDR_AIR_QUALITY_SHIELD_00.
// ============================================================

#define ADDR_PM25_PM10_00                      70
#define ADDR_PM25_PM10_01                      71
#define ADDR_PM25_PM10_02                      72
#define ADDR_PM25_PM10_03                      73
#define ADDR_PM25_PM10_04                      74

// ============================================================
// Rain and optional power controllers
// ============================================================

#define ADDR_OPTICAL_RAIN_00                   75
#define ADDR_LUMIAX_CONTROLLER_00              80
#define ADDR_EPEVER_CONTROLLER_00              81

// ============================================================
// Sanity checks
// ============================================================

#define MODBUS_ADDRESS_IN_RANGE(address, start, end) \
  (((address) >= (start)) && ((address) <= (end)))

static_assert(MODBUS_ADDRESS_IN_RANGE(ADDR_AIR_TEMP_HUMIDITY_00, MODBUS_ADDR_AIR_ENV_START, MODBUS_ADDR_AIR_ENV_END), "Air environment address is outside its range");
static_assert(MODBUS_ADDRESS_IN_RANGE(ADDR_SOIL_00, MODBUS_ADDR_SOIL_START, MODBUS_ADDR_SOIL_END), "Soil address is outside its range");
static_assert(MODBUS_ADDRESS_IN_RANGE(ADDR_LEAF_00, MODBUS_ADDR_LEAF_START, MODBUS_ADDR_LEAF_END), "Leaf address is outside its range");
static_assert(MODBUS_ADDRESS_IN_RANGE(ADDR_WIND_SPEED_00, MODBUS_ADDR_WIND_SPEED_START, MODBUS_ADDR_WIND_SPEED_END), "Wind speed address is outside its range");
static_assert(MODBUS_ADDRESS_IN_RANGE(ADDR_WIND_DIRECTION_00, MODBUS_ADDR_WIND_DIRECTION_START, MODBUS_ADDR_WIND_DIRECTION_END), "Wind direction address is outside its range");
static_assert(MODBUS_ADDRESS_IN_RANGE(ADDR_UV_RAYS_00, MODBUS_ADDR_RADIATION_START, MODBUS_ADDR_RADIATION_END), "Radiation address is outside its range");
static_assert(MODBUS_ADDRESS_IN_RANGE(ADDR_WATER_PH_00, MODBUS_ADDR_WATER_START, MODBUS_ADDR_WATER_END), "Water address is outside its range");
static_assert(MODBUS_ADDRESS_IN_RANGE(ADDR_AIR_QUALITY_SHIELD_00, MODBUS_ADDR_GAS_AIR_QUALITY_START, MODBUS_ADDR_GAS_AIR_QUALITY_END), "Air quality shield address is outside its range");
static_assert(MODBUS_ADDRESS_IN_RANGE(ADDR_GAS_O3_CO_NH3_SHIELD_00, MODBUS_ADDR_GAS_AIR_QUALITY_START, MODBUS_ADDR_GAS_AIR_QUALITY_END), "O3/CO/NH3 shield address is outside its range");
static_assert(MODBUS_ADDRESS_IN_RANGE(ADDR_GAS_SO2_NO2_PRESSURE_SHIELD_00, MODBUS_ADDR_GAS_AIR_QUALITY_START, MODBUS_ADDR_GAS_AIR_QUALITY_END), "SO2/NO2/pressure shield address is outside its range");
static_assert(MODBUS_ADDRESS_IN_RANGE(ADDR_PM25_PM10_00, MODBUS_ADDR_PARTICULATE_START, MODBUS_ADDR_PARTICULATE_END), "Particulate address is outside its range");
