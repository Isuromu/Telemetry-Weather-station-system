#include "TechnicianCLI.h"

// ============================================================================
// Constructor
// ============================================================================
TechnicianCLI::TechnicianCLI(HardwareSerial& port)
  : _serial(port), _state(CLI_LOCKED),
    _cmdPos(0), _failedAttempts(0), _cooldownStart(0),
    _slots(nullptr), _logger(nullptr), _memMon(nullptr), _wdt(nullptr) {
  strncpy(_passphrase, DEFAULT_CLI_PASSPHRASE, sizeof(_passphrase));
  memset(_cmdBuffer, 0, CLI_MAX_CMD_LEN);
}

// ============================================================================
// setPassphrase
// ============================================================================
void TechnicianCLI::setPassphrase(const char* pass) {
  strncpy(_passphrase, pass, sizeof(_passphrase) - 1);
  _passphrase[sizeof(_passphrase) - 1] = '\0';
}

// ============================================================================
// update — call in loop(), processes serial input non-blocking
// ============================================================================
void TechnicianCLI::update() {
  // Check cooldown
  if (_state == CLI_COOLDOWN) {
    if (millis() - _cooldownStart >= CLI_COOLDOWN_MS) {
      _state = CLI_LOCKED;
      _failedAttempts = 0;
      _serial.println(F("[CLI] Cooldown expired. Enter passphrase:"));
    }
    return;
  }

  // Read available characters
  while (_serial.available()) {
    char c = _serial.read();

    // Enter key = process command
    if (c == '\n' || c == '\r') {
      if (_cmdPos > 0) {
        _cmdBuffer[_cmdPos] = '\0';

        if (_state == CLI_LOCKED) {
          // Check passphrase
          if (strcmp(_cmdBuffer, _passphrase) == 0) {
            _state = CLI_AUTHENTICATED;
            _failedAttempts = 0;
            _serial.println(F("\n[CLI] Authenticated OK!"));
            showHelp();
            prompt();
          } else {
            _failedAttempts++;
            if (_failedAttempts >= CLI_MAX_ATTEMPTS) {
              _state = CLI_COOLDOWN;
              _cooldownStart = millis();
              _serial.println(F("[CLI] Too many attempts! Locked for 60 seconds."));
            } else {
              _serial.print(F("[CLI] Wrong passphrase. Attempts left: "));
              _serial.println(CLI_MAX_ATTEMPTS - _failedAttempts);
            }
          }
        } else {
          // Process command
          processCommand(_cmdBuffer);
          prompt();
        }

        _cmdPos = 0;
        memset(_cmdBuffer, 0, CLI_MAX_CMD_LEN);
      }
      continue;
    }

    // Backspace
    if (c == '\b' || c == 127) {
      if (_cmdPos > 0) _cmdPos--;
      continue;
    }

    // Store character
    if (_cmdPos < CLI_MAX_CMD_LEN - 1) {
      _cmdBuffer[_cmdPos++] = c;
    }
  }
}

// ============================================================================
// processCommand
// ============================================================================
void TechnicianCLI::processCommand(const char* cmd) {
  // Parse command and arguments
  if (strcmp(cmd, "help") == 0) {
    showHelp();
  }
  else if (strcmp(cmd, "status") == 0) {
    showStatus();
  }
  else if (strcmp(cmd, "memory") == 0) {
    showMemory();
  }
  else if (strncmp(cmd, "read ", 5) == 0) {
    uint8_t idx = atoi(cmd + 5);
    readSensor(idx);
  }
  else if (strncmp(cmd, "reset ", 6) == 0) {
    uint8_t idx = atoi(cmd + 6);
    resetSensor(idx);
  }
  else if (strncmp(cmd, "rate ", 5) == 0) {
    // Parse: "rate <index> <minutes>"
    uint8_t idx = atoi(cmd + 5);
    const char* space = strchr(cmd + 5, ' ');
    if (space) {
      uint8_t newRate = atoi(space + 1);
      changeSensorRate(idx, newRate);
    } else {
      _serial.println(F("Usage: rate <index> <minutes>"));
    }
  }
  else if (strncmp(cmd, "debug ", 6) == 0) {
    // Parse: "debug <index> <0|1>"
    uint8_t idx = atoi(cmd + 6);
    const char* space = strchr(cmd + 6, ' ');
    if (space) {
      bool en = atoi(space + 1) != 0;
      toggleDebug(idx, en);
    } else {
      _serial.println(F("Usage: debug <index> <0|1>"));
    }
  }
  else if (strcmp(cmd, "reboot") == 0) {
    _serial.println(F("[CLI] Rebooting..."));
    delay(500);
#if defined(ARDUINO_ARCH_ESP32)
    ESP.restart();
#elif defined(ARDUINO_ARCH_AVR)
    void (*resetFunc)(void) = 0;
    resetFunc();
#endif
  }
  else if (strcmp(cmd, "exit") == 0) {
    _state = CLI_LOCKED;
    _serial.println(F("[CLI] Session locked. Enter passphrase to reconnect."));
  }
  else {
    _serial.print(F("Unknown command: "));
    _serial.println(cmd);
    _serial.println(F("Type 'help' for available commands."));
  }
}

