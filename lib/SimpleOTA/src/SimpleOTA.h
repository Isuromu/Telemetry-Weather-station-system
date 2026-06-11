#pragma once

#include <Arduino.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include "PrintController.h"

struct SimpleOTAConfig {
  bool enabled;
  const char* stationId;
  const char* currentVersion;
  const char* versionUrl;
  const char* fallbackFirmwareUrl;
  const char* confirmUrl;
  uint32_t httpTimeoutMs;
};

class SimpleOTA {
public:
  SimpleOTA();

  void begin(const SimpleOTAConfig& config);
  void setDebug(PrintController* printer, bool enable);

  bool hasPendingUpdate();
  bool confirmPendingUpdate();
  bool checkAndApplyUpdate();

  const char* lastError() const { return _lastError; }
  const String& lastServerVersion() const { return _lastServerVersion; }

private:
  SimpleOTAConfig _config;
  PrintController* _log;
  bool _debug;
  bool _preferencesUsable;
  char _lastError[96];
  String _lastServerVersion;

  WiFiClient _plainClient;
  WiFiClientSecure _secureClient;

  bool readyToCheck() const;
  bool beginHttp(class HTTPClient& http, const String& url);
  bool fetchUpdateInfo(String& version, String& firmwareUrl);
  bool parseUpdateInfo(const String& body, String& version, String& firmwareUrl) const;
  bool extractJsonString(const String& body, const char* key, String& value) const;
  int compareVersions(const String& serverVersion, const String& currentVersion) const;
  uint32_t readVersionPart(const String& value, int& index) const;
  bool applyFirmwareFromUrl(const String& firmwareUrl, const String& version);
  bool postConfirmation(const char* status, const String& version, const char* message);
  bool openPreferences(class Preferences& prefs);
  void savePendingVersion(const String& version);
  String loadPendingVersion();
  void clearPendingVersion();
  void setError(const char* message);
  void logLine(const __FlashStringHelper* text);
  void logText(const char* prefix, const String& text);
};
