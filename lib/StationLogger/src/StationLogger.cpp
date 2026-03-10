#include "StationLogger.h"

// ============================================================================
// Constructor
// ============================================================================
StationLogger::StationLogger(uint8_t chipSelectPin)
  : _csPin(chipSelectPin), _sdReady(false),
    _year(2026), _month(1), _day(1),
    _hour(0), _minute(0), _second(0),
    _log(nullptr), _debugEnable(false) {}

// ============================================================================
// begin — initialize SD card
// ============================================================================
bool StationLogger::begin() {
  _sdReady = SD.begin(_csPin);

  if (_log) {
    if (_sdReady) {
      _log->println(F("[Logger] SD card initialized OK"), _debugEnable);
    } else {
      _log->println(F("[Logger] SD card FAILED to initialize!"), _debugEnable);
    }
  }

  return _sdReady;
}

// ============================================================================
// setDateTime — update internal clock (call after RTC/NTP sync)
// ============================================================================
void StationLogger::setDateTime(uint16_t year, uint8_t month, uint8_t day,
                                uint8_t hour, uint8_t minute, uint8_t second) {
  _year   = year;
  _month  = month;
  _day    = day;
  _hour   = hour;
  _minute = minute;
  _second = second;
}

// ============================================================================
// setDebug
// ============================================================================
void StationLogger::setDebug(PrintController* printer, bool enable) {
  _log = printer;
  _debugEnable = enable;
}

// ============================================================================
// getFilename — builds filename based on type and current month
// ============================================================================
// Results:
//   LOG_DATA   -> "data_2026_03.csv"
//   LOG_ACTION -> "act_2026_03.log"
//   LOG_ERROR  -> "err_2026_03.log"
void StationLogger::getFilename(LogType type, char* buf, size_t bufLen) {
  const char* prefix;
  const char* ext;

  switch (type) {
    case LOG_DATA:   prefix = "data"; ext = "csv"; break;
    case LOG_ACTION: prefix = "act";  ext = "log"; break;
    case LOG_ERROR:  prefix = "err";  ext = "log"; break;
    default:         prefix = "unk";  ext = "log"; break;
  }

  // Format: prefix_YYYY_MM.ext  (keep short for FAT32 compatibility)
  snprintf(buf, bufLen, "%s_%04u_%02u.%s", prefix, _year, _month, ext);
}

// ============================================================================
// buildTimestamp — "YYYY-MM-DD HH:MM:SS"
// ============================================================================
void StationLogger::buildTimestamp(char* buf, size_t bufLen) {
  snprintf(buf, bufLen, "%04u-%02u-%02u %02u:%02u:%02u",
           _year, _month, _day, _hour, _minute, _second);
}

// ============================================================================
// writeLine — appends "timestamp, message\n" to a file
// ============================================================================
bool StationLogger::writeLine(const char* filename, const char* timestamp, const char* message) {
  if (!_sdReady) return false;

  File file = SD.open(filename, FILE_WRITE);
  if (!file) {
    if (_log) {
      _log->print(F("[Logger] Cannot open: "), _debugEnable);
      _log->println(filename, _debugEnable);
    }
    return false;
  }

  file.print(timestamp);
  file.print(", ");
  file.println(message);
  file.flush();   // Force write to prevent data loss on power failure
  file.close();

  return true;
}

// ============================================================================
// log — generic log function
// ============================================================================
bool StationLogger::log(LogType type, const char* message) {
  char filename[24];
  getFilename(type, filename, sizeof(filename));

  char timestamp[22];
  buildTimestamp(timestamp, sizeof(timestamp));

  if (_log) {
    _log->print(F("[Logger] "), _debugEnable);
    _log->print(filename, _debugEnable);
    _log->print(F(" <- "), _debugEnable);
    _log->println(message, _debugEnable);
  }

  return writeLine(filename, timestamp, message);
}

// ============================================================================
// Convenience functions
// ============================================================================
bool StationLogger::logData(const char* message)   { return log(LOG_DATA, message); }
bool StationLogger::logAction(const char* message)  { return log(LOG_ACTION, message); }
bool StationLogger::logError(const char* message)   { return log(LOG_ERROR, message); }
