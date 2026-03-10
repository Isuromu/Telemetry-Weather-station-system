#include "MemoryMonitor.h"

// ============================================================================
// Constructor
// ============================================================================
MemoryMonitor::MemoryMonitor(uint8_t warningPercent, uint8_t criticalPercent)
  : _warningPercent(warningPercent), _criticalPercent(criticalPercent),
    _totalBytes(0), _usedBytes(0), _storageStatus(MEMORY_OK),
    _log(nullptr), _debugEnable(false) {}

// ============================================================================
// getFreeRAM — returns free RAM on the MCU in bytes
// ============================================================================
uint32_t MemoryMonitor::getFreeRAM() {
#if defined(ARDUINO_ARCH_ESP32)
  return ESP.getFreeHeap();
#elif defined(ARDUINO_ARCH_AVR)
  // AVR: free RAM = gap between top of heap and bottom of stack
  extern int __heap_start, *__brkval;
  int v;
  return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
#else
  return 0;  // Unknown platform
#endif
}

// ============================================================================
// setStorageInfo — main code reports SD card total/used after checking
// ============================================================================
void MemoryMonitor::setStorageInfo(uint64_t totalBytes, uint64_t usedBytes) {
  _totalBytes = totalBytes;
  _usedBytes  = usedBytes;

  // Calculate free percentage and update status
  uint8_t freePercent = getFreePercent();

  if (freePercent <= _criticalPercent) {
    _storageStatus = MEMORY_CRITICAL;
  } else if (freePercent <= _warningPercent) {
    _storageStatus = MEMORY_WARNING;
  } else {
    _storageStatus = MEMORY_OK;
  }
}

// ============================================================================
// getFreePercent
// ============================================================================
uint8_t MemoryMonitor::getFreePercent() const {
  if (_totalBytes == 0) return 100;  // No info yet, assume OK
  uint64_t freeBytes = _totalBytes - _usedBytes;
  return (uint8_t)((freeBytes * 100ULL) / _totalBytes);
}

// ============================================================================
// setDebug
// ============================================================================
void MemoryMonitor::setDebug(PrintController* printer, bool enable) {
  _log = printer;
  _debugEnable = enable;
}

// ============================================================================
// printStatus — debug output of memory state
// ============================================================================
void MemoryMonitor::printStatus() {
  if (!_log) return;

  _log->println(F("[Memory] --- Status ---"), _debugEnable);

  _log->print(F("[Memory] Free RAM: "), _debugEnable);
  _log->print((unsigned long)getFreeRAM(), _debugEnable);
  _log->println(F(" bytes"), _debugEnable);

  _log->print(F("[Memory] Storage free: "), _debugEnable);
  _log->print((unsigned long)(getFreeStorage() / 1024ULL), _debugEnable);
  _log->print(F(" KB ("), _debugEnable);
  _log->print((int)getFreePercent(), _debugEnable);
  _log->println(F("%)"), _debugEnable);

  _log->print(F("[Memory] Status: "), _debugEnable);
  switch (_storageStatus) {
    case MEMORY_OK:       _log->println(F("OK"), _debugEnable); break;
    case MEMORY_WARNING:  _log->println(F("WARNING"), _debugEnable); break;
    case MEMORY_CRITICAL: _log->println(F("CRITICAL — STOP COLLECTING"), _debugEnable); break;
  }
}
