#pragma once
#include <Arduino.h>
#include <SD.h>
#include "PrintController.h"

// ============================================================================
// StationLogger — Monthly Rotating Log Files on SD Card
// ============================================================================
// Creates one file per month per log type:
//   data_2026_03.csv    — sensor readings
//   actions_2026_03.log — boot, read, upload events
//   errors_2026_03.log  — failures, faults, watchdog resets
//
// File naming: <type>_<year>_<month>.<ext>
// Each line is timestamped: "2026-03-09 14:30:00, <message>"
//
// Usage:
//   logger.logData("leaf_s1, 25.3, 67.8, OK");
//   logger.logAction("BOOT: station started");
//   logger.logError("leaf_s1: CRC failure, 3 consecutive errors");
// ============================================================================

// Log types — each gets its own monthly file
enum LogType {
  LOG_DATA,     // Sensor readings CSV
  LOG_ACTION,   // System events (boot, read, upload)
  LOG_ERROR     // Failures and faults
};

class StationLogger {
public:
  // chipSelectPin = SD card SPI CS pin
  StationLogger(uint8_t chipSelectPin = 5);

  // Initialize the SD card. Returns true if OK.
  bool begin();

  // Check if SD card is working
  bool isReady() const { return _sdReady; }

  // Set current date/time (call after RTC read or NTP sync)
  // year = full year (2026), month = 1-12, day = 1-31
  // hour = 0-23, minute = 0-59, second = 0-59
  void setDateTime(uint16_t year, uint8_t month, uint8_t day,
                   uint8_t hour, uint8_t minute, uint8_t second);

  // --- Log Functions ---
  // Each appends one timestamped line to the appropriate monthly file.
  // Returns true if write succeeded.

  bool logData(const char* message);       // -> data_YYYY_MM.csv
  bool logAction(const char* message);     // -> actions_YYYY_MM.log
  bool logError(const char* message);      // -> errors_YYYY_MM.log

  // Generic log function
  bool log(LogType type, const char* message);

  // Get the filename for a given log type (based on current date)
  void getFilename(LogType type, char* buf, size_t bufLen);

  // Optional: attach PrintController for debug output
  void setDebug(PrintController* printer, bool enable);

private:
  uint8_t _csPin;
  bool _sdReady;

  // Current date/time (set by RTC or NTP)
  uint16_t _year;
  uint8_t  _month, _day, _hour, _minute, _second;

  // Debug
  PrintController* _log;
  bool _debugEnable;

  // Build timestamp string: "YYYY-MM-DD HH:MM:SS"
  void buildTimestamp(char* buf, size_t bufLen);

  // Write one line to the file
  bool writeLine(const char* filename, const char* timestamp, const char* message);
};
