#include <Arduino.h>
#include <time.h>
#include <WiFi.h>
#include <PubSubClient.h>

#include "../config/Configuration.h"
#include "../config/Configuration_System.h"
#include "../config/Configuration_PCB.h"
#include "../config/Configuration_ModbusAddresses.h"
#include "../config/Configuration_Sensors.h"
#include "../config/Configuration_Network.h"
#include "../config/Configuration_Telemetry.h"

#include "PrintController.h"
#include "RS485Modbus.h"
#include "SensorDriver.h"

#include "RikaLeafSensor.h"
#include "RikaSoilSensor3in1.h"
#include "JXBS_LeafSurfaceHumidity.h"
#include "SmallLeafTemperatureHumidity.h"
#include "JXBS_SoilComp7in1.h"
#include "JXBS_WaterPH.h"
#include "JXBS_WaterConductivity.h"
#include "JXSZ_WaterSuspendedSolids.h"
#include "JXCT_WeatherStationSensors.h"

// ============================================================
// Debug and transport ports
// ============================================================

HardwareSerial& DebugPort = Serial0;
HardwareSerial RS485Hw0(1);

static PrintController printer(DebugPort, false);
static RS485Bus rs485Bus0;
static WiFiClient wifiClient;
static PubSubClient mqttClient(wifiClient);

// ============================================================
// Runtime registry
// ============================================================

enum SensorKind : uint8_t {
  KIND_RIKA_LEAF,
  KIND_JXBS_LEAF_SURFACE,
  KIND_SMALL_LEAF,
  KIND_RIKA_SOIL3IN1,
  KIND_JXBS_SOIL7IN1,
  KIND_WIND_SPEED,
  KIND_WIND_DIRECTION,
  KIND_UV_RAYS,
  KIND_PAR,
  KIND_TOTAL_SOLAR,
  KIND_EVAPORATION,
  KIND_WATER_PH,
  KIND_WATER_EC,
  KIND_WATER_SS,
  KIND_AIR_QUALITY_SHIELD,
  KIND_GAS_O3_CO_NH3_SHIELD,
  KIND_GAS_SO2_NO2_PRESSURE_SHIELD
};

struct RuntimeSensor {
  SensorDriver* driver;
  SensorKind kind;
  char id[32];
  char key[32];
  bool configured;
  bool lastReadOk;
};

static const size_t MAX_RUNTIME_SENSORS = 48;
static RuntimeSensor g_runtimeSensors[MAX_RUNTIME_SENSORS];
static size_t g_runtimeSensorCount = 0;

static bool g_powerLineState[PCB_POWERLINE_COUNT] = {false};
static bool g_rs485InterfaceState[PCB_RS485_PORT_COUNT] = {false};

static uint32_t g_lastCycleMs = 0;
static uint32_t g_lastTimeSyncMs = 0;
static bool g_timeIsSynced = false;

// ============================================================
// Utility helpers
// ============================================================

static void printBanner() {
  printer.println(F(""), true);
  printer.println(F("============================================================"), true);
  printer.println(F(" Telemetry Weather Station Main Firmware"), true);
  printer.println(F("============================================================"), true);
  printer.print(F("Station ID: "), true);
  printer.println(STATION_ID, true);
  printer.print(F("Station name: "), true);
  printer.println(STATION_NAME, true);
  printer.print(F("PCB: "), true);
  printer.println(PCB_NAME, true);
  printer.print(F("MQTT JSON topic: "), true);
  printer.println(MQTT_JSON_TOPIC, true);
  printer.print(F("Upload rate min: "), true);
  printer.println((unsigned int)UPLOAD_RATE_MIN, true);
  printer.println(F(""), true);
}

static void makeIndexedName(char* out,
                            size_t outSize,
                            const char* prefix,
                            uint8_t index,
                            bool oneBased = true) {
  const uint8_t printedIndex = oneBased ? (uint8_t)(index + 1) : index;
  snprintf(out, outSize, "%s%u", prefix, (unsigned int)printedIndex);
}

static bool addRuntimeSensor(SensorDriver* driver,
                             SensorKind kind,
                             const char* id,
                             const char* key) {
  if (!driver || g_runtimeSensorCount >= MAX_RUNTIME_SENSORS) {
    return false;
  }

  RuntimeSensor& slot = g_runtimeSensors[g_runtimeSensorCount++];
  slot.driver = driver;
  slot.kind = kind;
  strlcpy(slot.id, id, sizeof(slot.id));
  strlcpy(slot.key, key, sizeof(slot.key));
  slot.configured = true;
  slot.lastReadOk = false;
  return true;
}

static void resetRuntimeFlags() {
  for (size_t i = 0; i < g_runtimeSensorCount; ++i) {
    g_runtimeSensors[i].lastReadOk = false;
  }
}

static void appendJsonEscaped(String& out, const char* value) {
  out += '"';
  if (value) {
    for (const char* p = value; *p; ++p) {
      if (*p == '"' || *p == '\\') out += '\\';
      out += *p;
    }
  }
  out += '"';
}

static void appendJsonKey(String& out, const char* key, bool& first) {
  if (!first) out += ',';
  first = false;
  appendJsonEscaped(out, key);
  out += ':';
}

static void appendJsonNumber(String& out, const char* key, double value, uint8_t decimals, bool& first) {
  appendJsonKey(out, key, first);
  out += String(value, (unsigned int)decimals);
}

static void appendJsonUInt(String& out, const char* key, uint32_t value, bool& first) {
  appendJsonKey(out, key, first);
  out += String(value);
}

static void appendJsonBool(String& out, const char* key, bool value, bool& first) {
  appendJsonKey(out, key, first);
  out += value ? F("true") : F("false");
}

