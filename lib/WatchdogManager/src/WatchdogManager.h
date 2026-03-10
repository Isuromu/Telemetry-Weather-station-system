#pragma once
#include <Arduino.h>
#include "PrintController.h"

// ============================================================================
// WatchdogManager — Hardware Watchdog Timer for ESP32 and AVR
// ============================================================================
// Prevents the station from hanging on sensor faults or bus lockups.
//
// Before each sensor read, call noteCurrentSensor() with the sensor name.
// If the WDT fires, on reboot check getLastCrashSensor() to identify
// which sensor caused the hang. After repeated crashes on the same
// sensor, the main loop can mark it OFFLINE.
//
// Usage:
//   wdt.begin(15);  // 15 second timeout
//   wdt.noteCurrentSensor("leaf_s1");
//   leaf_s1.readData();
//   wdt.feed();  // Reset the timer — everything OK
// ============================================================================

// Max length of sensor name stored in RTC memory (ESP32) or EEPROM (AVR)
#define WDT_SENSOR_NAME_LEN 16

class WatchdogManager {
public:
  WatchdogManager();

  // Initialize watchdog with a timeout in seconds
  // ESP32: uses esp_task_wdt
  // AVR: uses wdt_enable()
  void begin(uint8_t timeoutSeconds = 15);

  // Feed the watchdog — call this regularly to prevent reset
  void feed();

  // Record which sensor is currently being read.
  // If WDT fires during this sensor's read, we know who caused it.
  void noteCurrentSensor(const char* sensorName);

  // After a WDT reset, call this to check what sensor was being read.
  // Returns empty string if no crash data or clean boot.
  const char* getLastCrashSensor();

  // Check if the last reset was caused by the watchdog
  bool wasWatchdogReset();

  // Clear the crash sensor record (after handling it)
  void clearCrashRecord();

  // Optional debug
  void setDebug(PrintController* printer, bool enable);

private:
  uint8_t _timeoutSec;
  char _currentSensor[WDT_SENSOR_NAME_LEN];
  char _crashSensor[WDT_SENSOR_NAME_LEN];

  PrintController* _log;
  bool _debugEnable;

  // Save/load crash sensor name from persistent memory
  void saveCrashSensor();
  void loadCrashSensor();
};
