#include "SimpleOTA.h"

#include <HTTPClient.h>
#include <Preferences.h>
#include <Update.h>
#include <WiFi.h>

static const char* OTA_PREF_NAMESPACE = "simple_ota";
static const char* OTA_PREF_PENDING_VERSION = "pending";

static void appendJsonEscaped(String& out, const char* value) {
  out += '"';
  if (value) {
    for (const char* p = value; *p; ++p) {
      if (*p == '"' || *p == '\\') {
        out += '\\';
        out += *p;
      } else if (*p == '\n') {
        out += F("\\n");
      } else if (*p == '\r') {
        out += F("\\r");
      } else if (*p == '\t') {
        out += F("\\t");
      } else {
        out += *p;
      }
    }
  }
  out += '"';
}

SimpleOTA::SimpleOTA()
    : _log(nullptr),
      _debug(false),
      _preferencesUsable(true),
      _lastServerVersion("") {
  _config = {};
  _lastError[0] = '\0';
}

void SimpleOTA::begin(const SimpleOTAConfig& config) {
  _config = config;
  _secureClient.setInsecure();
}

void SimpleOTA::setDebug(PrintController* printer, bool enable) {
  _log = printer;
  _debug = enable;
}

bool SimpleOTA::readyToCheck() const {
  if (!_config.enabled) return false;
  if (!_config.versionUrl || _config.versionUrl[0] == '\0') return false;
  if (WiFi.status() != WL_CONNECTED) return false;
  return true;
}

bool SimpleOTA::hasPendingUpdate() {
  return loadPendingVersion().length() > 0;
}

bool SimpleOTA::beginHttp(HTTPClient& http, const String& url) {
  if (url.startsWith("https://")) {
    return http.begin(_secureClient, url);
  }
  return http.begin(_plainClient, url);
}

bool SimpleOTA::confirmPendingUpdate() {
  const String pendingVersion = loadPendingVersion();
  if (pendingVersion.length() == 0) {
    return true;
  }

  const String currentVersion = _config.currentVersion ? _config.currentVersion : "";
  const bool versionMatches = (pendingVersion == currentVersion);
  const char* status = versionMatches ? "applied" : "booted_version_mismatch";
  const char* message = versionMatches ? "new firmware booted" : "current firmware version does not match pending OTA version";

  const bool posted = postConfirmation(status, pendingVersion, message);
  clearPendingVersion();
  return posted && versionMatches;
}

bool SimpleOTA::checkAndApplyUpdate() {
  _lastError[0] = '\0';
  _lastServerVersion = "";

  if (!readyToCheck()) {
    return false;
  }

  String serverVersion;
  String firmwareUrl;
  if (!fetchUpdateInfo(serverVersion, firmwareUrl)) {
    return false;
  }

  _lastServerVersion = serverVersion;

  const String currentVersion = _config.currentVersion ? _config.currentVersion : "";
  if (compareVersions(serverVersion, currentVersion) <= 0) {
    logText("[OTA] Firmware already current: ", currentVersion);
    return false;
  }

  if (firmwareUrl.length() == 0) {
    firmwareUrl = _config.fallbackFirmwareUrl ? _config.fallbackFirmwareUrl : "";
  }

  if (firmwareUrl.length() == 0) {
    setError("server reported update but no firmware URL is configured");
    return false;
  }

  logText("[OTA] New version available: ", serverVersion);
  postConfirmation("download_started", serverVersion, "firmware download started");
  return applyFirmwareFromUrl(firmwareUrl, serverVersion);
}

bool SimpleOTA::fetchUpdateInfo(String& version, String& firmwareUrl) {
  HTTPClient http;
  const String url = _config.versionUrl;

  if (!beginHttp(http, url)) {
    setError("could not start version HTTP request");
    return false;
  }

  http.setTimeout(_config.httpTimeoutMs);
  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    snprintf(_lastError, sizeof(_lastError), "version request HTTP %d", code);
    http.end();
    return false;
  }

  const String body = http.getString();
  http.end();

  if (!parseUpdateInfo(body, version, firmwareUrl)) {
    setError("version response did not contain a usable version");
    return false;
  }

  return true;
}