static void appendDataKey(char* out, size_t outSize, const char* prefix, const char* suffix) {
  snprintf(out, outSize, "%s_%s", prefix, suffix);
}

static void appendStringField(String& out,
                              const char* key,
                              const String& value,
                              bool& first) {
  if (!first) out += ',';
  first = false;
  out += key;
  out += '=';
  out += value;
}

// ============================================================
// Power and RS485 interface control
// ============================================================

static void initPowerLines() {
  for (uint8_t i = 0; i < PCB_POWERLINE_COUNT; ++i) {
    const int8_t pin = PCB_POWERLINE_SWITCH_PINS[i];
    if (pin >= 0) {
      pinMode(pin, OUTPUT);
      const bool activeHigh = PCB_POWERLINE_ACTIVE_HIGH[i];
      digitalWrite(pin, activeHigh ? LOW : HIGH);
    }
    g_powerLineState[i] = false;
  }
}

static void initInterfaces() {
  for (uint8_t i = 0; i < PCB_RS485_PORT_COUNT; ++i) {
    const int8_t enPin = PCB_RS485_ENABLE_PINS[i];
    if (enPin >= 0) {
      pinMode(enPin, OUTPUT);
      const bool activeHigh = PCB_RS485_ENABLE_ACTIVE_HIGH[i];
      digitalWrite(enPin, activeHigh ? LOW : HIGH);
    }
    g_rs485InterfaceState[i] = false;
  }
}

static void powerLineSet(uint8_t index, bool on) {
  if (index >= PCB_POWERLINE_COUNT) return;

  const int8_t pin = PCB_POWERLINE_SWITCH_PINS[index];
  if (pin >= 0) {
    const bool activeHigh = PCB_POWERLINE_ACTIVE_HIGH[index];
    digitalWrite(pin, on ? (activeHigh ? HIGH : LOW)
                         : (activeHigh ? LOW : HIGH));
  }
  g_powerLineState[index] = on;
}

static bool powerLineReadState(uint8_t index) {
  if (index >= PCB_POWERLINE_COUNT) return false;

  const int8_t statusPin = PCB_POWERLINE_STATUS_PINS[index];
  if (statusPin >= 0) {
    const bool activeHigh = PCB_POWERLINE_STATUS_ACTIVE_HIGH[index];
    const int raw = digitalRead(statusPin);
    return activeHigh ? (raw == HIGH) : (raw == LOW);
  }

  if (PCB_POWERLINE_SWITCH_PINS[index] < 0) {
    return true;
  }

  return g_powerLineState[index];
}

static void rs485InterfaceSet(uint8_t index, bool on) {
  if (index >= PCB_RS485_PORT_COUNT) return;

  const int8_t pin = PCB_RS485_ENABLE_PINS[index];
  if (pin >= 0) {
    const bool activeHigh = PCB_RS485_ENABLE_ACTIVE_HIGH[index];
    digitalWrite(pin, on ? (activeHigh ? HIGH : LOW)
                         : (activeHigh ? LOW : HIGH));
  }
  g_rs485InterfaceState[index] = on;
}

static uint32_t maxWarmupForPowerLine(uint8_t powerLine) {
  uint32_t maxWarmup = 0;
  for (size_t i = 0; i < g_runtimeSensorCount; ++i) {
    SensorDriver* sensor = g_runtimeSensors[i].driver;
    if (sensor->getPowerLineIndex() == powerLine && sensor->getWarmUpTimeMs() > maxWarmup) {
      maxWarmup = sensor->getWarmUpTimeMs();
    }
  }
  return maxWarmup;
}

static bool hasSensorOnPowerLine(uint8_t powerLine) {
  for (size_t i = 0; i < g_runtimeSensorCount; ++i) {
    if (g_runtimeSensors[i].driver->getPowerLineIndex() == powerLine) {
      return true;
    }
  }
  return false;
}

static bool hasSensorOnInterface(uint8_t powerLine, uint8_t interfaceIndex) {
  for (size_t i = 0; i < g_runtimeSensorCount; ++i) {
    SensorDriver* sensor = g_runtimeSensors[i].driver;
    if (sensor->getPowerLineIndex() == powerLine &&
        sensor->getInterfaceIndex() == interfaceIndex) {
      return true;
    }
  }
  return false;
}

static bool powerLineShouldStayOn(uint8_t powerLine) {
  if (powerLine >= PCB_POWERLINE_COUNT) return false;
  if (!PCB_POWERLINE_CAN_STAY_ON_IN_SLEEP[powerLine]) return false;

  for (size_t i = 0; i < g_runtimeSensorCount; ++i) {
    SensorDriver* sensor = g_runtimeSensors[i].driver;
    if (sensor->getPowerLineIndex() == powerLine && sensor->shouldKeepPowerOn()) {
      return true;
    }
  }
  return false;
}

// ============================================================
// Runtime sensor construction
// ============================================================

static void registerRikaLeafSensors() {
  for (uint8_t i = 0; i < RIKA_LEAF_SENSOR_COUNT; ++i) {
    char id[32];
    char key[32];
    makeIndexedName(id, sizeof(id), "rika_leaf_", i, false);
    makeIndexedName(key, sizeof(key), "Leaf", i, true);

    addRuntimeSensor(new RikaLeafSensor(rs485Bus0,
                                        id,
                                        RIKA_LEAF_SENSOR_ADDRESSES[i],
                                        STATION_SENSOR_DEBUG,
                                        STATION_SENSOR_POWERLINE,
                                        STATION_SENSOR_RS485_PORT,
                                        STATION_FAST_SAMPLE_RATE,
                                        STATION_SENSOR_WARMUP_MS,
                                        SENSOR_DEFAULT_MAX_ERRORS,
                                        MIN_USEFUL_POWER_OFF_MS),
                     KIND_RIKA_LEAF,
                     id,
                     key);
  }
}

