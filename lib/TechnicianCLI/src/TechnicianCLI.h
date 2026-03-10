#pragma once
#include <Arduino.h>
#include "PrintController.h"
#include "SlotManager.h"
#include "StationLogger.h"
#include "MemoryMonitor.h"
#include "WatchdogManager.h"

// ============================================================================
// TechnicianCLI — Authenticated Serial Command Interface
// ============================================================================
// Physical USB serial connection for field technicians.
// Requires passphrase authentication — prevents unauthorized access.
//
// Commands (after authentication):
//   status    — show all sensor statuses and slot usage
//   memory    — show RAM and storage info
//   read <n>  — force-read sensor at index n
//   reset <n> — reset sensor n from OFFLINE back to ONLINE
//   rate <n> <mins> — change sensor n sample rate
//   debug <n> <0|1> — toggle debug for sensor n
//   logs      — list log files on SD
//   reboot    — restart the MCU
//   help      — show available commands
//   exit      — lock CLI (requires re-authentication)
//
// Security:
//   - Physical USB cable required (no wireless attack surface)
//   - Passphrase must be typed within 30 seconds of connecting
//   - After 3 wrong attempts, CLI locks for 60 seconds
// ============================================================================

// CLI authentication state
enum CLIState {
  CLI_LOCKED,        // Waiting for passphrase
  CLI_AUTHENTICATED, // Commands accepted
  CLI_COOLDOWN       // Too many wrong attempts, locked
};

// Default passphrase — CHANGE THIS in production!
#define DEFAULT_CLI_PASSPHRASE "STATION2026"

// Max wrong attempts before cooldown
#define CLI_MAX_ATTEMPTS 3

// Cooldown duration after max wrong attempts (ms)
#define CLI_COOLDOWN_MS 60000

// Max command line length
#define CLI_MAX_CMD_LEN 64

class TechnicianCLI {
public:
  TechnicianCLI(HardwareSerial& port);

  // Set the passphrase (call before begin)
  void setPassphrase(const char* pass);

  // Attach station components so CLI can interact with them
  void attachSlotManager(SlotManager* slots)       { _slots = slots; }
  void attachLogger(StationLogger* logger)         { _logger = logger; }
  void attachMemoryMonitor(MemoryMonitor* mem)     { _memMon = mem; }
  void attachWatchdog(WatchdogManager* wdt)        { _wdt = wdt; }

  // Call this in loop() — processes incoming serial commands
  // Non-blocking: returns immediately if no input
  void update();

  // Check if authenticated
  bool isAuthenticated() const { return _state == CLI_AUTHENTICATED; }

private:
  HardwareSerial& _serial;
  CLIState _state;

  char _passphrase[32];
  char _cmdBuffer[CLI_MAX_CMD_LEN];
  uint8_t _cmdPos;
  uint8_t _failedAttempts;
  uint32_t _cooldownStart;

  // Station components
  SlotManager*    _slots;
  StationLogger*  _logger;
  MemoryMonitor*  _memMon;
  WatchdogManager* _wdt;

  // Command processing
  void processCommand(const char* cmd);
  void showHelp();
  void showStatus();
  void showMemory();
  void resetSensor(uint8_t index);
  void readSensor(uint8_t index);
  void changeSensorRate(uint8_t index, uint8_t newRate);
  void toggleDebug(uint8_t index, bool enable);
  void prompt();
};