// ============================================================================
// showHelp
// ============================================================================
void TechnicianCLI::showHelp() {
  _serial.println(F("\n--- Technician CLI Commands ---"));
  _serial.println(F("  status         Show all sensors and slot usage"));
  _serial.println(F("  memory         Show RAM and storage info"));
  _serial.println(F("  read <n>       Force-read sensor at index n"));
  _serial.println(F("  reset <n>      Reset sensor n from OFFLINE to ONLINE"));
  _serial.println(F("  rate <n> <min> Change sensor n sample rate"));
  _serial.println(F("  debug <n> <0|1> Toggle debug for sensor n"));
  _serial.println(F("  reboot         Restart the MCU"));
  _serial.println(F("  exit           Lock CLI session"));
  _serial.println(F("  help           Show this help"));
  _serial.println();
}

// ============================================================================
// showStatus
// ============================================================================
void TechnicianCLI::showStatus() {
  if (_slots) {
    _slots->printStatus();
  } else {
    _serial.println(F("[CLI] SlotManager not attached."));
  }
}

// ============================================================================
// showMemory
// ============================================================================
void TechnicianCLI::showMemory() {
  if (_memMon) {
    _memMon->printStatus();
  } else {
    _serial.println(F("[CLI] MemoryMonitor not attached."));
  }
}

// ============================================================================
// readSensor
// ============================================================================
void TechnicianCLI::readSensor(uint8_t index) {
  if (!_slots) { _serial.println(F("[CLI] No SlotManager.")); return; }

  SensorDriver* s = _slots->getSensor(index);
  if (!s) { _serial.println(F("[CLI] Invalid sensor index.")); return; }

  _serial.print(F("[CLI] Reading sensor "));
  _serial.print(index);
  _serial.println(F("..."));

  bool ok = s->readData();
  _serial.print(F("[CLI] Result: "));
  _serial.println(ok ? F("OK") : F("FAILED"));
}

// ============================================================================
// resetSensor
// ============================================================================
void TechnicianCLI::resetSensor(uint8_t index) {
  if (!_slots) { _serial.println(F("[CLI] No SlotManager.")); return; }

  SensorDriver* s = _slots->getSensor(index);
  if (!s) { _serial.println(F("[CLI] Invalid sensor index.")); return; }

  s->resetStatus();
  _serial.print(F("[CLI] Sensor "));
  _serial.print(index);
  _serial.println(F(" reset to ONLINE."));

  if (_logger) {
    _logger->logAction("CLI: sensor reset to ONLINE");
  }
}

// ============================================================================
// changeSensorRate
// ============================================================================
void TechnicianCLI::changeSensorRate(uint8_t index, uint8_t newRate) {
  if (!_slots) { _serial.println(F("[CLI] No SlotManager.")); return; }

  SensorDriver* s = _slots->getSensor(index);
  if (!s) { _serial.println(F("[CLI] Invalid sensor index.")); return; }

  bool ok = _slots->changeSensorRate(s, newRate);
  if (ok) {
    _serial.print(F("[CLI] Sensor "));
    _serial.print(index);
    _serial.print(F(" rate changed to "));
    _serial.print(newRate);
    _serial.println(F(" min."));
  } else {
    _serial.println(F("[CLI] Rate change rejected (slot full)."));
  }
}

// ============================================================================
// toggleDebug
// ============================================================================
void TechnicianCLI::toggleDebug(uint8_t index, bool enable) {
  if (!_slots) { _serial.println(F("[CLI] No SlotManager.")); return; }

  SensorDriver* s = _slots->getSensor(index);
  if (!s) { _serial.println(F("[CLI] Invalid sensor index.")); return; }

  s->setDebug(enable);
  _serial.print(F("[CLI] Sensor "));
  _serial.print(index);
  _serial.print(F(" debug "));
  _serial.println(enable ? F("ENABLED") : F("DISABLED"));
}

// ============================================================================
// prompt
// ============================================================================
void TechnicianCLI::prompt() {
  _serial.print(F("station> "));
}
