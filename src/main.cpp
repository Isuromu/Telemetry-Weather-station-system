#include <Arduino.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include <WiFi.h>
#include <Wire.h>
#include <PubSubClient.h>
#include <esp_system.h>
#include <esp_sleep.h>
#include <esp_task_wdt.h>

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
#include "SimpleOTA.h"
#include "SimpleDS3231.h"

#include "RikaLeafSensor.h"
#include "RikaSoilSensor3in1.h"
#include "JXBS_LeafSurfaceHumidity.h"
#include "SmallLeafTemperatureHumidity.h"
#include "JXCT_TemperatureHumidity.h"
#include "JXCT_AtmosphericPressure.h"
#include "JXBS_SoilComp7in1.h"
#include "JXBS_WaterPH.h"
#include "JXBS_WaterConductivity.h"
#include "JXSZ_WaterSuspendedSolids.h"
#include "JXCT_AirQualityShield.h"
#include "JXCT_WindSpeed.h"
#include "JXCT_WindDirection.h"
#include "JXCT_UVRays.h"
#include "JXCT_PAR.h"
#include "JXCT_TotalSolarRadiation.h"
#include "JXCT_Evaporation.h"
#include "JXBS_OpticalRainGauge.h"
#include "JXBS_PM25PM10Standalone.h"
#include "JXBS_GasSensor.h"
#include "JXBS_GasO3CONH3Shield.h"
#include "JXBS_GasSO2NO2PressureShield.h"

#ifndef PCB_DS3231_ADDRESS
#define PCB_DS3231_ADDRESS 0x68
#endif

// ============================================================
// Debug and transport ports
// ============================================================

auto& DebugPort = PCB_DEBUG_SERIAL_PORT;
HardwareSerial RS485Hw0(1);

static PrintController printer(DebugPort, false);
static RS485Bus rs485Bus0;
static WiFiClient wifiClient;
static PubSubClient mqttClient(wifiClient);
static SimpleOTA ota;
static SimpleDS3231 ds3231(PCB_DS3231_ADDRESS);

// ============================================================
// Runtime registry
// ============================================================

enum SensorKind : uint8_t {
  KIND_RIKA_LEAF,
  KIND_JXBS_LEAF_SURFACE,
  KIND_SMALL_LEAF,
  KIND_TEMPERATURE_HUMIDITY,
  KIND_ATMOSPHERIC_PRESSURE,
  KIND_RIKA_SOIL3IN1,
  KIND_JXBS_SOIL7IN1,
  KIND_WIND_SPEED,
  KIND_WIND_DIRECTION,
  KIND_UV_RAYS,
  KIND_PAR,
  KIND_TOTAL_SOLAR,
  KIND_EVAPORATION,
  KIND_OPTICAL_RAIN,
  KIND_WATER_PH,
  KIND_WATER_EC,
  KIND_WATER_SS,
  KIND_AIR_QUALITY_SHIELD,
  KIND_PM25_PM10_STANDALONE,
  KIND_GAS_CO,
  KIND_GAS_O3,
  KIND_GAS_NH3,
  KIND_GAS_SO2,
  KIND_GAS_NO2,
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
static bool g_networkPowerState = false;

static uint32_t g_lastCycleMs = 0;
static uint32_t g_lastTimeSyncMs = 0;
static bool g_timeIsSynced = false;
static bool g_i2cStarted = false;
static bool g_rtcAvailable = false;

static const uint32_t RETAINED_STATE_MAGIC = 0x54574D31UL; // "TWM1"
RTC_DATA_ATTR uint32_t g_retainedStateMagic = 0;
RTC_DATA_ATTR uint32_t g_lastNetworkRtcSyncEpoch = 0;
RTC_DATA_ATTR uint32_t g_deepSleepCycleCount = 0;

struct ErrorLog {
  char code[32];
  char message[96];
  char sensorId[32];
  uint8_t address;
  uint8_t consecutiveErrors;
};

static const size_t MAX_ERROR_LOGS = 16;
static ErrorLog g_errorLogs[MAX_ERROR_LOGS];
static size_t g_errorLogCount = 0;
static size_t g_errorLogWriteIndex = 0;

// RTC memory survives watchdog resets. We keep the last work block name here
// so the next boot can report where the firmware was when it stopped feeding.
static const uint32_t WATCHDOG_STAGE_MAGIC = 0x57445431UL; // "WDT1"
static const uint32_t STAGE_TIMEOUT_MAGIC = 0x53544731UL; // "STG1"
static const size_t WATCHDOG_STAGE_TEXT_SIZE = 48;
RTC_DATA_ATTR uint32_t g_watchdogStageMagic = 0;
RTC_DATA_ATTR char g_watchdogStage[WATCHDOG_STAGE_TEXT_SIZE] = "";
RTC_DATA_ATTR uint32_t g_stageTimeoutMagic = 0;
RTC_DATA_ATTR char g_stageTimeoutStage[WATCHDOG_STAGE_TEXT_SIZE] = "";
RTC_DATA_ATTR uint32_t g_stageTimeoutElapsedMs = 0;
static uint32_t g_watchdogStageStartedMs = 0;
static bool g_stageTimeoutRestarting = false;
static uint32_t g_lastLoopHeartbeatMs = 0;

// ============================================================
// Utility helpers
// ============================================================

static void flushLog() {
  printer.flush();
}

static const char* resetReasonName(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_POWERON: return "POWERON";
    case ESP_RST_EXT: return "EXT";
    case ESP_RST_SW: return "SW";
    case ESP_RST_PANIC: return "PANIC";
    case ESP_RST_INT_WDT: return "INT_WDT";
    case ESP_RST_TASK_WDT: return "TASK_WDT";
    case ESP_RST_WDT: return "WDT";
    case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
    case ESP_RST_BROWNOUT: return "BROWNOUT";
    case ESP_RST_SDIO: return "SDIO";
    default: return "UNKNOWN";
  }
}

static void beginDebugSerial() {
  DebugPort.begin(PCB_DEBUG_SERIAL_BAUD);

  const uint32_t start = millis();
  DebugPort.println();
  DebugPort.println("[BOOT] Serial started");
  DebugPort.print("[BOOT] Baud: ");
  DebugPort.println((unsigned long)PCB_DEBUG_SERIAL_BAUD);
  DebugPort.flush();

  while ((millis() - start) < DEBUG_SERIAL_WAIT_MS) {
    DebugPort.print("[BOOT] Waiting for monitor, ms=");
    DebugPort.println((unsigned long)(millis() - start));
    DebugPort.flush();
    delay(500);
  }

  DebugPort.println("[BOOT] Continuing firmware startup");
  DebugPort.flush();
}

static void printResetReason() {
  const esp_reset_reason_t reason = esp_reset_reason();
  printer.print(F("[BOOT] Reset reason: "), true);
  printer.print(resetReasonName(reason), true);
  printer.print(F(" ("), true);
  printer.print((int)reason, true);
  printer.println(F(")"), true);
  flushLog();
}

