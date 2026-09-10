#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <RadioLib.h>
#include <SPI.h>
#include <string.h>
#if __has_include("lorawan_keys.h")
#include "lorawan_keys.h"
#else
#include "lorawan_keys_default.h"
#endif

class ValveLoRaWan {
 public:
  using CommandHandler = void (*)(String command);
  using StatusBuilder = size_t (*)(uint8_t *payload, size_t capacity);

  ValveLoRaWan(CommandHandler commandHandler, StatusBuilder statusBuilder)
      : commandHandler_(commandHandler),
        statusBuilder_(statusBuilder),
        spiSettings_(2000000, MSBFIRST, SPI_MODE0),
        module_(kNssPin, kDio1Pin, kResetPin, kBusyPin, SPI, spiSettings_),
        radio_(&module_),
        node_(&radio_, &kRegion, 0) {}

  bool begin() {
    ready_ = false;
    lastJoinAttemptMs_ = millis();
    if (!initialized_) {
      SPI.begin(kSckPin, kMisoPin, kMosiPin, kNssPin);
      int16_t state = radio_.begin(868.0, 125.0, 9, 7,
                                   RADIOLIB_SX126X_SYNC_WORD_PRIVATE, 14, 8,
                                   0.0, false);
      if (state != RADIOLIB_ERR_NONE) return logError("SX1262 init", state);
      radio_.setRfSwitchPins(kRxEnablePin, kTxEnablePin);

      state = node_.beginOTAA(LoRaKeys::joinEui, LoRaKeys::devEui, nullptr, LoRaKeys::appKey);
      if (state != RADIOLIB_ERR_NONE) return logError("OTAA config", state);
      node_.setADR(true);
      node_.setDutyCycle(true);
      restoreNonces();
      restoreSession();
      initialized_ = true;
    }
    Serial.println(F("[LORAWAN] OTAA join / session restore..."));
    int16_t state = node_.activateOTAA();
    if (!saveNonces()) return logError("OTAA nonce save", -1);
    if (state != RADIOLIB_LORAWAN_NEW_SESSION &&
        state != RADIOLIB_LORAWAN_SESSION_RESTORED) {
      return logError("OTAA join", state);
    }

    state = node_.setClass(RADIOLIB_LORAWAN_CLASS_C);
    if (state != RADIOLIB_ERR_NONE) return logError("Class C", state);
    ready_ = true;
    if (!saveSession()) {
      ready_ = false;
      return logError("session save", -1);
    }
    statusPending_ = true;
    lastUplinkMs_ = millis();
    Serial.println(F("[LORAWAN] Valve Class C active; status interval 2 seconds (airtime limits apply)."));
    return true;
  }

  void requestStatus() { statusPending_ = true; }
  bool isReady() const { return ready_; }

  void poll() {
    if (storageFailed_) return;
    if (!ready_) {
      if (millis() - lastJoinAttemptMs_ >= kJoinRetryMs) begin();
      return;
    }
    receiveClassC();
    if (!ready_) return;
    if (millis() - lastUplinkMs_ >= kUplinkIntervalMs) statusPending_ = true;
    // RadioLib checks regional airtime limits; retain pending status meanwhile.
    if (statusPending_ && millis() - lastSendAttemptMs_ >= kUplinkIntervalMs &&
        node_.timeUntilUplink() == 0) {
      lastSendAttemptMs_ = millis();
      statusPending_ = false;
      if (sendStatus(false)) lastUplinkMs_ = lastSendAttemptMs_;
      else statusPending_ = true;
    }
  }

 private:
  static constexpr int kMosiPin = 23;
  static constexpr int kMisoPin = 19;
  static constexpr int kSckPin = 18;
  static constexpr int kNssPin = 5;
  static constexpr int kResetPin = 14;
  static constexpr int kBusyPin = 25;
  static constexpr int kDio1Pin = 26;
  static constexpr int kRxEnablePin = 33;
  static constexpr int kTxEnablePin = 32;
  inline static const LoRaWANBand_t kRegion = EU868;
  static constexpr uint8_t kFPort = 10;
  static constexpr uint32_t kUplinkIntervalMs = 2000UL;
  static constexpr char kNvsNamespace[] = "valve_lora";
  static constexpr char kNonceKey[] = "nonces";
  static constexpr char kSessionKey[] = "session";


  CommandHandler commandHandler_;
  StatusBuilder statusBuilder_;
  SPISettings spiSettings_;
  Module module_;
  SX1262 radio_;
  LoRaWANNode node_;
  Preferences preferences_;
  bool ready_{false};
  bool initialized_{false};
  bool storageFailed_{false};
  bool statusPending_{false};
  static constexpr uint32_t kJoinRetryMs = 60000UL;
  uint32_t lastJoinAttemptMs_{0};
  uint32_t lastSendAttemptMs_{0};
  uint32_t lastUplinkMs_{0};