static void registerLeafSurfaceSensors() {
  for (uint8_t i = 0; i < JXBS_LEAF_SURFACE_HUMIDITY_COUNT; ++i) {
    char id[32];
    char key[32];
    makeIndexedName(id, sizeof(id), "leaf_surface_", i, false);
    makeIndexedName(key, sizeof(key), "LeafSurface", i, true);

    addRuntimeSensor(new JXBS_LeafSurfaceHumidity(rs485Bus0,
                                                  id,
                                                  JXBS_LEAF_SURFACE_HUMIDITY_ADDRESSES[i],
                                                  STATION_SENSOR_DEBUG,
                                                  STATION_SENSOR_POWERLINE,
                                                  STATION_SENSOR_RS485_PORT,
                                                  STATION_FAST_SAMPLE_RATE,
                                                  STATION_SENSOR_WARMUP_MS,
                                                  SENSOR_DEFAULT_MAX_ERRORS,
                                                  MIN_USEFUL_POWER_OFF_MS),
                     KIND_JXBS_LEAF_SURFACE,
                     id,
                     key);
  }
}

static void registerSmallLeafSensors() {
  for (uint8_t i = 0; i < SMALL_LEAF_TEMP_HUMIDITY_COUNT; ++i) {
    char id[32];
    char key[32];
    makeIndexedName(id, sizeof(id), "small_leaf_", i, false);
    makeIndexedName(key, sizeof(key), "SmallLeaf", i, true);

    addRuntimeSensor(new SmallLeafTemperatureHumidity(rs485Bus0,
                                                      id,
                                                      SMALL_LEAF_TEMP_HUMIDITY_ADDRESSES[i],
                                                      STATION_SENSOR_DEBUG,
                                                      STATION_SENSOR_POWERLINE,
                                                      STATION_SENSOR_RS485_PORT,
                                                      STATION_FAST_SAMPLE_RATE,
                                                      STATION_SENSOR_WARMUP_MS,
                                                      SENSOR_DEFAULT_MAX_ERRORS,
                                                      MIN_USEFUL_POWER_OFF_MS),
                     KIND_SMALL_LEAF,
                     id,
                     key);
  }
}

static void registerRikaSoilSensors() {
  for (uint8_t i = 0; i < RIKA_SOIL3IN1_COUNT; ++i) {
    char id[32];
    char key[32];
    makeIndexedName(id, sizeof(id), "soil_", i, false);
    makeIndexedName(key, sizeof(key), "Soil", i, true);

    addRuntimeSensor(new RikaSoilSensor3in1(rs485Bus0,
                                            id,
                                            RIKA_SOIL3IN1_ADDRESSES[i],
                                            STATION_SENSOR_DEBUG,
                                            STATION_SENSOR_POWERLINE,
                                            STATION_SENSOR_RS485_PORT,
                                            STATION_NORMAL_SAMPLE_RATE,
                                            STATION_SENSOR_WARMUP_MS,
                                            SENSOR_DEFAULT_MAX_ERRORS,
                                            MIN_USEFUL_POWER_OFF_MS),
                     KIND_RIKA_SOIL3IN1,
                     id,
                     key);
  }
}

static void registerJXBSSoilSensors() {
  for (uint8_t i = 0; i < JXBS_SOIL7IN1_COUNT; ++i) {
    char id[32];
    char key[32];
    makeIndexedName(id, sizeof(id), "soil7_", i, false);
    makeIndexedName(key, sizeof(key), "Soil7", i, true);

    addRuntimeSensor(new JXBS_SoilComp7in1(rs485Bus0,
                                           id,
                                           JXBS_SOIL7IN1_ADDRESSES[i],
                                           STATION_SENSOR_DEBUG,
                                           STATION_SENSOR_POWERLINE,
                                           STATION_SENSOR_RS485_PORT,
                                           STATION_NORMAL_SAMPLE_RATE,
                                           STATION_SENSOR_WARMUP_MS,
                                           SENSOR_DEFAULT_MAX_ERRORS,
                                           MIN_USEFUL_POWER_OFF_MS),
                     KIND_JXBS_SOIL7IN1,
                     id,
                     key);
  }
}