bool SimpleOTA::parseUpdateInfo(const String& body, String& version, String& firmwareUrl) const {
  version = "";
  firmwareUrl = "";

  String trimmed = body;
  trimmed.trim();
  if (trimmed.length() == 0) {
    return false;
  }

  if (trimmed[0] == '{') {
    extractJsonString(trimmed, "version", version);
    extractJsonString(trimmed, "firmware_url", firmwareUrl);
    if (firmwareUrl.length() == 0) {
      extractJsonString(trimmed, "url", firmwareUrl);
    }
  } else {
    const int lineEnd = trimmed.indexOf('\n');
    version = (lineEnd >= 0) ? trimmed.substring(0, lineEnd) : trimmed;
    version.trim();
  }

  version.trim();
  firmwareUrl.trim();
  return version.length() > 0;
}

bool SimpleOTA::extractJsonString(const String& body, const char* key, String& value) const {
  value = "";
  const String quotedKey = String("\"") + key + "\"";
  int keyIndex = body.indexOf(quotedKey);
  if (keyIndex < 0) return false;

  int colonIndex = body.indexOf(':', keyIndex + quotedKey.length());
  if (colonIndex < 0) return false;

  int firstQuote = body.indexOf('"', colonIndex + 1);
  if (firstQuote < 0) return false;

  int secondQuote = body.indexOf('"', firstQuote + 1);
  if (secondQuote < 0) return false;

  value = body.substring(firstQuote + 1, secondQuote);
  return true;
}

int SimpleOTA::compareVersions(const String& serverVersion, const String& currentVersion) const {
  int serverIndex = 0;
  int currentIndex = 0;

  while (serverIndex < (int)serverVersion.length() || currentIndex < (int)currentVersion.length()) {
    const uint32_t serverPart = readVersionPart(serverVersion, serverIndex);
    const uint32_t currentPart = readVersionPart(currentVersion, currentIndex);

    if (serverPart > currentPart) return 1;
    if (serverPart < currentPart) return -1;

    while (serverIndex < (int)serverVersion.length() && !isDigit(serverVersion[serverIndex])) serverIndex++;
    while (currentIndex < (int)currentVersion.length() && !isDigit(currentVersion[currentIndex])) currentIndex++;
  }

  return 0;
}

uint32_t SimpleOTA::readVersionPart(const String& value, int& index) const {
  while (index < (int)value.length() && !isDigit(value[index])) {
    index++;
  }

  uint32_t part = 0;
  while (index < (int)value.length() && isDigit(value[index])) {
    part = (part * 10UL) + (uint32_t)(value[index] - '0');
    index++;
  }

  return part;
}

bool SimpleOTA::applyFirmwareFromUrl(const String& firmwareUrl, const String& version) {
  HTTPClient http;
  if (!beginHttp(http, firmwareUrl)) {
    setError("could not start firmware HTTP request");
    postConfirmation("download_failed", version, _lastError);
    return false;
  }

  http.setTimeout(_config.httpTimeoutMs);
  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    snprintf(_lastError, sizeof(_lastError), "firmware request HTTP %d", code);
    http.end();
    postConfirmation("download_failed", version, _lastError);
    return false;
  }

  const int contentLength = http.getSize();
  const size_t updateSize = (contentLength > 0) ? (size_t)contentLength : UPDATE_SIZE_UNKNOWN;

  if (!Update.begin(updateSize)) {
    setError("not enough space for OTA image");
    http.end();
    postConfirmation("apply_failed", version, _lastError);
    return false;
  }

  WiFiClient* stream = http.getStreamPtr();
  const size_t written = Update.writeStream(*stream);
  if (contentLength > 0 && written != (size_t)contentLength) {
    setError("OTA image download size mismatch");
    Update.abort();
    http.end();
    postConfirmation("download_failed", version, _lastError);
    return false;
  }

  if (!Update.end()) {
    snprintf(_lastError, sizeof(_lastError), "OTA apply failed: %s", Update.errorString());
    http.end();
    postConfirmation("apply_failed", version, _lastError);
    return false;
  }

  if (!Update.isFinished()) {
    setError("OTA apply did not finish");
    http.end();
    postConfirmation("apply_failed", version, _lastError);
    return false;
  }

  http.end();
  savePendingVersion(version);
  postConfirmation("rebooting", version, "firmware applied, rebooting");
  logLine(F("[OTA] Firmware applied. Restarting."));
  delay(500);
  ESP.restart();
  return true;
}