static void printBanner() {
  printer.println(F(""), true);
  printer.println(F("============================================================"), true);
  printer.println(F(" Telemetry Weather Station Main Firmware"), true);
  printer.println(F("============================================================"), true);
  printer.print(F("Station ID: "), true);
  printer.println(STATION_ID, true);
  printer.print(F("Station name: "), true);
  printer.println(STATION_NAME, true);
  printer.print(F("Firmware version: "), true);
  printer.println(FIRMWARE_VERSION, true);
  printer.print(F("PCB: "), true);
  printer.println(PCB_NAME, true);
  printer.print(F("MQTT JSON topic: "), true);
  printer.println(MQTT_JSON_TOPIC, true);
  printer.print(F("Upload rate min: "), true);
  printer.println((unsigned int)UPLOAD_RATE_MIN, true);
  printer.print(F("Watchdog timeout s: "), true);
  printer.println((unsigned int)WATCHDOG_TIMEOUT_SECONDS, true);
  printer.print(F("Stage hard timeout ms: "), true);
  printer.println((unsigned long)STATION_STAGE_HARD_TIMEOUT_MS, true);
  printer.print(F("Sleep after cycle: "), true);
  printer.println(SLEEP_AFTER_CYCLE_ENABLED ? F("yes") : F("no"), true);
  printer.print(F("RTC DS3231 enabled: "), true);
  printer.println(RTC_DS3231_ENABLED ? F("yes") : F("no"), true);
  printer.print(F("Wi-Fi enabled: "), true);
  printer.println(WIFI_ENABLED ? F("yes") : F("no"), true);
  printer.print(F("MQTT host: "), true);
  printer.print(MQTT_HOST, true);
  printer.print(F(":"), true);
  printer.println((unsigned int)MQTT_PORT, true);
  printer.println(F(""), true);
  flushLog();
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

  driver->setUploadRate(UPLOAD_RATE_MIN);
  driver->setWarmUpTimeMs(STATION_SENSOR_WARMUP_MS);

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
      if (*p == '"' || *p == '\\') {
        out += '\\';
        out += *p;
      } else if (*p == '\n') {
        out += F("\\n");
      } else if (*p == '\r') {
        out += F("\\r");
      } else if (*p == '\t') {
        out += F("\\t");
      } else {
        out += *p;
      }
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

static void appendJsonString(String& out, const char* key, const char* value, bool& first) {
  appendJsonKey(out, key, first);
  appendJsonEscaped(out, value);
}

/*
  rememberError(code, message, sensorId, address, consecutiveErrors)
  Accepts:
    code              - short server-readable error code
    message           - human-readable explanation
    sensorId          - optional sensor ID, empty for station/network errors
    address           - optional Modbus address
    consecutiveErrors - optional sensor error counter
  Returns: nothing.

  Errors are kept in a small RAM ring. They are included in the next JSON
  upload and cleared only after MQTT publish succeeds.
*/
static void rememberError(const char* code,
                          const char* message,
                          const char* sensorId = "",
                          uint8_t address = 0,
                          uint8_t consecutiveErrors = 0) {
  ErrorLog& slot = g_errorLogs[g_errorLogWriteIndex];
  strlcpy(slot.code, code ? code : "ERROR", sizeof(slot.code));
  strlcpy(slot.message, message ? message : "", sizeof(slot.message));
  strlcpy(slot.sensorId, sensorId ? sensorId : "", sizeof(slot.sensorId));
  slot.address = address;
  slot.consecutiveErrors = consecutiveErrors;

  if (g_errorLogCount < MAX_ERROR_LOGS) {
    ++g_errorLogCount;
  }
  g_errorLogWriteIndex = (g_errorLogWriteIndex + 1) % MAX_ERROR_LOGS;
}

static void clearRememberedErrors() {
  g_errorLogCount = 0;
  g_errorLogWriteIndex = 0;
}

static void initRetainedState() {
  if (g_retainedStateMagic == RETAINED_STATE_MAGIC) {
    return;
  }

  g_retainedStateMagic = RETAINED_STATE_MAGIC;
  g_lastNetworkRtcSyncEpoch = 0;
  g_deepSleepCycleCount = 0;
}

static bool setSystemClockFromEpoch(uint32_t epochSeconds) {
  if (epochSeconds < 1577836800UL) {
    return false;
  }

  timeval tv = {};
  tv.tv_sec = (time_t)epochSeconds;
  tv.tv_usec = 0;
  return settimeofday(&tv, nullptr) == 0;
}

static void initI2CBus() {
  if (g_i2cStarted) {
    return;
  }

  Wire.begin(PCB_I2C_SDA_PIN, PCB_I2C_SCL_PIN);
  Wire.setClock(100000UL);
  ds3231.begin(Wire);
  g_i2cStarted = true;

  printer.print(F("[I2C] Started SDA="), true);
  printer.print((int)PCB_I2C_SDA_PIN, true);
  printer.print(F(" SCL="), true);
  printer.println((int)PCB_I2C_SCL_PIN, true);
}

static bool readRtcIntoSystemClock() {
  if (!RTC_DS3231_ENABLED) {
    return false;
  }

  initI2CBus();

  SimpleDS3231DateTime rtcNow = {};
  if (!ds3231.read(rtcNow)) {
    g_rtcAvailable = false;
    rememberError("RTC_READ_FAILED", "DS3231 time is unavailable or not valid");
    printer.println(F("[RTC] DS3231 read failed"), true);
    return false;
  }

  if (!setSystemClockFromEpoch(rtcNow.epochSeconds)) {
    rememberError("RTC_SYSTEM_TIME_FAILED", "could not copy DS3231 time to ESP32 clock");
    printer.println(F("[RTC] Could not set ESP32 system time"), true);
    return false;
  }

  char text[24] = "";
  SimpleDS3231::formatUtc(rtcNow.epochSeconds, text, sizeof(text));
  g_rtcAvailable = true;
  g_timeIsSynced = true;

  printer.print(F("[RTC] DS3231 UTC: "), true);
  printer.println(text, true);
  return true;
}

static bool writeSystemClockToRtc() {
  if (!RTC_DS3231_ENABLED) {
    return false;
  }

  const time_t now = time(nullptr);
  if (now < 1577836800UL) {
    return false;
  }

  initI2CBus();
  const bool ok = ds3231.setFromEpoch((uint32_t)now);
  if (ok) {
    g_rtcAvailable = true;
    g_lastNetworkRtcSyncEpoch = (uint32_t)now;
    printer.println(F("[RTC] DS3231 updated from network time"), true);
  } else {
    rememberError("RTC_SET_FAILED", "could not write network time to DS3231");
    printer.println(F("[RTC] DS3231 update failed"), true);
  }
  return ok;
}

static bool networkTimeSyncIsDue(bool force) {
  if (!TIME_SYNC_ENABLED) {
    return false;
  }
  if (force) {
    return true;
  }

  const time_t now = time(nullptr);
  if (!g_timeIsSynced || now < 1577836800UL) {
    return true;
  }
  if (g_lastNetworkRtcSyncEpoch == 0) {
    return true;
  }

  const uint32_t elapsedSeconds = (uint32_t)now - g_lastNetworkRtcSyncEpoch;
  return elapsedSeconds >= (RTC_NETWORK_SYNC_INTERVAL_MS / 1000UL);
}

/*
  appendErrorLogsJson(out, firstRoot)
  Accepts:
    out       - JSON string being built
    firstRoot - root-object comma state
  Returns: nothing.

  Adds "logs": [...] to the station JSON. Empty logs are sent as an empty array
  so the server always receives a stable shape.
*/
static void appendErrorLogsJson(String& out, bool& firstRoot) {
  appendJsonKey(out, "logs", firstRoot);
  out += '[';

  for (size_t i = 0; i < g_errorLogCount; ++i) {
    if (i > 0) out += ',';
    out += '{';

    bool firstLog = true;
    appendJsonString(out, "level", "error", firstLog);
    appendJsonString(out, "code", g_errorLogs[i].code, firstLog);
    appendJsonString(out, "message", g_errorLogs[i].message, firstLog);
    if (g_errorLogs[i].sensorId[0] != '\0') {
      appendJsonString(out, "sensor", g_errorLogs[i].sensorId, firstLog);
      appendJsonUInt(out, "address", g_errorLogs[i].address, firstLog);
      appendJsonUInt(out, "consecutive_errors", g_errorLogs[i].consecutiveErrors, firstLog);
    }

    out += '}';
  }

  out += ']';
}

// ============================================================
// Simple watchdog
// ============================================================

/*
  setWatchdogStage(stage)
  Accepts: short text that describes the block currently running.
  Returns: nothing.

  The text is saved in RTC memory. If the watchdog resets ESP32, the next boot
  can include this stage in the JSON error log.
*/
static void setWatchdogStage(const char* stage) {
  const char* stageText = stage ? stage : "unknown";
  g_watchdogStageStartedMs = millis();

  if (WATCHDOG_ENABLED) {
    g_watchdogStageMagic = WATCHDOG_STAGE_MAGIC;
  }
  strlcpy(g_watchdogStage, stageText, sizeof(g_watchdogStage));

  printer.print(F("[STAGE] +"), true);
  printer.print((unsigned long)g_watchdogStageStartedMs, true);
  printer.print(F(" ms -> "), true);
  printer.println(stageText, true);
  flushLog();
}

static bool currentStageIsGuarded() {
  if (g_watchdogStage[0] == '\0') return false;
  if (strcmp(g_watchdogStage, "idle") == 0) return false;
  if (strcmp(g_watchdogStage, "deep_sleep") == 0) return false;
  return true;
}

static void restartForStageTimeout(uint32_t elapsedMs) {
  if (g_stageTimeoutRestarting) {
    return;
  }
  g_stageTimeoutRestarting = true;

  g_stageTimeoutMagic = STAGE_TIMEOUT_MAGIC;
  strlcpy(g_stageTimeoutStage, g_watchdogStage, sizeof(g_stageTimeoutStage));
  g_stageTimeoutElapsedMs = elapsedMs;

  printer.print(F("[FATAL] Stage timeout, restarting. stage="), true);
  printer.print(g_watchdogStage, true);
  printer.print(F(" elapsed_ms="), true);
  printer.println((unsigned long)elapsedMs, true);
  flushLog();

  delay(100);
  ESP.restart();
}

static void checkStageDeadline() {
  if (!currentStageIsGuarded() || g_watchdogStageStartedMs == 0) {
    return;
  }

  const uint32_t elapsedMs = millis() - g_watchdogStageStartedMs;
  if (elapsedMs > STATION_STAGE_HARD_TIMEOUT_MS) {
    restartForStageTimeout(elapsedMs);
  }
}

/*
  feedWatchdog()
  Accepts: nothing.
  Returns: nothing.

  Call this after every normal work block. If code hangs inside a block and this
  is not called before WATCHDOG_TIMEOUT_SECONDS, ESP32 reboots.
*/
static void feedWatchdog() {
  checkStageDeadline();
  if (!WATCHDOG_ENABLED) return;

  #if defined(ARDUINO_ARCH_ESP32)
    esp_task_wdt_reset();
  #endif
}

/*
  waitWithWatchdog(durationMs)
  Accepts: wait time in milliseconds.
  Returns: nothing.

  This replaces long delay() calls in station code so planned waiting does not
  look like a firmware hang.
*/
static void waitWithWatchdog(uint32_t durationMs) {
  const uint32_t start = millis();
  while ((millis() - start) < durationMs) {
    const uint32_t elapsed = millis() - start;
    const uint32_t remaining = durationMs - elapsed;
    delay(remaining > 250UL ? 250UL : remaining);
    feedWatchdog();
  }
}

static bool resetReasonWasWatchdog(esp_reset_reason_t reason) {
  return reason == ESP_RST_WDT ||
         reason == ESP_RST_TASK_WDT ||
         reason == ESP_RST_INT_WDT;
}

/*
  rememberWatchdogResetIfNeeded()
  Accepts: nothing.
  Returns: nothing.

  If the previous boot ended by watchdog, this adds a WATCHDOG_RESET entry to
  the next MQTT JSON payload. The RTC stage is then cleared to avoid duplicates.
*/
static void rememberWatchdogResetIfNeeded() {
  if (g_stageTimeoutMagic == STAGE_TIMEOUT_MAGIC) {
    char message[96];
    snprintf(message,
             sizeof(message),
             "stage timeout reset while running: %s (%lu ms)",
             g_stageTimeoutStage,
             (unsigned long)g_stageTimeoutElapsedMs);
    rememberError("STAGE_TIMEOUT_RESET", message);
    printer.print(F("[BOOT] Previous stage timeout: "), true);
    printer.println(message, true);
    flushLog();
    g_stageTimeoutMagic = 0;
    g_stageTimeoutStage[0] = '\0';
    g_stageTimeoutElapsedMs = 0;
  }

  const esp_reset_reason_t reason = esp_reset_reason();
  if (!resetReasonWasWatchdog(reason)) {
    return;
  }

  char message[96];
  if (g_watchdogStageMagic == WATCHDOG_STAGE_MAGIC && g_watchdogStage[0] != '\0') {
    snprintf(message, sizeof(message), "watchdog reset while running: %s", g_watchdogStage);
  } else {
    strlcpy(message, "watchdog reset on previous boot", sizeof(message));
  }

  rememberError("WATCHDOG_RESET", message);
  g_watchdogStageMagic = 0;
  g_watchdogStage[0] = '\0';
}

/*
  startWatchdog()
  Accepts: nothing.
  Returns: nothing.

  Starts the ESP32 task watchdog for the main Arduino loop task.
*/
static void startWatchdog() {
  if (!WATCHDOG_ENABLED) return;

  #if defined(ARDUINO_ARCH_ESP32)
    setWatchdogStage("setup");
    esp_task_wdt_init(WATCHDOG_TIMEOUT_SECONDS, true);
    esp_task_wdt_add(NULL);
    feedWatchdog();

    printer.print(F("[WDT] Started, timeout seconds: "), true);
    printer.println((unsigned int)WATCHDOG_TIMEOUT_SECONDS, true);
    flushLog();
  #endif
}

// ============================================================
// Power and RS485 interface control
// ============================================================

static void initPowerLines() {
  printer.print(F("[HW] Power line count: "), true);
  printer.println((unsigned int)PCB_POWERLINE_COUNT, true);
  for (uint8_t i = 0; i < PCB_POWERLINE_COUNT; ++i) {
    const int8_t pin = PCB_POWERLINE_SWITCH_PINS[i];
    printer.print(F("[HW] Power line "), true);
    printer.print((unsigned int)i, true);
    printer.print(F(" switch_pin="), true);
    printer.print((int)pin, true);
    printer.print(F(" active_high="), true);
    printer.println(PCB_POWERLINE_ACTIVE_HIGH[i] ? F("yes") : F("no"), true);
    if (pin >= 0) {
      pinMode(pin, OUTPUT);
      const bool activeHigh = PCB_POWERLINE_ACTIVE_HIGH[i];
      digitalWrite(pin, activeHigh ? LOW : HIGH);
    }
    g_powerLineState[i] = false;
  }
  flushLog();
}

static void initInterfaces() {
  printer.print(F("[HW] RS485 interface count: "), true);
  printer.println((unsigned int)PCB_RS485_PORT_COUNT, true);
  for (uint8_t i = 0; i < PCB_RS485_PORT_COUNT; ++i) {
    const int8_t enPin = PCB_RS485_ENABLE_PINS[i];
    printer.print(F("[HW] RS485 "), true);
    printer.print((unsigned int)i, true);
    printer.print(F(" rx="), true);
    printer.print((int)PCB_RS485_RX_PINS[i], true);
    printer.print(F(" tx="), true);
    printer.print((int)PCB_RS485_TX_PINS[i], true);
    printer.print(F(" de="), true);
    printer.print((int)PCB_RS485_DE_PINS[i], true);
    printer.print(F(" enable="), true);
    printer.println((int)enPin, true);
    if (enPin >= 0) {
      pinMode(enPin, OUTPUT);
      const bool activeHigh = PCB_RS485_ENABLE_ACTIVE_HIGH[i];
      digitalWrite(enPin, activeHigh ? LOW : HIGH);
    }
    g_rs485InterfaceState[i] = false;
  }
  flushLog();
}

static void initNetworkPowerControl() {
  if (!NETWORK_POWER_CONTROL_ENABLED || NETWORK_POWER_PIN < 0) {
    g_networkPowerState = true;
    printer.println(F("[NET] Network power control disabled"), true);
    flushLog();
    return;
  }

  pinMode(NETWORK_POWER_PIN, OUTPUT);
  digitalWrite(NETWORK_POWER_PIN, NETWORK_POWER_ACTIVE_HIGH ? LOW : HIGH);
  g_networkPowerState = false;
  printer.print(F("[NET] Network power pin initialized: "), true);
  printer.println((int)NETWORK_POWER_PIN, true);
  flushLog();
}

static void networkPowerSet(bool on) {
  if (!NETWORK_POWER_CONTROL_ENABLED || NETWORK_POWER_PIN < 0) {
    g_networkPowerState = true;
    return;
  }

  if (g_networkPowerState == on) {
    return;
  }

  digitalWrite(NETWORK_POWER_PIN,
               on ? (NETWORK_POWER_ACTIVE_HIGH ? HIGH : LOW)
                  : (NETWORK_POWER_ACTIVE_HIGH ? LOW : HIGH));
  g_networkPowerState = on;

  printer.print(on ? F("[NET] Network power ON") : F("[NET] Network power OFF"), true);
  printer.println("", true);
  flushLog();

  if (on && NETWORK_POWER_WARMUP_MS > 0) {
    printer.print(F("[NET] Waiting network device warmup ms: "), true);
    printer.println((unsigned long)NETWORK_POWER_WARMUP_MS, true);
    flushLog();
    waitWithWatchdog(NETWORK_POWER_WARMUP_MS);
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
  printer.print(F("[HW] Power line "), true);
  printer.print((unsigned int)index, true);
  printer.println(on ? F(" ON") : F(" OFF"), true);
  flushLog();
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
  printer.print(F("[HW] RS485 interface "), true);
  printer.print((unsigned int)index, true);
  printer.println(on ? F(" ON") : F(" OFF"), true);
  flushLog();
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

static void registerAirEnvironmentSensors() {
  for (uint8_t i = 0; i < JXCT_TEMPERATURE_HUMIDITY_COUNT; ++i) {
    char id[32];
    char key[32];
    makeIndexedName(id, sizeof(id), "air_th_", i, false);
    makeIndexedName(key, sizeof(key), "AirTH", i, true);

    addRuntimeSensor(new JXCT_TemperatureHumidity(rs485Bus0,
                                                  id,
                                                  JXCT_TEMPERATURE_HUMIDITY_ADDRESSES[i],
                                                  STATION_SENSOR_DEBUG,
                                                  STATION_SENSOR_POWERLINE,
                                                  STATION_SENSOR_RS485_PORT,
                                                  STATION_FAST_SAMPLE_RATE,
                                                  STATION_SENSOR_WARMUP_MS,
                                                  SENSOR_DEFAULT_MAX_ERRORS,
                                                  MIN_USEFUL_POWER_OFF_MS),
                     KIND_TEMPERATURE_HUMIDITY,
                     id,
                     key);
  }

  for (uint8_t i = 0; i < JXCT_ATMOSPHERIC_PRESSURE_COUNT; ++i) {
    char id[32];
    char key[32];
    makeIndexedName(id, sizeof(id), "atm_pressure_", i, false);
    makeIndexedName(key, sizeof(key), "AtmP", i, true);

    addRuntimeSensor(new JXCT_AtmosphericPressure(rs485Bus0,
                                                  id,
                                                  JXCT_ATMOSPHERIC_PRESSURE_ADDRESSES[i],
                                                  STATION_SENSOR_DEBUG,
                                                  STATION_SENSOR_POWERLINE,
                                                  STATION_SENSOR_RS485_PORT,
                                                  STATION_FAST_SAMPLE_RATE,
                                                  STATION_SENSOR_WARMUP_MS,
                                                  SENSOR_DEFAULT_MAX_ERRORS,
                                                  MIN_USEFUL_POWER_OFF_MS),
                     KIND_ATMOSPHERIC_PRESSURE,
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

  for (uint8_t i = 0; i < JXBS_OPTICAL_RAIN_GAUGE_COUNT; ++i) {
    char id[32]; char key[32];
    makeIndexedName(id, sizeof(id), "optical_rain_", i, false);
    makeIndexedName(key, sizeof(key), "Rain", i, true);
    addRuntimeSensor(new JXBS_OpticalRainGauge(rs485Bus0,
                                               id,
                                               JXBS_OPTICAL_RAIN_GAUGE_ADDRESSES[i],
                                               STATION_SENSOR_DEBUG),
                     KIND_OPTICAL_RAIN,
                     id,
                     key);
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

static void addSingleGasSensor(uint8_t index,
                               const char* idPrefix,
                               const char* keyPrefix,
                               SensorKind kind,
                               const char* gasName,
                               uint8_t address,
                               double scaleDivisor,
                               double maxGasPpm) {
  char id[32];
  char key[32];
  makeIndexedName(id, sizeof(id), idPrefix, index, false);
  makeIndexedName(key, sizeof(key), keyPrefix, index, true);

  addRuntimeSensor(new JXBS_GasSensor(rs485Bus0,
                                      id,
                                      gasName,
                                      address,
                                      scaleDivisor,
                                      maxGasPpm,
                                      STATION_SENSOR_DEBUG,
                                      STATION_SENSOR_POWERLINE,
                                      STATION_SENSOR_RS485_PORT,
                                      STATION_FAST_SAMPLE_RATE,
                                      STATION_SENSOR_WARMUP_MS,
                                      SENSOR_DEFAULT_MAX_ERRORS,
                                      MIN_USEFUL_POWER_OFF_MS),
                   kind,
                   id,
                   key);
}

static void registerSingleGasSensors() {
  for (uint8_t i = 0; i < JXBS_GAS_CO_COUNT; ++i) {
    addSingleGasSensor(i, "gas_co_", "CO", KIND_GAS_CO, "CO", JXBS_GAS_CO_ADDRESSES[i], 10.0, 2000.0);
  }
  for (uint8_t i = 0; i < JXBS_GAS_O3_COUNT; ++i) {
    addSingleGasSensor(i, "gas_o3_", "O3", KIND_GAS_O3, "O3", JXBS_GAS_O3_ADDRESSES[i], 100.0, 100.0);
  }
  for (uint8_t i = 0; i < JXBS_GAS_NH3_COUNT; ++i) {
    addSingleGasSensor(i, "gas_nh3_", "NH3", KIND_GAS_NH3, "NH3", JXBS_GAS_NH3_ADDRESSES[i], 10.0, 5000.0);
  }
  for (uint8_t i = 0; i < JXBS_GAS_SO2_COUNT; ++i) {
    addSingleGasSensor(i, "gas_so2_", "SO2", KIND_GAS_SO2, "SO2", JXBS_GAS_SO2_ADDRESSES[i], 10.0, 2000.0);
  }
  for (uint8_t i = 0; i < JXBS_GAS_NO2_COUNT; ++i) {
    addSingleGasSensor(i, "gas_no2_", "NO2", KIND_GAS_NO2, "NO2", JXBS_GAS_NO2_ADDRESSES[i], 10.0, 2000.0);
  }
}

static void registerShieldSensors() {
  for (uint8_t i = 0; i < JXCT_AIR_QUALITY_SHIELD_COUNT; ++i) {
    char id[32]; char key[32];
    makeIndexedName(id, sizeof(id), "air_quality_", i, false);
    makeIndexedName(key, sizeof(key), "AirQ", i, true);
    addRuntimeSensor(new JXCT_AirQualityShield(rs485Bus0, id, JXCT_AIR_QUALITY_SHIELD_ADDRESSES[i], STATION_SENSOR_DEBUG), KIND_AIR_QUALITY_SHIELD, id, key);
  }

  for (uint8_t i = 0; i < JXBS_PM25_PM10_STANDALONE_COUNT; ++i) {
    char id[32]; char key[32];
    makeIndexedName(id, sizeof(id), "pm25_pm10_", i, false);
    makeIndexedName(key, sizeof(key), "PM", i, true);
    addRuntimeSensor(new JXBS_PM25PM10Standalone(rs485Bus0,
                                                 id,
                                                 JXBS_PM25_PM10_STANDALONE_ADDRESSES[i],
                                                 STATION_SENSOR_DEBUG),
                     KIND_PM25_PM10_STANDALONE,
                     id,
                     key);
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
  registerAirEnvironmentSensors();
  registerRikaSoilSensors();
  registerJXBSSoilSensors();
  registerJXCTWeatherSensors();
  registerWaterSensors();
  registerSingleGasSensors();
  registerShieldSensors();
}

static void printSensorMap() {
  printer.println(F("Configured sensor map:"), true);
  if (g_runtimeSensorCount == 0) {
    printer.println(F("  <none>"), true);
    flushLog();
    return;
  }

  for (size_t i = 0; i < g_runtimeSensorCount; ++i) {
    SensorDriver* s = g_runtimeSensors[i].driver;
    printer.print(F("  #"), true);
    printer.print((unsigned int)i, true, " ");
    printer.print(g_runtimeSensors[i].id, true, " | key=");
    printer.print(g_runtimeSensors[i].key, true, " | addr=0x");
    printer.print((unsigned int)s->getAddress(), true, " | ", HEX);
    printer.print(F("uploadMin="), true);
    printer.println((unsigned int)s->getUploadRateMin(), true);
  }
  flushLog();
}

// ============================================================
// Time, Wi-Fi, MQTT, battery
// ============================================================

static bool ensureWifiConnected() {
  if (!WIFI_ENABLED) {
    printer.println(F("[NET] Wi-Fi disabled by configuration"), true);
    flushLog();
    return false;
  }
  if (WiFi.status() == WL_CONNECTED) {
    printer.print(F("[NET] Wi-Fi already connected, IP="), true);
    printer.print(WiFi.localIP().toString().c_str(), true);
    printer.print(F(" RSSI="), true);
    printer.println((int)WiFi.RSSI(), true);
    flushLog();
    return true;
  }

  setWatchdogStage("net:wifi_connect");
  networkPowerSet(true);

  printer.print(F("[NET] Connecting Wi-Fi SSID: "), true);
  printer.println(WIFI_SSID, true);
  printer.print(F("[NET] Wi-Fi timeout ms: "), true);
  printer.println((unsigned long)WIFI_CONNECT_TIMEOUT_MS, true);
  flushLog();

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  const uint32_t start = millis();
  uint32_t lastProgress = 0;
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < WIFI_CONNECT_TIMEOUT_MS) {
    const uint32_t elapsed = millis() - start;
    if ((elapsed - lastProgress) >= 1000UL || lastProgress == 0) {
      lastProgress = elapsed;
      printer.print(F("[NET] Wi-Fi wait elapsed_ms="), true);
      printer.print((unsigned long)elapsed, true);
      printer.print(F(" status="), true);
      printer.println((int)WiFi.status(), true);
      flushLog();
    }
    waitWithWatchdog(250);
  }

  if (WiFi.status() == WL_CONNECTED) {
    printer.print(F("[NET] Wi-Fi connected, IP: "), true);
    printer.print(WiFi.localIP().toString().c_str(), true);
    printer.print(F(" RSSI="), true);
    printer.print((int)WiFi.RSSI(), true);
    printer.print(F(" elapsed_ms="), true);
    printer.println((unsigned long)(millis() - start), true);
    flushLog();
    return true;
  }

  printer.println(F("[NET] Wi-Fi connection failed"), true);
  printer.print(F("[NET] Final Wi-Fi status: "), true);
  printer.println((int)WiFi.status(), true);
  flushLog();
  rememberError("WIFI_CONNECT_FAILED", "Wi-Fi connection failed");
  return false;
}

static bool internetLooksAvailable() {
  if (WiFi.status() != WL_CONNECTED) {
    printer.println(F("[NET] DNS skipped because Wi-Fi is disconnected"), true);
    flushLog();
    return false;
  }

  setWatchdogStage("net:dns_check");
  IPAddress resolved;
  printer.print(F("[NET] DNS check host: "), true);
  printer.println(TIME_NTP_SERVER_1, true);
  flushLog();
  const uint32_t start = millis();
  const bool ok = WiFi.hostByName(TIME_NTP_SERVER_1, resolved) == 1;
  if (!ok) {
    printer.println(F("[NET] DNS/internet check failed"), true);
    printer.print(F("[NET] DNS elapsed_ms="), true);
    printer.println((unsigned long)(millis() - start), true);
    flushLog();
    rememberError("INTERNET_CHECK_FAILED", "DNS/internet check failed");
  } else {
    printer.print(F("[NET] DNS OK: "), true);
    printer.print(resolved.toString().c_str(), true);
    printer.print(F(" elapsed_ms="), true);
    printer.println((unsigned long)(millis() - start), true);
    flushLog();
  }
  return ok;
}

static bool syncTimeFromInternet(bool force) {
  if (!TIME_SYNC_ENABLED) {
    printer.println(F("[TIME] Network time sync disabled"), true);
    flushLog();
    return false;
  }
  if (!networkTimeSyncIsDue(force)) {
    printer.println(F("[TIME] Network time sync not due"), true);
    flushLog();
    return true;
  }
  if (!ensureWifiConnected() || !internetLooksAvailable()) {
    return false;
  }

  setWatchdogStage("time:ntp_sync");
  printer.println(F("[TIME] Syncing UTC time from NTP"), true);
  printer.print(F("[TIME] NTP servers: "), true);
  printer.print(TIME_NTP_SERVER_1, true);
  printer.print(F(", "), true);
  printer.println(TIME_NTP_SERVER_2, true);
  flushLog();
  configTime(0, 0, TIME_NTP_SERVER_1, TIME_NTP_SERVER_2);

  struct tm timeInfo;
  const uint32_t start = millis();
  uint32_t lastProgress = 0;
  while ((millis() - start) < TIME_SYNC_TIMEOUT_MS) {
    if (getLocalTime(&timeInfo, 500)) {
      g_timeIsSynced = true;
      g_lastTimeSyncMs = millis();
      char text[24] = "";
      const time_t syncedNow = time(nullptr);
      if (syncedNow >= 1577836800UL) {
        SimpleDS3231::formatUtc((uint32_t)syncedNow, text, sizeof(text));
      }
      printer.print(F("[TIME] Time sync OK UTC="), true);
      printer.print(text, true);
      printer.print(F(" elapsed_ms="), true);
      printer.println((unsigned long)(millis() - start), true);
      flushLog();
      writeSystemClockToRtc();
      return true;
    }
    const uint32_t elapsed = millis() - start;
    if ((elapsed - lastProgress) >= 1000UL || lastProgress == 0) {
      lastProgress = elapsed;
      printer.print(F("[TIME] Waiting NTP elapsed_ms="), true);
      printer.println((unsigned long)elapsed, true);
      flushLog();
    }
    feedWatchdog();
  }

  printer.println(F("[TIME] Time sync failed"), true);
  printer.print(F("[TIME] Timeout ms: "), true);
  printer.println((unsigned long)TIME_SYNC_TIMEOUT_MS, true);
  flushLog();
  rememberError("TIME_SYNC_FAILED", "NTP time sync failed");
  return false;
}

static uint32_t currentUnixTime() {
  time_t now = time(nullptr);
  if (now < 1577836800UL) return 0;
  return (uint32_t)now;
}

static void currentDateTimeUTC(char* out, size_t outSize) {
  if (!out || outSize == 0) return;

  const uint32_t now = currentUnixTime();
  if (now == 0 || !SimpleDS3231::formatUtc(now, out, outSize)) {
    strlcpy(out, "", outSize);
  }
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
  if (!MQTT_ENABLED) {
    printer.println(F("[MQTT] MQTT disabled by configuration"), true);
    flushLog();
    return false;
  }
  if (mqttClient.connected()) {
    printer.println(F("[MQTT] Already connected"), true);
    flushLog();
    return true;
  }
  if (!ensureWifiConnected() || !internetLooksAvailable()) return false;

  setWatchdogStage("mqtt:connect");
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  printer.print(F("[MQTT] Host: "), true);
  printer.print(MQTT_HOST, true);
  printer.print(F(":"), true);
  printer.println((unsigned int)MQTT_PORT, true);
  printer.print(F("[MQTT] Client ID: "), true);
  printer.println(MQTT_CLIENT_ID, true);
  flushLog();

  const uint32_t start = millis();
  while (!mqttClient.connected() && (millis() - start) < MQTT_CONNECT_TIMEOUT_MS) {
    printer.print(F("[MQTT] Connecting elapsed_ms="), true);
    printer.print((unsigned long)(millis() - start), true);
    printer.print(F(" state="), true);
    printer.println((int)mqttClient.state(), true);
    flushLog();
    if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD)) {
      printer.println(F("[MQTT] Connected"), true);
      flushLog();
      return true;
    }
    waitWithWatchdog(1000);
  }

  printer.println(F("[MQTT] Connection failed"), true);
  printer.print(F("[MQTT] Final state: "), true);
  printer.println((int)mqttClient.state(), true);
  flushLog();
  rememberError("MQTT_CONNECT_FAILED", "MQTT connection failed");
  return false;
}

static bool publishMqtt(const char* topic, const String& payload) {
  if (!ensureMqttConnected()) return false;

  setWatchdogStage("mqtt:publish");
  printer.print(F("[MQTT] Publishing topic="), true);
  printer.print(topic, true);
  printer.print(F(" bytes="), true);
  printer.println((unsigned int)payload.length(), true);
  flushLog();

  const bool ok = mqttClient.publish(topic, payload.c_str());
  printer.print(ok ? F("[MQTT] Published: ") : F("[MQTT] Publish failed: "), true);
  printer.println(topic, true);
  flushLog();
  if (!ok) {
    rememberError("MQTT_PUBLISH_FAILED", "MQTT publish failed");
  }
  return ok;
}

static void initOta() {
  SimpleOTAConfig config = {};
  config.enabled = OTA_UPDATE_ENABLED;
  config.stationId = STATION_ID;
  config.currentVersion = FIRMWARE_VERSION;
  config.versionUrl = OTA_VERSION_URL;
  config.fallbackFirmwareUrl = OTA_FIRMWARE_URL;
  config.confirmUrl = OTA_CONFIRM_URL;
  config.httpTimeoutMs = OTA_HTTP_TIMEOUT_MS;

  ota.begin(config);
  ota.setDebug(&printer, STATION_DEBUG);
  printer.print(F("[OTA] Enabled: "), true);
  printer.println(OTA_UPDATE_ENABLED ? F("yes") : F("no"), true);
  printer.print(F("[OTA] Version URL configured: "), true);
  printer.println(strlen(OTA_VERSION_URL) > 0 ? F("yes") : F("no"), true);
  printer.print(F("[OTA] Confirm URL configured: "), true);
  printer.println(strlen(OTA_CONFIRM_URL) > 0 ? F("yes") : F("no"), true);
  flushLog();
}

static void confirmPendingOtaUpdate() {
  if (!OTA_UPDATE_ENABLED) {
    printer.println(F("[OTA] Confirm skipped; OTA disabled"), true);
    flushLog();
    return;
  }
  setWatchdogStage("ota:confirm_pending");
  if (!ota.hasPendingUpdate()) {
    printer.println(F("[OTA] No pending update to confirm"), true);
    flushLog();
    return;
  }
  if (!ensureWifiConnected()) return;

  if (!ota.confirmPendingUpdate() && ota.lastError()[0] != '\0') {
    rememberError("OTA_CONFIRM_FAILED", ota.lastError());
  }
  feedWatchdog();
}

static void checkForOtaUpdate() {
  if (!OTA_UPDATE_ENABLED) {
    printer.println(F("[OTA] Check skipped; OTA disabled"), true);
    flushLog();
    return;
  }
  if (strlen(OTA_VERSION_URL) == 0) {
    printer.println(F("[OTA] Check skipped; version URL empty"), true);
    flushLog();
    return;
  }
  setWatchdogStage("ota:check");
  if (!ensureWifiConnected()) return;

  ota.checkAndApplyUpdate();
  if (ota.lastError()[0] != '\0') {
    rememberError("OTA_CHECK_FAILED", ota.lastError());
  }
  feedWatchdog();
}

// ============================================================
// Sensor reading
// ============================================================

/*
  readConfiguredSensors()
  Accepts: nothing.
  Returns: nothing.

  Turns on the configured sensor power line, reads every sensor that belongs to
  the active station profile once, stores per-sensor OK flags, and records
  SENSOR_READ_FAILED logs for failed reads.
*/
static void readConfiguredSensors() {
  resetRuntimeFlags();
  printer.print(F("[READ] Runtime sensor count: "), true);
  printer.println((unsigned int)g_runtimeSensorCount, true);
  flushLog();

  if (g_runtimeSensorCount == 0) {
    printer.println(F("[READ] No sensors configured for this profile"), true);
    flushLog();
    return;
  }

  for (uint8_t powerLine = 0; powerLine < PCB_POWERLINE_COUNT; ++powerLine) {
    if (!hasSensorOnPowerLine(powerLine)) continue;

    printer.print(F("[READ] Power line "), true);
    printer.print((unsigned int)powerLine, true);
    printer.println(F(" begin"), true);
    flushLog();

    if (!powerLineReadState(powerLine)) {
      powerLineSet(powerLine, true);
    }

    const uint32_t warmup = maxWarmupForPowerLine(powerLine);
    if (warmup > 0) {
      printer.print(F("[READ] Warmup ms: "), true);
      printer.println((unsigned long)warmup, true);
      flushLog();
      waitWithWatchdog(warmup);
    }

    for (uint8_t iface = 0; iface < PCB_RS485_PORT_COUNT; ++iface) {
      if (!hasSensorOnInterface(powerLine, iface)) continue;

      printer.print(F("[READ] RS485 interface "), true);
      printer.print((unsigned int)iface, true);
      printer.println(F(" enable"), true);
      flushLog();
      rs485InterfaceSet(iface, true);
      if (PCB_RS485_ENABLE_DELAY_MS[iface] > 0) {
        printer.print(F("[READ] RS485 enable delay ms: "), true);
        printer.println((unsigned int)PCB_RS485_ENABLE_DELAY_MS[iface], true);
        flushLog();
        waitWithWatchdog(PCB_RS485_ENABLE_DELAY_MS[iface]);
      }

      for (size_t i = 0; i < g_runtimeSensorCount; ++i) {
        SensorDriver* sensor = g_runtimeSensors[i].driver;
        if (sensor->getPowerLineIndex() != powerLine) continue;
        if (sensor->getInterfaceIndex() != iface) continue;

        char watchdogStage[WATCHDOG_STAGE_TEXT_SIZE];
        snprintf(watchdogStage, sizeof(watchdogStage), "sensor:%s", g_runtimeSensors[i].id);
        setWatchdogStage(watchdogStage);
        feedWatchdog();

        printer.print(F("[READ] "), true);
        printer.print(g_runtimeSensors[i].id, true, " addr=0x");
        printer.println((unsigned int)sensor->getAddress(), true, "", HEX);
        printer.print(F("[READ] Sensor timeout ms="), true);
        printer.print((unsigned int)SENSOR_DEFAULT_READ_TIMEOUT_MS, true);
        printer.print(F(" retries="), true);
        printer.println((unsigned int)SENSOR_DEFAULT_DRIVER_RETRIES, true);
        flushLog();

        const uint32_t sensorStart = millis();
        const bool ok = sensor->readData();
        const uint32_t sensorElapsed = millis() - sensorStart;
        g_runtimeSensors[i].lastReadOk = ok;

        printer.print(ok ? F("[READ] OK: ") : F("[READ] FAIL: "), true);
        printer.print(g_runtimeSensors[i].id, true);
        printer.print(F(" elapsed_ms="), true);
        printer.print((unsigned long)sensorElapsed, true);
        printer.print(F(" consecutive_errors="), true);
        printer.println((unsigned int)sensor->getConsecutiveErrors(), true);
        flushLog();
        if (!ok) {
          rememberError("SENSOR_READ_FAILED",
                        "sensor did not return valid data",
                        g_runtimeSensors[i].id,
                        sensor->getAddress(),
                        sensor->getConsecutiveErrors());
        }

        feedWatchdog();
      }

      printer.print(F("[READ] RS485 interface "), true);
      printer.print((unsigned int)iface, true);
      printer.println(F(" disable"), true);
      flushLog();
      rs485InterfaceSet(iface, false);
    }

    if (!powerLineShouldStayOn(powerLine)) {
      powerLineSet(powerLine, false);
    }
    printer.print(F("[READ] Power line "), true);
    printer.print((unsigned int)powerLine, true);
    printer.println(F(" complete"), true);
    flushLog();
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
  uint8_t rain;
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
    case KIND_TEMPERATURE_HUMIDITY: {
      JXCT_TemperatureHumidity* s = static_cast<JXCT_TemperatureHumidity*>(entry.driver);
      appendTelemetryNumber(out, "AirT", counters.airTemperature++, s->air_temperature_C, 2, first);
      appendTelemetryNumber(out, "AirH", counters.airHumidity++, s->humidity_percent, 2, first);
      break;
    }
    case KIND_ATMOSPHERIC_PRESSURE: {
      JXCT_AtmosphericPressure* s = static_cast<JXCT_AtmosphericPressure*>(entry.driver);
      appendTelemetryNumber(out, "AtmP", counters.atmosphericPressure++, s->pressure_mbar, 2, first);
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
      appendTelemetryNumber(out, "SolarRad", counters.totalSolar++, s->total_solar_w_m2, 2, first);
      break;
    }
    case KIND_EVAPORATION: {
      JXCT_Evaporation* s = static_cast<JXCT_Evaporation*>(entry.driver);
      const uint8_t occurrence = counters.evaporation++;
      appendTelemetryNumber(out, "EvapWaterWeight", occurrence, s->evaporation_value, 2, first);
      appendTelemetryNumber(out, "EvapWaterLevel", occurrence, s->evaporation_value / 31.41593, 2, first);
      break;
    }
    case KIND_OPTICAL_RAIN: {
      JXBS_OpticalRainGauge* s = static_cast<JXBS_OpticalRainGauge*>(entry.driver);
      appendTelemetryNumber(out, "Rain", counters.rain++, s->rainfall_mm, 2, first);
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
      appendTelemetryNumber(out, "SuspendedSolids", counters.waterSS++, s->suspended_solids_mg_L, 2, first);
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
    case KIND_PM25_PM10_STANDALONE: {
      JXBS_PM25PM10Standalone* s = static_cast<JXBS_PM25PM10Standalone*>(entry.driver);
      appendTelemetryNumber(out, "PM2_5", counters.pm25++, s->pm2_5_ug_m3, 2, first);
      appendTelemetryNumber(out, "PM10", counters.pm10++, s->pm10_ug_m3, 2, first);
      break;
    }
    case KIND_GAS_CO: {
      JXBS_GasSensor* s = static_cast<JXBS_GasSensor*>(entry.driver);
      appendTelemetryNumber(out, "CO", counters.co++, s->gas_ppm, 2, first);
      break;
    }
    case KIND_GAS_O3: {
      JXBS_GasSensor* s = static_cast<JXBS_GasSensor*>(entry.driver);
      appendTelemetryNumber(out, "O3", counters.o3++, s->gas_ppm, 2, first);
      break;
    }
    case KIND_GAS_NH3: {
      JXBS_GasSensor* s = static_cast<JXBS_GasSensor*>(entry.driver);
      appendTelemetryNumber(out, "NH3", counters.nh3++, s->gas_ppm, 2, first);
      break;
    }
    case KIND_GAS_SO2: {
      JXBS_GasSensor* s = static_cast<JXBS_GasSensor*>(entry.driver);
      appendTelemetryNumber(out, "SO2", counters.so2++, s->gas_ppm, 2, first);
      break;
    }
    case KIND_GAS_NO2: {
      JXBS_GasSensor* s = static_cast<JXBS_GasSensor*>(entry.driver);
      appendTelemetryNumber(out, "NO2", counters.no2++, s->gas_ppm, 2, first);
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

/*
  buildJsonPayload()
  Accepts: nothing.
  Returns: complete MQTT JSON payload as Arduino String.

  The payload contains station identity, firmware version, sensor flags, sensor
  data, and logs. Failed sensor values are not added to "data"; their status is
  visible in "flags" and "logs".
*/
static String buildJsonPayload() {
  char dateTime[24] = "";
  currentDateTimeUTC(dateTime, sizeof(dateTime));

  double batteryVoltage = 0.0;
  const bool hasBattery = readBatteryVoltage(batteryVoltage);

  String out;
  out.reserve(3072);
  out += '{';

  bool firstRoot = true;
  appendJsonString(out, "station_id", STATION_ID, firstRoot);
  appendJsonString(out, "station_name", STATION_NAME, firstRoot);
  appendJsonString(out, "firmware_version", FIRMWARE_VERSION, firstRoot);
  appendJsonUInt(out, "upload_rate_min", UPLOAD_RATE_MIN, firstRoot);
  appendJsonUInt(out, "sleep_cycle", g_deepSleepCycleCount, firstRoot);
  appendJsonBool(out, "rtc_available", g_rtcAvailable, firstRoot);
  appendJsonUInt(out, "timestamp", currentUnixTime(), firstRoot);
  appendJsonString(out, "datetime_utc", dateTime, firstRoot);
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

  appendErrorLogsJson(out, firstRoot);

  out += '}';
  return out;
}

/*
  publishTelemetry()
  Accepts: nothing.
  Returns: true when the JSON payload was published to MQTT.

  On success, remembered error logs are cleared. On failure, the logs stay in
  memory and will be retried with the next upload cycle.
*/
static bool publishTelemetry() {
  if (!MQTT_ENABLED) {
    printer.println(F("[PAYLOAD] MQTT publishing disabled"), true);
    flushLog();
    return false;
  }

  bool jsonPublished = false;
  if (TELEMETRY_ENABLE_JSON_PAYLOAD) {
    printer.println(F("[PAYLOAD] Building JSON payload"), true);
    flushLog();
    const String payload = buildJsonPayload();
    printer.print(F("[PAYLOAD] JSON bytes: "), true);
    printer.println((unsigned int)payload.length(), true);
    flushLog();
    if (TELEMETRY_PRINT_PAYLOADS) {
      printer.print(F("[PAYLOAD][JSON] "), true);
      printer.println(payload.c_str(), true);
      flushLog();
    }
    jsonPublished = publishMqtt(MQTT_JSON_TOPIC, payload);
    if (jsonPublished) {
      clearRememberedErrors();
    }
  } else {
    printer.println(F("[PAYLOAD] JSON payload disabled"), true);
    flushLog();
  }

  return jsonPublished;
}

static uint32_t secondsUntilNextUploadCycle() {
  uint32_t fallbackSeconds = (uint32_t)UPLOAD_RATE_MIN * 60UL;
  if (fallbackSeconds < SLEEP_MIN_SECONDS) {
    fallbackSeconds = SLEEP_MIN_SECONDS;
  }

  const uint32_t now = currentUnixTime();
  const uint32_t intervalSeconds = (uint32_t)UPLOAD_RATE_MIN * 60UL;
  if (now == 0 || intervalSeconds == 0) {
    return fallbackSeconds;
  }

  uint32_t nextEpoch = ((now / intervalSeconds) + 1UL) * intervalSeconds;
  uint32_t sleepSeconds = nextEpoch - now;
  if (sleepSeconds < SLEEP_MIN_SECONDS) {
    sleepSeconds += intervalSeconds;
  }
  if (sleepSeconds > SLEEP_MAX_SECONDS) {
    sleepSeconds = SLEEP_MAX_SECONDS;
  }
  return sleepSeconds;
}

static void prepareHardwareForSleep() {
  if (mqttClient.connected()) {
    mqttClient.disconnect();
  }

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  networkPowerSet(false);

  for (uint8_t iface = 0; iface < PCB_RS485_PORT_COUNT; ++iface) {
    rs485InterfaceSet(iface, false);
  }
  for (uint8_t powerLine = 0; powerLine < PCB_POWERLINE_COUNT; ++powerLine) {
    powerLineSet(powerLine, false);
  }
}

static void sleepUntilNextCycle() {
  if (!SLEEP_AFTER_CYCLE_ENABLED) {
    return;
  }

  const uint32_t sleepSeconds = secondsUntilNextUploadCycle();
  ++g_deepSleepCycleCount;

  printer.print(F("[SLEEP] Deep sleep seconds: "), true);
  printer.println((unsigned long)sleepSeconds, true);
  printer.print(F("[SLEEP] Cycle count: "), true);
  printer.println((unsigned long)g_deepSleepCycleCount, true);

  setWatchdogStage("deep_sleep");
  prepareHardwareForSleep();
  printer.flush();

  #if defined(ARDUINO_ARCH_ESP32)
    esp_task_wdt_delete(NULL);
    esp_sleep_enable_timer_wakeup((uint64_t)sleepSeconds * 1000000ULL);
    esp_deep_sleep_start();
  #endif
}

/*
  runStationCycle()
  Accepts: nothing.
  Returns: nothing.

  One full station cycle: refresh time from RTC, read sensors once, sync RTC
  from network only when due, publish JSON, check OTA after upload, then sleep.
*/
static void runStationCycle() {
  setWatchdogStage("cycle:start");
  feedWatchdog();
  const uint32_t cycleStart = millis();

  printer.println(F(""), true);
  printer.println(F("============================================================"), true);
  printer.println(F("[CYCLE] Starting station read/upload cycle"), true);
  printer.println(F("============================================================"), true);
  flushLog();

  setWatchdogStage("cycle:rtc_time");
  if (!g_timeIsSynced || !SLEEP_AFTER_CYCLE_ENABLED) {
    const bool rtcOk = readRtcIntoSystemClock();
    printer.println(rtcOk ? F("[CYCLE] RTC read OK") : F("[CYCLE] RTC read skipped/failed"), true);
    flushLog();
  } else {
    printer.println(F("[CYCLE] RTC read skipped; system time already synced"), true);
    flushLog();
  }
  feedWatchdog();

  setWatchdogStage("cycle:read_sensors");
  readConfiguredSensors();
  printer.println(F("[CYCLE] Sensor read stage complete"), true);
  flushLog();
  feedWatchdog();

  setWatchdogStage("cycle:network_time_sync");
  const bool timeOk = syncTimeFromInternet(false);
  printer.println(timeOk ? F("[CYCLE] Time stage OK") : F("[CYCLE] Time stage failed/skipped"), true);
  flushLog();
  feedWatchdog();

  setWatchdogStage("cycle:mqtt_upload");
  const bool uploaded = publishTelemetry();
  printer.println(uploaded ? F("[CYCLE] Upload OK") : F("[CYCLE] Upload failed/skipped"), true);
  flushLog();
  feedWatchdog();

  if (uploaded) {
    setWatchdogStage("cycle:ota_confirm");
    confirmPendingOtaUpdate();
    feedWatchdog();

    setWatchdogStage("cycle:ota");
    checkForOtaUpdate();
  } else {
    printer.println(F("[CYCLE] OTA skipped because upload did not succeed"), true);
    flushLog();
  }

  setWatchdogStage("idle");
  g_lastCycleMs = millis();
  printer.print(F("[CYCLE] Complete elapsed_ms="), true);
  printer.println((unsigned long)(millis() - cycleStart), true);
  flushLog();
  sleepUntilNextCycle();
}

static void printLoopHeartbeatIfDue() {
  const uint32_t nowMs = millis();
  if ((nowMs - g_lastLoopHeartbeatMs) < LOOP_HEARTBEAT_INTERVAL_MS) {
    return;
  }
  g_lastLoopHeartbeatMs = nowMs;

  printer.print(F("[LOOP] alive ms="), true);
  printer.print((unsigned long)nowMs, true);
  printer.print(F(" stage="), true);
  printer.print(g_watchdogStage, true);
  printer.print(F(" wifi_status="), true);
  printer.print((int)WiFi.status(), true);
  printer.print(F(" mqtt="), true);
  printer.print(mqttClient.connected() ? F("connected") : F("disconnected"), true);
  printer.print(F(" heap="), true);
  printer.print((unsigned long)ESP.getFreeHeap(), true);
  printer.print(F(" unix="), true);
  printer.println((unsigned long)currentUnixTime(), true);
  flushLog();
}

void setup() {
  beginDebugSerial();
  printer.setEnable(true);

  initRetainedState();
  printBanner();
  printResetReason();
  rememberWatchdogResetIfNeeded();
  startWatchdog();

  setWatchdogStage("setup:init_hardware");
  initPowerLines();
  initInterfaces();
  initNetworkPowerControl();
  initOta();
  mqttClient.setBufferSize(MQTT_BUFFER_SIZE);
  printer.print(F("[MQTT] Buffer size: "), true);
  printer.println((unsigned int)MQTT_BUFFER_SIZE, true);
  flushLog();
  feedWatchdog();

  setWatchdogStage("setup:rtc");
  readRtcIntoSystemClock();
  feedWatchdog();

  setWatchdogStage("setup:rs485");
  rs485Bus0.setDebug(&printer);
  printer.print(F("[RS485] begin baud="), true);
  printer.print((unsigned long)RS485_DEFAULT_BAUD, true);
  printer.print(F(" rx="), true);
  printer.print((int)PCB_RS485_RX_PINS[RS485_PORT_INDEX_0], true);
  printer.print(F(" tx="), true);
  printer.print((int)PCB_RS485_TX_PINS[RS485_PORT_INDEX_0], true);
  printer.print(F(" de="), true);
  printer.println((int)PCB_RS485_DE_PINS[RS485_PORT_INDEX_0], true);
  flushLog();
  rs485Bus0.begin(RS485Hw0,
                  RS485_DEFAULT_BAUD,
                  PCB_RS485_RX_PINS[RS485_PORT_INDEX_0],
                  PCB_RS485_TX_PINS[RS485_PORT_INDEX_0],
                  RS485_DEFAULT_SERIAL_CONFIG);
  rs485Bus0.setDirectionControl(PCB_RS485_DE_PINS[RS485_PORT_INDEX_0],
                                PCB_RS485_DE_ACTIVE_HIGH[RS485_PORT_INDEX_0]);
  feedWatchdog();

  setWatchdogStage("setup:sensor_map");
  buildRuntimeSensors();
  printSensorMap();
  feedWatchdog();

  setWatchdogStage("idle");
  g_lastCycleMs = millis() - TELEMETRY_CYCLE_INTERVAL_MS;
  printer.println(F("[BOOT] Setup complete, starting first cycle"), true);
  flushLog();
  runStationCycle();
}

void loop() {
  feedWatchdog();
  printLoopHeartbeatIfDue();

  if ((millis() - g_lastCycleMs) >= TELEMETRY_CYCLE_INTERVAL_MS) {
    runStationCycle();
  }

  if (TIME_SYNC_ENABLED && (millis() - g_lastTimeSyncMs) >= TIME_SYNC_INTERVAL_MS) {
    setWatchdogStage("loop:time_sync");
    syncTimeFromInternet(true);
    setWatchdogStage("idle");
  }

  if (mqttClient.connected()) {
    setWatchdogStage("loop:mqtt");
    mqttClient.loop();
    setWatchdogStage("idle");
  }

  waitWithWatchdog(1000);
}
