#pragma once

/*
  Configuration_ModbusAddresses.h

  Canonical Modbus RTU address plan for the Telemetry Weather Station project.

  Rules:
  - Addresses are hexadecimal Modbus slave addresses.
  - Example: ADDR_WATER_PH_00 is 0x50, not decimal 50.
  - One physical RS485 device or shield = one Modbus address.
  - Internal register addresses are not device addresses.
  - Combined shields keep one physical address even when they expose several
    telemetry values.
*/

// ============================================================
// Address families
// ============================================================

#define MODBUS_ADDR_AIR_ENV_START              0x01
#define MODBUS_ADDR_AIR_ENV_END                0x09

#define MODBUS_ADDR_SOIL_START                 0x10
#define MODBUS_ADDR_SOIL_END                   0x19

#define MODBUS_ADDR_LEAF_START                 0x20
#define MODBUS_ADDR_LEAF_END                   0x29

#define MODBUS_ADDR_WIND_SPEED_START           0x30
#define MODBUS_ADDR_WIND_SPEED_END             0x34

#define MODBUS_ADDR_WIND_DIRECTION_START       0x35
#define MODBUS_ADDR_WIND_DIRECTION_END         0x39

#define MODBUS_ADDR_RADIATION_START            0x40
#define MODBUS_ADDR_RADIATION_END              0x49

#define MODBUS_ADDR_WATER_START                0x50
#define MODBUS_ADDR_WATER_END                  0x59

#define MODBUS_ADDR_GAS_AIR_QUALITY_START      0x60
#define MODBUS_ADDR_GAS_AIR_QUALITY_END        0x69

#define MODBUS_ADDR_PARTICULATE_START          0x70
#define MODBUS_ADDR_PARTICULATE_END            0x74

#define MODBUS_ADDR_RAIN_START                 0x75
#define MODBUS_ADDR_RAIN_END                   0x79

#define MODBUS_ADDR_POWER_CONTROLLER_START     0x80
#define MODBUS_ADDR_POWER_CONTROLLER_END       0x89

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
// Air environment: 0x01..0x09
// ============================================================

#define ADDR_INSTRUMENT_SHELTER_THL_00         0x01
#define ADDR_AIR_TEMP_HUMIDITY_00              0x01
#define ADDR_AIR_TEMP_HUMIDITY_01              0x02
#define ADDR_AIR_TEMP_HUMIDITY_02              0x03
#define ADDR_AIR_TEMP_HUMIDITY_03              0x04
#define ADDR_AIR_TEMP_HUMIDITY_04              0x05
#define ADDR_ATMOSPHERIC_PRESSURE_00           0x06
#define ADDR_ATMOSPHERIC_PRESSURE_01           0x07

// ============================================================
// Soil: 0x10..0x19
// ============================================================

#define ADDR_SOIL_00                           0x10
#define ADDR_SOIL_01                           0x11
#define ADDR_SOIL_02                           0x12
#define ADDR_SOIL_03                           0x13
#define ADDR_SOIL_04                           0x14
#define ADDR_SOIL_05                           0x15
#define ADDR_SOIL_06                           0x16
#define ADDR_SOIL_07                           0x17
#define ADDR_SOIL_08                           0x18
#define ADDR_SOIL_09                           0x19

// ============================================================
// Leaf surface: 0x20..0x29
// ============================================================

#define ADDR_LEAF_00                           0x20
#define ADDR_LEAF_01                           0x21
#define ADDR_LEAF_02                           0x22
#define ADDR_LEAF_03                           0x23
#define ADDR_LEAF_04                           0x24
#define ADDR_LEAF_05                           0x25
#define ADDR_LEAF_06                           0x26
#define ADDR_LEAF_07                           0x27
#define ADDR_LEAF_08                           0x28
#define ADDR_LEAF_09                           0x29

#define ADDR_LEAF_SURFACE_HUMIDITY_00          ADDR_LEAF_00
#define ADDR_SMALL_LEAF_TEMP_HUMIDITY_00       ADDR_LEAF_01

// ============================================================
// Wind: 0x30..0x39
// ============================================================

#define ADDR_WIND_SPEED_00                     0x30
#define ADDR_WIND_SPEED_01                     0x31
#define ADDR_WIND_SPEED_02                     0x32
#define ADDR_WIND_SPEED_03                     0x33
#define ADDR_WIND_SPEED_04                     0x34

