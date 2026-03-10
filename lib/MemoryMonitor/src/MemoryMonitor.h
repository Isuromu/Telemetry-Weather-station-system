#pragma once
#include <Arduino.h>
#include "PrintController.h"

// ============================================================================
// MemoryMonitor — Tracks SD card and MCU memory usage
// ============================================================================
// Checks available memory and alerts when thresholds are hit.
// On ESP32: checks internal heap (ESP.getFreeHeap())
// On AVR: checks free RAM between stack and heap
//
// If storage hits critical limit:
//   1. Sends alert flag (for server upload)
//   2. Station enters STORAGE_FULL mode — stops data collection
//   3. Only checks server for resolution commands
//
// Usage:
//   if (memMonitor.isStorageFull()) { /* stop collecting, alert server */ }
// ============================================================================

// Memory status
enum MemoryStatus {
  MEMORY_OK,           // Everything fine
  MEMORY_WARNING,      // Getting low (< 20% free)
  MEMORY_CRITICAL      // Hit limit — stop data collection
};

class MemoryMonitor {
public:
  // thresholdPercent = when free space drops below this %, status = WARNING
  // criticalPercent  = when free space drops below this %, status = CRITICAL
  MemoryMonitor(uint8_t warningPercent = 20, uint8_t criticalPercent = 5);

  // Get free RAM on the MCU (bytes)
  uint32_t getFreeRAM();

  // --- SD Card Space (must be set externally after checking SD) ---
  // The SD library doesn't have a universal free-space function,
  // so the main code checks and reports here.
  void setStorageInfo(uint64_t totalBytes, uint64_t usedBytes);

  // Get storage status based on last reported info
  MemoryStatus getStorageStatus() const { return _storageStatus; }
  bool isStorageFull() const { return _storageStatus == MEMORY_CRITICAL; }
  bool isStorageWarning() const { return _storageStatus == MEMORY_WARNING; }

  // Get free storage in bytes and percent
  uint64_t getFreeStorage() const { return _totalBytes - _usedBytes; }
  uint8_t  getFreePercent() const;

  // Server resolved the storage issue (e.g., cleared old files)
  void clearStorageAlert() { _storageStatus = MEMORY_OK; }

  // Optional debug
  void setDebug(PrintController* printer, bool enable);

  // Print a status summary to debug serial
  void printStatus();

private:
  uint8_t _warningPercent;
  uint8_t _criticalPercent;

  uint64_t _totalBytes;
  uint64_t _usedBytes;
  MemoryStatus _storageStatus;

  PrintController* _log;
  bool _debugEnable;
};