  bool logError(const char *where, int16_t state) {
    if (strstr(where, "save") != nullptr) {
      ready_ = false;
      storageFailed_ = true;
    }
    Serial.printf("[LORAWAN][ERROR] %s: %d\n", where, state);
    return false;
  }

  void restoreNonces() {
    if (!preferences_.begin(kNvsNamespace, true)) return;
    uint8_t nonces[RADIOLIB_LORAWAN_NONCES_BUF_SIZE];
    if (preferences_.getBytesLength(kNonceKey) == sizeof(nonces) &&
        preferences_.getBytes(kNonceKey, nonces, sizeof(nonces)) == sizeof(nonces)) {
      node_.setBufferNonces(nonces);
    }
    preferences_.end();
  }

  bool saveNonces() {
    if (!preferences_.begin(kNvsNamespace, false)) return false;
    const size_t written = preferences_.putBytes(
        kNonceKey, node_.getBufferNonces(), RADIOLIB_LORAWAN_NONCES_BUF_SIZE);
    preferences_.end();
    return written == RADIOLIB_LORAWAN_NONCES_BUF_SIZE;
  }

  void restoreSession() {
    if (!preferences_.begin(kNvsNamespace, true)) return;
    uint8_t session[RADIOLIB_LORAWAN_SESSION_BUF_SIZE];
    if (preferences_.getBytesLength(kSessionKey) == sizeof(session) &&
        preferences_.getBytes(kSessionKey, session, sizeof(session)) == sizeof(session)) {
      node_.setBufferSession(session);
    }
    preferences_.end();
  }

  bool saveSession() {
    if (!preferences_.begin(kNvsNamespace, false)) return false;
    const size_t written = preferences_.putBytes(
        kSessionKey, node_.getBufferSession(), RADIOLIB_LORAWAN_SESSION_BUF_SIZE);
    preferences_.end();
    return written == RADIOLIB_LORAWAN_SESSION_BUF_SIZE;
  }

  bool sendStatus(bool confirmed) {
    uint8_t uplink[16] = {0};
    const size_t length = statusBuilder_(uplink, sizeof(uplink));
    if (length == 0 || length > sizeof(uplink)) return logError("status payload", -1);

    // RadioLib 7.7.1 removed RADIOLIB_LORAWAN_MAX_DOWNLINK_SIZE (250 in
    // 7.6.0). MAX_PAYLOAD_SIZE is the 242-byte application-payload maximum
    // that sendReceive/getDownlinkClassC actually write here.
    uint8_t downlink[RADIOLIB_LORAWAN_MAX_PAYLOAD_SIZE] = {0};
    size_t downlinkLength = 0;
    LoRaWANEvent_t event{};
    const int16_t state = node_.sendReceive(uplink, length, kFPort, downlink,
                                            &downlinkLength, confirmed, nullptr, &event);
    if (!saveSession()) return logError("session save", -1);
    if (state < RADIOLIB_ERR_NONE) {
      if (state == RADIOLIB_ERR_NETWORK_NOT_JOINED) ready_ = false;
      return logError("uplink", state);
    }
    Serial.printf("[LORAWAN] Uplink: %u bytes, port %u\n", unsigned(length), kFPort);
    if (state > RADIOLIB_ERR_NONE && downlinkLength > 0) {
      processDownlink(downlink, downlinkLength, event.fPort);
    }
    return true;
  }

  void receiveClassC() {
    // RadioLib 7.7.1 removed RADIOLIB_LORAWAN_MAX_DOWNLINK_SIZE (250 in
    // 7.6.0). MAX_PAYLOAD_SIZE is the 242-byte application-payload maximum
    // that sendReceive/getDownlinkClassC actually write here.
    uint8_t downlink[RADIOLIB_LORAWAN_MAX_PAYLOAD_SIZE] = {0};
    size_t downlinkLength = 0;
    LoRaWANEvent_t event{};
    const int16_t state = node_.getDownlinkClassC(downlink, &downlinkLength, &event);
    if (state > RADIOLIB_ERR_NONE) {
      if (!saveSession()) { logError("session save", -1); return; }
      processDownlink(downlink, downlinkLength, event.fPort);
    }
  }

  void processDownlink(const uint8_t *data, size_t length, uint8_t port) {
    if (port != kFPort || length == 0 || length > 40) return;
    for (size_t i = 0; i < length; ++i) {
      if (data[i] < 0x20 || data[i] > 0x7e) return;
    }
    char command[96];
    memcpy(command, data, length);
    command[length] = '\0';
    String text(command);
    text.trim();
    text.toLowerCase();
    if (text != "open" && text != "close" && text != "status") {
      Serial.println(F("[LORAWAN][ERROR] Command rejected."));
      return;
    }
    Serial.printf("[LORAWAN] Command: %s\n", text.c_str());
    commandHandler_(text);
    requestStatus();
  }
};