#pragma once
#include <Arduino.h>
#include "PrintController.h"
#include "StationLogger.h"

// ============================================================================
// NetworkManager — Upload Phase Controller
// ============================================================================
// Handles the entire network upload sequence:
//   1. Power ON network module, wait for warm-up
//   2. Check connection to network and server
//   3. Push new sensor data
//   4. Push any unsent/pending data (newest first)
//   5. Check server for config changes
//   6. Sync RTC via NTP (or GPS)
//   7. Check for OTA firmware updates
//   8. Power OFF network module
//
// This is an ABSTRACT BASE CLASS. You must create a child class for your
// specific network hardware (WiFi, SIM800, LoRa, etc.) and implement
// the pure virtual methods.
//
// Usage:
//   WiFiNetworkManager net(PIN_WIFI_POWER, PIN_WIFI_ENABLE, 60000);
//   net.runUploadPhase(logger);
// ============================================================================

// Network status
enum NetworkStatus {
  NET_OFF,          // Module powered off
  NET_WARMING_UP,   // Module powered on, waiting for warm-up
  NET_CONNECTING,   // Trying to connect
  NET_CONNECTED,    // Connected to network and server
  NET_ERROR         // Connection failed
};

// Upload result
struct UploadResult {
  bool dataPushed;       // New data uploaded OK
  bool pendingPushed;    // Old pending data uploaded OK
  bool configSynced;     // Server config changes applied
  bool rtcSynced;        // RTC time updated
  bool otaAvailable;     // OTA firmware update found
  bool otaApplied;       // OTA was applied (will reboot)
};

class NetworkManager {
public:
  // powerPin   = GPIO for network module MOSFET (255 = not used)
  // enablePin  = GPIO for network IC enable (255 = not used)
  // warmUpMs   = time to wait after power on (e.g., 60000 for WiFi router)
  NetworkManager(uint8_t powerPin = 255, uint8_t enablePin = 255,
                 uint32_t warmUpMs = 5000);

  virtual ~NetworkManager() {}

  // --- Upload Rate (system level, in minutes) ---
  void    setUploadRate(uint8_t minutes) { _uploadRateMin = minutes; }
  uint8_t getUploadRate() const { return _uploadRateMin; }

  // Check if it is time to upload
  bool isDueForUpload(uint32_t currentMillis);
  void markUploadTime(uint32_t currentMillis) { _lastUploadTime = currentMillis; }

  // --- Power Control ---
  void powerOn();
  void powerOff();
  NetworkStatus getStatus() const { return _status; }

  // --- Full Upload Phase (calls all steps in order) ---
  // Returns result struct with what succeeded/failed
  UploadResult runUploadPhase(StationLogger* logger = nullptr);

  // --- Pure Virtual Methods (implement in your network child class) ---

  // Connect to the network (WiFi, cellular, etc.)
  // Return true if connected
  virtual bool connect() = 0;

  // Check if connected to server
  virtual bool isConnected() = 0;

  // Push a data string to the server. Return true if ACK received.
  virtual bool pushData(const char* data) = 0;

  // Check server for config changes. Return true if changes found.
  // The child class should apply the changes internally.
  virtual bool checkServerConfig() = 0;

  // Sync RTC from NTP or other time source.
  // If synced, write time into the provided variables.
  // Return true if time was updated.
  virtual bool syncRTC(uint16_t &year, uint8_t &month, uint8_t &day,
                       uint8_t &hour, uint8_t &minute, uint8_t &second) = 0;

  // Check for OTA firmware update. Return true if update available.
  // If applyNow is true, download and apply (will reboot).
  virtual bool checkOTA(bool applyNow = false) = 0;

  // Disconnect from network
  virtual void disconnect() = 0;

  // Debug
  void setDebug(PrintController* printer, bool enable);

protected:
  uint8_t  _powerPin;
  uint8_t  _enablePin;
  uint32_t _warmUpMs;

  uint8_t  _uploadRateMin;
  uint32_t _lastUploadTime;

  NetworkStatus _status;

  PrintController* _log;
  bool _debugEnable;
};
