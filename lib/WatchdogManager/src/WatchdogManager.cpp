#include "WatchdogManager.h"

#if defined(ARDUINO_ARCH_ESP32)
  #include <esp_task_wdt.h>
  // ESP32 RTC memory survives deep sleep AND watchdog resets
  RTC_DATA_ATTR char _rtcCrashSensor[WDT_SENSOR_NAME_LEN] = {0};
  RTC_DATA_ATTR bool _rtcHasCrash = false;
#elif defined(ARDUINO_ARCH_AVR)
  #include <avr/wdt.h>
  #include <EEPROM.h>
  // AVR: store crash sensor in EEPROM (address 0-15)
  #define EEPROM_CRASH_ADDR 0
  #define EEPROM_CRASH_FLAG_ADDR 16
#endif

// ============================================================================
// Constructor
// ============================================================================
WatchdogManager::WatchdogManager()
  : _timeoutSec(15), _log(nullptr), _debugEnable(false) {
  memset(_currentSensor, 0, WDT_SENSOR_NAME_LEN);
  memset(_crashSensor, 0, WDT_SENSOR_NAME_LEN);
}

// ============================================================================
// begin — initialize hardware watchdog
// ============================================================================
void WatchdogManager::begin(uint8_t timeoutSeconds) {
  _timeoutSec = timeoutSeconds;

  // Load any crash data from last boot
  loadCrashSensor();

#if defined(ARDUINO_ARCH_ESP32)
  // Configure ESP32 Task WDT
  esp_task_wdt_init(_timeoutSec, true);  // true = panic on timeout
  esp_task_wdt_add(NULL);                // Add current task

  if (_log) {
    _log->print(F("[WDT] ESP32 watchdog started, timeout: "), _debugEnable);
    _log->print((int)_timeoutSec, _debugEnable);
    _log->println(F(" sec"), _debugEnable);
  }

#elif defined(ARDUINO_ARCH_AVR)
  // AVR: max WDT timeout is 8 seconds
  // Use the closest available prescaler
  if (_timeoutSec >= 8) wdt_enable(WDTO_8S);
  else if (_timeoutSec >= 4) wdt_enable(WDTO_4S);
  else if (_timeoutSec >= 2) wdt_enable(WDTO_2S);
  else wdt_enable(WDTO_1S);

  if (_log) {
    _log->println(F("[WDT] AVR watchdog started"), _debugEnable);
  }
#endif
}

// ============================================================================
// feed — pet the watchdog (call regularly!)
// ============================================================================
void WatchdogManager::feed() {
#if defined(ARDUINO_ARCH_ESP32)
  esp_task_wdt_reset();
#elif defined(ARDUINO_ARCH_AVR)
  wdt_reset();
#endif
}

// ============================================================================
// noteCurrentSensor — record what we're currently reading
// ============================================================================
// If the WDT fires during this sensor's read, we know the culprit
void WatchdogManager::noteCurrentSensor(const char* sensorName) {
  strncpy(_currentSensor, sensorName, WDT_SENSOR_NAME_LEN - 1);
  _currentSensor[WDT_SENSOR_NAME_LEN - 1] = '\0';

  // Save to persistent memory so it survives the WDT reset
  saveCrashSensor();
}

// ============================================================================
// wasWatchdogReset — check if last boot was caused by WDT
// ============================================================================
bool WatchdogManager::wasWatchdogReset() {
#if defined(ARDUINO_ARCH_ESP32)
  return _rtcHasCrash;
#elif defined(ARDUINO_ARCH_AVR)
  return EEPROM.read(EEPROM_CRASH_FLAG_ADDR) == 0xAA;
#else
  return false;
#endif
}

// ============================================================================
// getLastCrashSensor — which sensor was being read when WDT fired?
// ============================================================================
const char* WatchdogManager::getLastCrashSensor() {
  return _crashSensor;
}

// ============================================================================
// clearCrashRecord — call after handling the crash info
// ============================================================================
void WatchdogManager::clearCrashRecord() {
  memset(_crashSensor, 0, WDT_SENSOR_NAME_LEN);

#if defined(ARDUINO_ARCH_ESP32)
  _rtcHasCrash = false;
  memset(_rtcCrashSensor, 0, WDT_SENSOR_NAME_LEN);
#elif defined(ARDUINO_ARCH_AVR)
  EEPROM.write(EEPROM_CRASH_FLAG_ADDR, 0x00);
#endif
}

// ============================================================================
// saveCrashSensor — save to persistent memory before potential crash
// ============================================================================
void WatchdogManager::saveCrashSensor() {
#if defined(ARDUINO_ARCH_ESP32)
  strncpy(_rtcCrashSensor, _currentSensor, WDT_SENSOR_NAME_LEN);
  _rtcHasCrash = true;
#elif defined(ARDUINO_ARCH_AVR)
  for (uint8_t i = 0; i < WDT_SENSOR_NAME_LEN; i++) {
    EEPROM.update(EEPROM_CRASH_ADDR + i, _currentSensor[i]);
  }
  EEPROM.update(EEPROM_CRASH_FLAG_ADDR, 0xAA);
#endif
}

// ============================================================================
// loadCrashSensor — load from persistent memory on boot
// ============================================================================
void WatchdogManager::loadCrashSensor() {
#if defined(ARDUINO_ARCH_ESP32)
  if (_rtcHasCrash) {
    strncpy(_crashSensor, _rtcCrashSensor, WDT_SENSOR_NAME_LEN);
  }
#elif defined(ARDUINO_ARCH_AVR)
  if (EEPROM.read(EEPROM_CRASH_FLAG_ADDR) == 0xAA) {
    for (uint8_t i = 0; i < WDT_SENSOR_NAME_LEN; i++) {
      _crashSensor[i] = EEPROM.read(EEPROM_CRASH_ADDR + i);
    }
  }
#endif
}

// ============================================================================
// setDebug
// ============================================================================
void WatchdogManager::setDebug(PrintController* printer, bool enable) {
  _log = printer;
  _debugEnable = enable;
}