bool SimpleOTA::postConfirmation(const char* status, const String& version, const char* message) {
  if (!_config.confirmUrl || _config.confirmUrl[0] == '\0') {
    return true;
  }
  if (WiFi.status() != WL_CONNECTED) {
    setError("cannot confirm OTA because Wi-Fi is disconnected");
    return false;
  }

  HTTPClient http;
  const String url = _config.confirmUrl;
  if (!beginHttp(http, url)) {
    setError("could not start OTA confirmation request");
    return false;
  }

  http.setTimeout(_config.httpTimeoutMs);
  http.addHeader("Content-Type", "application/json");

  String body;
  body.reserve(220);
  body += F("{\"station_id\":");
  appendJsonEscaped(body, _config.stationId ? _config.stationId : "");
  body += F(",\"firmware_version\":");
  appendJsonEscaped(body, _config.currentVersion ? _config.currentVersion : "");
  body += F(",\"reported_version\":");
  appendJsonEscaped(body, version.c_str());
  body += F(",\"status\":");
  appendJsonEscaped(body, status ? status : "");
  body += F(",\"message\":");
  appendJsonEscaped(body, message ? message : "");
  body += '}';

  const int code = http.POST(body);
  http.end();

  if (code < 200 || code >= 300) {
    snprintf(_lastError, sizeof(_lastError), "OTA confirmation HTTP %d", code);
    return false;
  }

  return true;
}

bool SimpleOTA::openPreferences(Preferences& prefs) {
  if (!_preferencesUsable) {
    return false;
  }

  if (prefs.begin(OTA_PREF_NAMESPACE, false)) {
    return true;
  }

  _preferencesUsable = false;
  logLine(F("[OTA] NVS Preferences unavailable; continuing without pending update state"));
  return false;
}

void SimpleOTA::savePendingVersion(const String& version) {
  Preferences prefs;
  if (!openPreferences(prefs)) {
    return;
  }

  const size_t written = prefs.putString(OTA_PREF_PENDING_VERSION, version);
  prefs.end();
  if (written == 0 && version.length() > 0) {
    _preferencesUsable = false;
    logLine(F("[OTA] Could not save pending update state; continuing without NVS"));
  }
}

String SimpleOTA::loadPendingVersion() {
  Preferences prefs;
  String version;
  // Open read-write so the namespace is created quietly on first boot. Opening
  // a missing namespace read-only makes ESP32 Preferences print
  // "nvs_open failed: NOT_FOUND", which looks like a firmware halt in logs.
  if (!openPreferences(prefs)) {
    return version;
  }

  if (prefs.isKey(OTA_PREF_PENDING_VERSION)) {
    version = prefs.getString(OTA_PREF_PENDING_VERSION, "");
  }
  prefs.end();
  return version;
}

void SimpleOTA::clearPendingVersion() {
  Preferences prefs;
  if (!openPreferences(prefs)) {
    return;
  }

  if (prefs.isKey(OTA_PREF_PENDING_VERSION)) {
    prefs.remove(OTA_PREF_PENDING_VERSION);
  }
  prefs.end();
}

void SimpleOTA::setError(const char* message) {
  strlcpy(_lastError, message ? message : "unknown OTA error", sizeof(_lastError));
  logText("[OTA] ", _lastError);
}

void SimpleOTA::logLine(const __FlashStringHelper* text) {
  if (_log && _debug) {
    _log->println(text, true);
  }
}

void SimpleOTA::logText(const char* prefix, const String& text) {
  if (!_log || !_debug) return;
  _log->print(prefix, true);
  _log->println(text.c_str(), true);
}