static void registerJXCTWeatherSensors() {
  for (uint8_t i = 0; i < JXCT_WIND_SPEED_COUNT; ++i) {
    char id[32]; char key[32];
    makeIndexedName(id, sizeof(id), "wind_speed_", i, false);
    makeIndexedName(key, sizeof(key), "WindS", i, true);
    addRuntimeSensor(new JXCT_WindSpeed(rs485Bus0, id, JXCT_WIND_SPEED_ADDRESSES[i], STATION_SENSOR_DEBUG), KIND_WIND_SPEED, id, key);
  }

  for (uint8_t i = 0; i < JXCT_WIND_DIRECTION_COUNT; ++i) {
    char id[32]; char key[32];
    makeIndexedName(id, sizeof(id), "wind_direction_", i, false);
    makeIndexedName(key, sizeof(key), "WindD", i, true);
    addRuntimeSensor(new JXCT_WindDirection(rs485Bus0, id, JXCT_WIND_DIRECTION_ADDRESSES[i], STATION_SENSOR_DEBUG), KIND_WIND_DIRECTION, id, key);
  }

  for (uint8_t i = 0; i < JXCT_UV_RAYS_COUNT; ++i) {
    char id[32]; char key[32];
    makeIndexedName(id, sizeof(id), "uv_", i, false);
    makeIndexedName(key, sizeof(key), "UV", i, true);
    addRuntimeSensor(new JXCT_UVRays(rs485Bus0, id, JXCT_UV_RAYS_ADDRESSES[i], STATION_SENSOR_DEBUG), KIND_UV_RAYS, id, key);
  }

  for (uint8_t i = 0; i < JXCT_PAR_COUNT; ++i) {
    char id[32]; char key[32];
    makeIndexedName(id, sizeof(id), "par_", i, false);
    makeIndexedName(key, sizeof(key), "PAR", i, true);
    addRuntimeSensor(new JXCT_PAR(rs485Bus0, id, JXCT_PAR_ADDRESSES[i], STATION_SENSOR_DEBUG), KIND_PAR, id, key);
  }

  for (uint8_t i = 0; i < JXCT_TOTAL_SOLAR_RADIATION_COUNT; ++i) {
    char id[32]; char key[32];
    makeIndexedName(id, sizeof(id), "solar_", i, false);
    makeIndexedName(key, sizeof(key), "Solar", i, true);
    addRuntimeSensor(new JXCT_TotalSolarRadiation(rs485Bus0, id, JXCT_TOTAL_SOLAR_RADIATION_ADDRESSES[i], STATION_SENSOR_DEBUG), KIND_TOTAL_SOLAR, id, key);
  }

  for (uint8_t i = 0; i < JXCT_EVAPORATION_COUNT; ++i) {
    char id[32]; char key[32];
    makeIndexedName(id, sizeof(id), "evaporation_", i, false);
    makeIndexedName(key, sizeof(key), "Evap", i, true);
    addRuntimeSensor(new JXCT_Evaporation(rs485Bus0, id, JXCT_EVAPORATION_ADDRESSES[i], STATION_SENSOR_DEBUG), KIND_EVAPORATION, id, key);
  }
}

static void registerWaterSensors() {
  for (uint8_t i = 0; i < JXBS_WATER_PH_COUNT; ++i) {
    char id[32]; char key[32];
    makeIndexedName(id, sizeof(id), "water_ph_", i, false);
    makeIndexedName(key, sizeof(key), "WaterPH", i, true);
    addRuntimeSensor(new JXBS_WaterPH(rs485Bus0, id, JXBS_WATER_PH_ADDRESSES[i], STATION_SENSOR_DEBUG), KIND_WATER_PH, id, key);
  }

  for (uint8_t i = 0; i < JXBS_WATER_CONDUCTIVITY_COUNT; ++i) {
    char id[32]; char key[32];
    makeIndexedName(id, sizeof(id), "water_ec_", i, false);
    makeIndexedName(key, sizeof(key), "WaterEC", i, true);
    addRuntimeSensor(new JXBS_WaterConductivity(rs485Bus0,
                                                id,
                                                JXBS_WATER_CONDUCTIVITY_ADDRESSES[i],
                                                STATION_SENSOR_DEBUG,
                                                STATION_WATER_EC_SCALE_DIVISOR,
                                                STATION_WATER_EC_MAX_US_CM),
                     KIND_WATER_EC,
                     id,
                     key);
  }

  for (uint8_t i = 0; i < JXSZ_WATER_SUSPENDED_SOLIDS_COUNT; ++i) {
    char id[32]; char key[32];
    makeIndexedName(id, sizeof(id), "water_ss_", i, false);
    makeIndexedName(key, sizeof(key), "WaterSS", i, true);
    addRuntimeSensor(new JXSZ_WaterSuspendedSolids(rs485Bus0,
                                                   id,
                                                   JXSZ_WATER_SUSPENDED_SOLIDS_ADDRESSES[i],
                                                   STATION_SENSOR_DEBUG,
                                                   STATION_SUSPENDED_SOLIDS_SCALE_DIVISOR,
                                                   STATION_SUSPENDED_SOLIDS_MAX_MG_L),
                     KIND_WATER_SS,
                     id,
                     key);
  }
}

static void registerShieldSensors() {
  for (uint8_t i = 0; i < JXCT_AIR_QUALITY_SHIELD_COUNT; ++i) {
    char id[32]; char key[32];
    makeIndexedName(id, sizeof(id), "air_quality_", i, false);
    makeIndexedName(key, sizeof(key), "AirQ", i, true);
    addRuntimeSensor(new JXCT_AirQualityShield(rs485Bus0, id, JXCT_AIR_QUALITY_SHIELD_ADDRESSES[i], STATION_SENSOR_DEBUG), KIND_AIR_QUALITY_SHIELD, id, key);
  }

  for (uint8_t i = 0; i < JXBS_GAS_O3_CO_NH3_SHIELD_COUNT; ++i) {
    char id[32]; char key[32];
    makeIndexedName(id, sizeof(id), "gas_o3_co_nh3_", i, false);
    makeIndexedName(key, sizeof(key), "GasA", i, true);
    addRuntimeSensor(new JXBS_GasO3CONH3Shield(rs485Bus0, id, JXBS_GAS_O3_CO_NH3_SHIELD_ADDRESSES[i], STATION_SENSOR_DEBUG), KIND_GAS_O3_CO_NH3_SHIELD, id, key);
  }

  for (uint8_t i = 0; i < JXBS_GAS_SO2_NO2_PRESSURE_SHIELD_COUNT; ++i) {
    char id[32]; char key[32];
    makeIndexedName(id, sizeof(id), "gas_so2_no2_pressure_", i, false);
    makeIndexedName(key, sizeof(key), "GasB", i, true);
    addRuntimeSensor(new JXBS_GasSO2NO2PressureShield(rs485Bus0, id, JXBS_GAS_SO2_NO2_PRESSURE_SHIELD_ADDRESSES[i], STATION_SENSOR_DEBUG), KIND_GAS_SO2_NO2_PRESSURE_SHIELD, id, key);
  }
}