#define ADDR_WIND_DIRECTION_00                 0x35
#define ADDR_WIND_DIRECTION_01                 0x36
#define ADDR_WIND_DIRECTION_02                 0x37
#define ADDR_WIND_DIRECTION_03                 0x38
#define ADDR_WIND_DIRECTION_04                 0x39

// ============================================================
// Radiation, light, and evaporation: 0x40..0x49
// ============================================================

#define ADDR_UV_RAYS_00                        0x40
#define ADDR_PAR_00                            0x41
#define ADDR_TOTAL_SOLAR_RADIATION_00          0x42
#define ADDR_EVAPORATION_00                    0x43
#define ADDR_ILLUMINANCE_00                    0x44
#define ADDR_RADIATION_RESERVED_05             0x45
#define ADDR_RADIATION_RESERVED_06             0x46
#define ADDR_RADIATION_RESERVED_07             0x47
#define ADDR_RADIATION_RESERVED_08             0x48
#define ADDR_RADIATION_RESERVED_09             0x49

// ============================================================
// Water: 0x50..0x59
// ============================================================

#define ADDR_WATER_PH_00                       0x50
#define ADDR_WATER_EC_00                       0x51
#define ADDR_WATER_SUSPENDED_SOLIDS_00         0x52
#define ADDR_WATER_PH_01                       0x53
#define ADDR_WATER_EC_01                       0x54
#define ADDR_WATER_SUSPENDED_SOLIDS_01         0x55
#define ADDR_WATER_RESERVED_06                 0x56
#define ADDR_WATER_RESERVED_07                 0x57
#define ADDR_WATER_RESERVED_08                 0x58
#define ADDR_WATER_RESERVED_09                 0x59

// ============================================================
// Combined air-quality and gas shields: 0x60..0x69
// ============================================================

#define ADDR_AIR_QUALITY_SHIELD_00             0x60
#define ADDR_GAS_O3_CO_NH3_SHIELD_00           0x61
#define ADDR_TVOC_00                           0x60
#define ADDR_O3_00                             0x61
#define ADDR_CO_00                             0x62
#define ADDR_NH3_00                            0x63
#define ADDR_GAS_SO2_NO2_PRESSURE_SHIELD_00    0x64
#define ADDR_SO2_00                            0x64
#define ADDR_NO2_00                            0x65
#define ADDR_GAS_AIR_QUALITY_RESERVED_08       0x68
#define ADDR_GAS_AIR_QUALITY_RESERVED_09       0x69

// ============================================================
// Standalone particulate devices: 0x70..0x74
// Use only if PM2.5/PM10 is not inside ADDR_AIR_QUALITY_SHIELD_00.
// ============================================================

#define ADDR_PM25_PM10_00                      0x70
#define ADDR_PM25_PM10_01                      0x71
#define ADDR_PM25_PM10_02                      0x72
#define ADDR_PM25_PM10_03                      0x73
#define ADDR_PM25_PM10_04                      0x74

// ============================================================
// Rain and optional power controllers
// ============================================================

#define ADDR_OPTICAL_RAIN_00                   0x75
#define ADDR_RAIN_RESERVED_01                  0x76
#define ADDR_RAIN_RESERVED_02                  0x77
#define ADDR_RAIN_RESERVED_03                  0x78
#define ADDR_RAIN_RESERVED_04                  0x79
#define ADDR_LUMIAX_CONTROLLER_00              0x80
#define ADDR_EPEVER_CONTROLLER_00              0x81

// ============================================================
// Sanity checks
// ============================================================

#define MODBUS_ADDRESS_IN_RANGE(address, start, end) \
  (((address) >= (start)) && ((address) <= (end)))

static_assert(MODBUS_ADDRESS_IN_RANGE(ADDR_AIR_TEMP_HUMIDITY_00, MODBUS_ADDR_AIR_ENV_START, MODBUS_ADDR_AIR_ENV_END), "Air environment address is outside its range");
static_assert(MODBUS_ADDRESS_IN_RANGE(ADDR_ATMOSPHERIC_PRESSURE_00, MODBUS_ADDR_AIR_ENV_START, MODBUS_ADDR_AIR_ENV_END), "Atmospheric pressure address is outside its range");
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
static_assert(MODBUS_ADDRESS_IN_RANGE(ADDR_OPTICAL_RAIN_00, MODBUS_ADDR_RAIN_START, MODBUS_ADDR_RAIN_END), "Rain address is outside its range");
