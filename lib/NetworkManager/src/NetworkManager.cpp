#include "NetworkManager.h"

// ============================================================================
// Constructor
// ============================================================================
NetworkManager::NetworkManager(uint8_t powerPin, uint8_t enablePin, uint32_t warmUpMs)
  : _powerPin(powerPin), _enablePin(enablePin), _warmUpMs(warmUpMs),
    _uploadRateMin(60), _lastUploadTime(0),
    _status(NET_OFF), _log(nullptr), _debugEnable(false) {}

// ============================================================================
// isDueForUpload
// ============================================================================
bool NetworkManager::isDueForUpload(uint32_t currentMillis) {
  uint32_t intervalMs = (uint32_t)_uploadRateMin * 60UL * 1000UL;
  return (currentMillis - _lastUploadTime) >= intervalMs;
}

// ============================================================================
// powerOn — turn on network module and wait for warm-up
// ============================================================================
void NetworkManager::powerOn() {
  if (_powerPin != 255) {
    pinMode(_powerPin, OUTPUT);
    digitalWrite(_powerPin, HIGH);
  }
  if (_enablePin != 255) {
    pinMode(_enablePin, OUTPUT);
    digitalWrite(_enablePin, HIGH);
  }

  _status = NET_WARMING_UP;

  if (_log) {
    _log->print(F("[Net] Power ON, waiting "), _debugEnable);
    _log->print((unsigned long)(_warmUpMs / 1000), _debugEnable);
    _log->println(F(" sec warm-up..."), _debugEnable);
  }

  delay(_warmUpMs);  // Wait for module to boot
  _status = NET_CONNECTING;
}

// ============================================================================
// powerOff — turn off network module
// ============================================================================
void NetworkManager::powerOff() {
  disconnect();

  if (_enablePin != 255) {
    digitalWrite(_enablePin, LOW);
  }
  if (_powerPin != 255) {
    digitalWrite(_powerPin, LOW);
  }

  _status = NET_OFF;

  if (_log) {
    _log->println(F("[Net] Power OFF"), _debugEnable);
  }
}

// ============================================================================
// runUploadPhase — executes the complete upload sequence
// ============================================================================
UploadResult NetworkManager::runUploadPhase(StationLogger* logger) {
  UploadResult result = {false, false, false, false, false, false};

  // --- Step 1: Power on ---
  powerOn();

  // --- Step 2: Connect ---
  if (_log) _log->println(F("[Net] Connecting..."), _debugEnable);

  if (!connect()) {
    _status = NET_ERROR;
    if (_log) _log->println(F("[Net] Connection FAILED!"), _debugEnable);
    if (logger) logger->logError("NET: connection failed");
    powerOff();
    return result;
  }

  _status = NET_CONNECTED;
  if (_log) _log->println(F("[Net] Connected OK"), _debugEnable);
  if (logger) logger->logAction("NET: connected");

  // --- Step 3: Push new data ---
  // (The main code should prepare the data string before calling this)
  // For now, this is a placeholder — the child class pushData() handles it
  if (_log) _log->println(F("[Net] Pushing new data..."), _debugEnable);
  result.dataPushed = true;  // Main code calls pushData() separately

  // --- Step 4: Push pending/unsent data ---
  if (_log) _log->println(F("[Net] Checking for pending data..."), _debugEnable);
  result.pendingPushed = true;  // Main code handles pending queue

  // --- Step 5: Check server for config changes ---
  if (_log) _log->println(F("[Net] Checking server config..."), _debugEnable);
  result.configSynced = checkServerConfig();
  if (result.configSynced) {
    if (_log) _log->println(F("[Net] Config updated from server"), _debugEnable);
    if (logger) logger->logAction("NET: config synced from server");
  }

  // --- Step 6: Sync RTC ---
  if (_log) _log->println(F("[Net] Syncing RTC..."), _debugEnable);
  uint16_t y; uint8_t mo, d, h, mi, s;
  result.rtcSynced = syncRTC(y, mo, d, h, mi, s);
  if (result.rtcSynced) {
    if (logger) {
      logger->setDateTime(y, mo, d, h, mi, s);
      logger->logAction("NET: RTC synced");
    }
    if (_log) _log->println(F("[Net] RTC synced OK"), _debugEnable);
  }

  // --- Step 7: Check for OTA ---
  if (_log) _log->println(F("[Net] Checking OTA..."), _debugEnable);
  result.otaAvailable = checkOTA(false);  // Check only, don't apply yet
  if (result.otaAvailable) {
    if (_log) _log->println(F("[Net] OTA update available!"), _debugEnable);
    if (logger) logger->logAction("NET: OTA update available");
    // Apply OTA (will reboot)
    result.otaApplied = checkOTA(true);
  }

  // --- Step 8: Power off ---
  markUploadTime(millis());
  powerOff();

  if (logger) logger->logAction("NET: upload phase complete");
  return result;
}

// ============================================================================
// setDebug
// ============================================================================
void NetworkManager::setDebug(PrintController* printer, bool enable) {
  _log = printer;
  _debugEnable = enable;
}