static void buildRuntimeSensors() {
  registerRikaLeafSensors();
  registerLeafSurfaceSensors();
  registerSmallLeafSensors();
  registerRikaSoilSensors();
  registerJXBSSoilSensors();
  registerJXCTWeatherSensors();
  registerWaterSensors();
  registerShieldSensors();
}

static void printSensorMap() {
  printer.println(F("Configured sensor map:"), true);
  if (g_runtimeSensorCount == 0) {
    printer.println(F("  <none>"), true);
    return;
  }

  for (size_t i = 0; i < g_runtimeSensorCount; ++i) {
    SensorDriver* s = g_runtimeSensors[i].driver;
    printer.print(F("  #"), true);
    printer.print((unsigned int)i, true, " ");
    printer.print(g_runtimeSensors[i].id, true, " | key=");
    printer.print(g_runtimeSensors[i].key, true, " | addr=0x");
    printer.print((unsigned int)s->getAddress(), true, " | ", HEX);
    printer.print(F("sampleMin="), true);
    printer.println((unsigned int)s->getSampleRateMin(), true);
  }
}

// ============================================================
// Time, Wi-Fi, MQTT, battery
// ============================================================

static bool ensureWifiConnected() {
  if (!WIFI_ENABLED) return false;
  if (WiFi.status() == WL_CONNECTED) return true;

  printer.print(F("[NET] Connecting Wi-Fi SSID: "), true);
  printer.println(WIFI_SSID, true);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < WIFI_CONNECT_TIMEOUT_MS) {
    delay(250);
  }

  if (WiFi.status() == WL_CONNECTED) {
    printer.print(F("[NET] Wi-Fi connected, IP: "), true);
    printer.println(WiFi.localIP().toString().c_str(), true);
    return true;
  }

  printer.println(F("[NET] Wi-Fi connection failed"), true);
  return false;
}

static bool internetLooksAvailable() {
  if (WiFi.status() != WL_CONNECTED) return false;

  IPAddress resolved;
  const bool ok = WiFi.hostByName(TIME_NTP_SERVER_1, resolved) == 1;
  if (!ok) {
    printer.println(F("[NET] DNS/internet check failed"), true);
  }
  return ok;
}

static bool syncTimeFromInternet(bool force) {
  if (!TIME_SYNC_ENABLED) return false;
  if (!force && g_timeIsSynced && (millis() - g_lastTimeSyncMs) < TIME_SYNC_INTERVAL_MS) {
    return true;
  }
  if (!ensureWifiConnected() || !internetLooksAvailable()) {
    return false;
  }

  printer.println(F("[TIME] Syncing UTC time from NTP"), true);
  configTime(0, 0, TIME_NTP_SERVER_1, TIME_NTP_SERVER_2);

  struct tm timeInfo;
  const uint32_t start = millis();
  while ((millis() - start) < TIME_SYNC_TIMEOUT_MS) {
    if (getLocalTime(&timeInfo, 500)) {
      g_timeIsSynced = true;
      g_lastTimeSyncMs = millis();
      printer.println(F("[TIME] Time sync OK"), true);
      return true;
    }
  }

  printer.println(F("[TIME] Time sync failed"), true);
  return false;
}

static uint32_t currentUnixTime() {
  time_t now = time(nullptr);
  if (now < 100000UL) return 0;
  return (uint32_t)now;
}

static void currentDateTimeUTC(char* out, size_t outSize) {
  time_t now = time(nullptr);
  struct tm timeInfo;
  gmtime_r(&now, &timeInfo);
  snprintf(out, outSize, "%04d-%02d-%02d %02d:%02d:%02d",
           timeInfo.tm_year + 1900,
           timeInfo.tm_mon + 1,
           timeInfo.tm_mday,
           timeInfo.tm_hour,
           timeInfo.tm_min,
           timeInfo.tm_sec);
}

static bool readBatteryVoltage(double& voltage) {
  voltage = 0.0;
  if (!BATTERY_MONITOR_ENABLED || BATTERY_ADC_PIN < 0) return false;

  #if defined(ARDUINO_ARCH_ESP32)
    analogSetPinAttenuation(BATTERY_ADC_PIN, (adc_attenuation_t)BATTERY_ADC_ATTENUATION_DB);
    const uint32_t mv = analogReadMilliVolts(BATTERY_ADC_PIN);
    const double pinV = (double)mv / 1000.0;
  #else
    const double pinV = (double)analogRead(BATTERY_ADC_PIN) * (5.0 / 1023.0);
  #endif

  voltage = pinV * ((BATTERY_DIVIDER_R1_OHMS + BATTERY_DIVIDER_R2_OHMS) / BATTERY_DIVIDER_R2_OHMS) +
            BATTERY_DIODE_DROP_V;
  return voltage > 0.1;
}

static bool ensureMqttConnected() {
  if (!MQTT_ENABLED) return false;
  if (mqttClient.connected()) return true;
  if (!ensureWifiConnected() || !internetLooksAvailable()) return false;

  mqttClient.setServer(MQTT_HOST, MQTT_PORT);

  const uint32_t start = millis();
  while (!mqttClient.connected() && (millis() - start) < MQTT_CONNECT_TIMEOUT_MS) {
    printer.println(F("[MQTT] Connecting..."), true);
    if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD)) {
      printer.println(F("[MQTT] Connected"), true);
      return true;
    }
    delay(1000);
  }

  printer.println(F("[MQTT] Connection failed"), true);
  return false;
}

static bool publishMqtt(const char* topic, const String& payload) {
  if (!ensureMqttConnected()) return false;

  const bool ok = mqttClient.publish(topic, payload.c_str());
  printer.print(ok ? F("[MQTT] Published: ") : F("[MQTT] Publish failed: "), true);
  printer.println(topic, true);
  return ok;
}

// ============================================================
// Sensor reading
// ============================================================

static void readConfiguredSensors() {
  resetRuntimeFlags();

  for (uint8_t powerLine = 0; powerLine < PCB_POWERLINE_COUNT; ++powerLine) {
    if (!hasSensorOnPowerLine(powerLine)) continue;

    if (!powerLineReadState(powerLine)) {
      powerLineSet(powerLine, true);
    }

    const uint32_t warmup = maxWarmupForPowerLine(powerLine);
    if (warmup > 0) delay(warmup);

    for (uint8_t iface = 0; iface < PCB_RS485_PORT_COUNT; ++iface) {
      if (!hasSensorOnInterface(powerLine, iface)) continue;

      rs485InterfaceSet(iface, true);
      if (PCB_RS485_ENABLE_DELAY_MS[iface] > 0) delay(PCB_RS485_ENABLE_DELAY_MS[iface]);

      for (size_t i = 0; i < g_runtimeSensorCount; ++i) {
        SensorDriver* sensor = g_runtimeSensors[i].driver;
        if (sensor->getPowerLineIndex() != powerLine) continue;
        if (sensor->getInterfaceIndex() != iface) continue;

        printer.print(F("[READ] "), true);
        printer.print(g_runtimeSensors[i].id, true, " addr=0x");
        printer.println((unsigned int)sensor->getAddress(), true, "", HEX);

        const bool ok = sensor->readData();
        g_runtimeSensors[i].lastReadOk = ok;

        printer.print(ok ? F("[READ] OK: ") : F("[READ] FAIL: "), true);
        printer.println(g_runtimeSensors[i].id, true);
      }

      rs485InterfaceSet(iface, false);
    }

    if (!powerLineShouldStayOn(powerLine)) {
      powerLineSet(powerLine, false);
    }
  }
}

// ============================================================
// Payload builders
// ============================================================

struct TelemetryFieldCounters {
  uint8_t leaf;
  uint8_t soil;
  uint8_t windSpeed;
  uint8_t windDirection;
  uint8_t uv;
  uint8_t par;
  uint8_t totalSolar;
  uint8_t evaporation;
  uint8_t waterTemperature;
  uint8_t waterPH;
  uint8_t waterEC;
  uint8_t waterSS;
  uint8_t airTemperature;
  uint8_t airHumidity;
  uint8_t pm25;
  uint8_t pm10;
  uint8_t tvoc;
  uint8_t co;
  uint8_t o3;
  uint8_t nh3;
  uint8_t so2;
  uint8_t no2;
  uint8_t atmosphericPressure;
};

static void makeTelemetryField(char* out, size_t outSize, const char* base, uint8_t occurrence) {
  if (occurrence == 0) {
    snprintf(out, outSize, "%s", base);
  } else {
    snprintf(out, outSize, "%s_%u", base, (unsigned int)occurrence);
  }
}

static void appendTelemetryNumber(String& out,
                                  const char* field,
                                  uint8_t occurrence,
                                  double value,
                                  uint8_t decimals,
                                  bool& first) {
  char key[32];
  makeTelemetryField(key, sizeof(key), field, occurrence);
  appendJsonNumber(out, key, value, decimals, first);
}

static void appendTelemetryUInt(String& out,
                                const char* field,
                                uint8_t occurrence,
                                uint32_t value,
                                bool& first) {
  char key[32];
  makeTelemetryField(key, sizeof(key), field, occurrence);
  appendJsonUInt(out, key, value, first);
}

static void appendSensorJsonData(String& out,
                                 RuntimeSensor& entry,
                                 bool& first,
                                 TelemetryFieldCounters& counters) {
  if (!entry.lastReadOk) return;

  switch (entry.kind) {
    case KIND_RIKA_LEAF: {
      RikaLeafSensor* s = static_cast<RikaLeafSensor*>(entry.driver);
      const uint8_t occurrence = counters.leaf++;
      appendTelemetryNumber(out, "LeafT", occurrence, s->leaf_temp, 2, first);
      appendTelemetryNumber(out, "LeafH", occurrence, s->leaf_humid, 2, first);
      break;
    }
    case KIND_JXBS_LEAF_SURFACE: {
      JXBS_LeafSurfaceHumidity* s = static_cast<JXBS_LeafSurfaceHumidity*>(entry.driver);
      const uint8_t occurrence = counters.leaf++;
      appendTelemetryNumber(out, "LeafT", occurrence, s->leaf_temperature, 2, first);
      appendTelemetryNumber(out, "LeafH", occurrence, s->leaf_humidity, 2, first);
      break;
    }
    case KIND_SMALL_LEAF: {
      SmallLeafTemperatureHumidity* s = static_cast<SmallLeafTemperatureHumidity*>(entry.driver);
      const uint8_t occurrence = counters.leaf++;
      appendTelemetryNumber(out, "LeafT", occurrence, s->leaf_temperature, 2, first);
      appendTelemetryNumber(out, "LeafH", occurrence, s->leaf_humidity, 2, first);
      break;
    }
    case KIND_RIKA_SOIL3IN1: {
      RikaSoilSensor3in1* s = static_cast<RikaSoilSensor3in1*>(entry.driver);
      const uint8_t occurrence = counters.soil++;
      appendTelemetryNumber(out, "SoilT", occurrence, s->soil_temp, 2, first);
      appendTelemetryNumber(out, "SoilVWC", occurrence, s->soil_vwc, 2, first);
      appendTelemetryNumber(out, "SoilEC", occurrence, s->soil_ec, 4, first);
      break;
    }
    case KIND_JXBS_SOIL7IN1: {
      JXBS_SoilComp7in1* s = static_cast<JXBS_SoilComp7in1*>(entry.driver);
      const uint8_t occurrence = counters.soil++;
      appendTelemetryNumber(out, "SoilT", occurrence, s->soil_temp, 2, first);
      appendTelemetryNumber(out, "SoilVWC", occurrence, s->soil_moisture, 2, first);
      appendTelemetryNumber(out, "SoilEC", occurrence, s->soil_ec, 2, first);
      appendTelemetryNumber(out, "SoilPH", occurrence, s->soil_ph, 2, first);
      appendTelemetryUInt(out, "SoilN", occurrence, s->soil_nitrogen, first);
      appendTelemetryUInt(out, "SoilP", occurrence, s->soil_phosphorus, first);
      appendTelemetryUInt(out, "SoilK", occurrence, s->soil_potassium, first);
      break;
    }
    case KIND_WIND_SPEED: {
      JXCT_WindSpeed* s = static_cast<JXCT_WindSpeed*>(entry.driver);
      appendTelemetryNumber(out, "WindS", counters.windSpeed++, s->wind_speed_m_s, 2, first);
      break;
    }
    case KIND_WIND_DIRECTION: {
      JXCT_WindDirection* s = static_cast<JXCT_WindDirection*>(entry.driver);
      appendTelemetryNumber(out, "WindD", counters.windDirection++, s->wind_direction_deg, 1, first);
      break;
    }
    case KIND_UV_RAYS: {
      JXCT_UVRays* s = static_cast<JXCT_UVRays*>(entry.driver);
      appendTelemetryNumber(out, "UV", counters.uv++, s->uv_w_m2, 2, first);
      break;
    }
    case KIND_PAR: {
      JXCT_PAR* s = static_cast<JXCT_PAR*>(entry.driver);
      appendTelemetryNumber(out, "PAR", counters.par++, s->par_value, 2, first);
      break;
    }
    case KIND_TOTAL_SOLAR: {
      JXCT_TotalSolarRadiation* s = static_cast<JXCT_TotalSolarRadiation*>(entry.driver);
      appendTelemetryNumber(out, "SolarR", counters.totalSolar++, s->total_solar_w_m2, 2, first);
      break;
    }
    case KIND_EVAPORATION: {
      JXCT_Evaporation* s = static_cast<JXCT_Evaporation*>(entry.driver);
      appendTelemetryNumber(out, "EvapWeight", counters.evaporation++, s->evaporation_value, 2, first);
      break;
    }
    case KIND_WATER_PH: {
      JXBS_WaterPH* s = static_cast<JXBS_WaterPH*>(entry.driver);
      appendTelemetryNumber(out, "WaterT", counters.waterTemperature++, s->water_temperature, 2, first);
      appendTelemetryNumber(out, "WaterPH", counters.waterPH++, s->water_ph, 2, first);
      break;
    }
    case KIND_WATER_EC: {
      JXBS_WaterConductivity* s = static_cast<JXBS_WaterConductivity*>(entry.driver);
      appendTelemetryNumber(out, "WaterT", counters.waterTemperature++, s->water_temperature_C, 2, first);
      appendTelemetryNumber(out, "WaterEC", counters.waterEC++, s->conductivity_uS_cm, 2, first);
      break;
    }
    case KIND_WATER_SS: {
      JXSZ_WaterSuspendedSolids* s = static_cast<JXSZ_WaterSuspendedSolids*>(entry.driver);
      appendTelemetryNumber(out, "WaterT", counters.waterTemperature++, s->water_temperature_C, 2, first);
      appendTelemetryNumber(out, "WaterSS", counters.waterSS++, s->suspended_solids_mg_L, 2, first);
      break;
    }
    case KIND_AIR_QUALITY_SHIELD: {
      JXCT_AirQualityShield* s = static_cast<JXCT_AirQualityShield*>(entry.driver);
      appendTelemetryNumber(out, "AirT", counters.airTemperature++, s->air_temperature_C, 2, first);
      appendTelemetryNumber(out, "AirH", counters.airHumidity++, s->humidity_percent, 2, first);
      appendTelemetryNumber(out, "PM2_5", counters.pm25++, s->pm2_5_ug_m3, 2, first);
      appendTelemetryNumber(out, "PM10", counters.pm10++, s->pm10_ug_m3, 2, first);
      appendTelemetryNumber(out, "TVOC", counters.tvoc++, s->tvoc_ppb, 2, first);
      break;
    }
    case KIND_GAS_O3_CO_NH3_SHIELD: {
      JXBS_GasO3CONH3Shield* s = static_cast<JXBS_GasO3CONH3Shield*>(entry.driver);
      appendTelemetryNumber(out, "CO", counters.co++, s->co_ppm, 2, first);
      appendTelemetryNumber(out, "O3", counters.o3++, s->o3_ppm, 2, first);
      appendTelemetryNumber(out, "NH3", counters.nh3++, s->nh3_ppm, 2, first);
      break;
    }
    case KIND_GAS_SO2_NO2_PRESSURE_SHIELD: {
      JXBS_GasSO2NO2PressureShield* s = static_cast<JXBS_GasSO2NO2PressureShield*>(entry.driver);
      appendTelemetryNumber(out, "SO2", counters.so2++, s->so2_ppm, 2, first);
      appendTelemetryNumber(out, "NO2", counters.no2++, s->no2_ppm, 2, first);
      appendTelemetryNumber(out, "AtmP", counters.atmosphericPressure++, s->pressure_mbar, 2, first);
      break;
    }
  }
}

static String buildJsonPayload() {
  char dateTime[24] = "";
  currentDateTimeUTC(dateTime, sizeof(dateTime));

  double batteryVoltage = 0.0;
  const bool hasBattery = readBatteryVoltage(batteryVoltage);

  String out;
  out.reserve(2048);
  out += '{';

  bool firstRoot = true;
  appendJsonKey(out, "station_id", firstRoot);
  appendJsonEscaped(out, STATION_ID);
  appendJsonUInt(out, "timestamp", currentUnixTime(), firstRoot);
  appendJsonKey(out, "datetime_utc", firstRoot);
  appendJsonEscaped(out, dateTime);
  if (hasBattery) {
    appendJsonNumber(out, "battery_v", batteryVoltage, 2, firstRoot);
  }

  appendJsonKey(out, "flags", firstRoot);
  out += '{';
  bool firstFlag = true;
  for (size_t i = 0; i < g_runtimeSensorCount; ++i) {
    appendJsonBool(out, g_runtimeSensors[i].key, g_runtimeSensors[i].lastReadOk, firstFlag);
  }
  out += '}';

  appendJsonKey(out, "data", firstRoot);
  out += '{';
  bool firstData = true;
  TelemetryFieldCounters counters = {};
  if (hasBattery) appendJsonNumber(out, "Batt", batteryVoltage, 2, firstData);
  for (size_t i = 0; i < g_runtimeSensorCount; ++i) {
    appendSensorJsonData(out, g_runtimeSensors[i], firstData, counters);
  }
  out += '}';

  out += '}';
  return out;
}

static void appendSensorStringData(String& out,
                                   RuntimeSensor& entry,
                                   bool& first,
                                   TelemetryFieldCounters& counters) {
  if (!entry.lastReadOk) return;

  String json = "";
  bool firstJson = true;
  appendSensorJsonData(json, entry, firstJson, counters);

  json.replace("\"", "");
  json.replace(":", "=");
  appendStringField(out, entry.key, String("ok"), first);

  int start = 0;
  while (start < (int)json.length()) {
    int comma = json.indexOf(',', start);
    if (comma < 0) comma = json.length();
    String token = json.substring(start, comma);
    const int eq = token.indexOf('=');
    if (eq > 0) {
      appendStringField(out, token.substring(0, eq).c_str(), token.substring(eq + 1), first);
    }
    start = comma + 1;
  }
}

static String buildLegacyStringPayload() {
  String out = String(F("meteometric,stationID=")) + STATION_ID + " ";
  bool first = true;

  double batteryVoltage = 0.0;
  if (readBatteryVoltage(batteryVoltage)) {
    appendStringField(out, "Batt", String(batteryVoltage, 2), first);
  }

  TelemetryFieldCounters counters = {};
  for (size_t i = 0; i < g_runtimeSensorCount; ++i) {
    appendSensorStringData(out, g_runtimeSensors[i], first, counters);
  }

  out += ' ';
  out += String(currentUnixTime());
  out += F("000000000");
  return out;
}

static void publishTelemetry() {
  if (!MQTT_ENABLED) return;

  if (TELEMETRY_ENABLE_JSON_PAYLOAD) {
    const String payload = buildJsonPayload();
    if (TELEMETRY_PRINT_PAYLOADS) {
      printer.print(F("[PAYLOAD][JSON] "), true);
      printer.println(payload.c_str(), true);
    }
    publishMqtt(MQTT_JSON_TOPIC, payload);
  }

  if (TELEMETRY_ENABLE_STRING_PAYLOAD) {
    const String payload = buildLegacyStringPayload();
    if (TELEMETRY_PRINT_PAYLOADS) {
      printer.print(F("[PAYLOAD][STRING] "), true);
      printer.println(payload.c_str(), true);
    }
    publishMqtt(MQTT_STRING_TOPIC, payload);
  }
}

static void runStationCycle() {
  printer.println(F(""), true);
  printer.println(F("============================================================"), true);
  printer.println(F("[CYCLE] Starting station read/upload cycle"), true);
  printer.println(F("============================================================"), true);

  syncTimeFromInternet(false);
  readConfiguredSensors();
  publishTelemetry();

  g_lastCycleMs = millis();
}

void setup() {
  DebugPort.begin(PCB_DEBUG_SERIAL_BAUD);
  delay(300);

  printBanner();
  initPowerLines();
  initInterfaces();

  rs485Bus0.setDebug(&printer);
  rs485Bus0.begin(RS485Hw0,
                  RS485_DEFAULT_BAUD,
                  PCB_RS485_RX_PINS[RS485_PORT_INDEX_0],
                  PCB_RS485_TX_PINS[RS485_PORT_INDEX_0],
                  RS485_DEFAULT_SERIAL_CONFIG);
  rs485Bus0.setDirectionControl(PCB_RS485_DE_PINS[RS485_PORT_INDEX_0],
                                PCB_RS485_DE_ACTIVE_HIGH[RS485_PORT_INDEX_0]);

  buildRuntimeSensors();
  printSensorMap();

  syncTimeFromInternet(true);
  g_lastCycleMs = millis() - TELEMETRY_CYCLE_INTERVAL_MS;
}

void loop() {
  if ((millis() - g_lastCycleMs) >= TELEMETRY_CYCLE_INTERVAL_MS) {
    runStationCycle();
  }

  if (TIME_SYNC_ENABLED && (millis() - g_lastTimeSyncMs) >= TIME_SYNC_INTERVAL_MS) {
    syncTimeFromInternet(true);
  }

  if (mqttClient.connected()) {
    mqttClient.loop();
  }

  delay(1000);
}
