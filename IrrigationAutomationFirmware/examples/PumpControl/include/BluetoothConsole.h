#pragma once

// USB-free diagnostics for PumpNode. The pump's VFD/RS-485 environment can drop
// the Windows USB-UART link while the ESP32 keeps running, so the Bluetooth
// console carries the same log output and the same commands over an SPP stream.
// It reuses the USB password, but prompts on every client connect and accepts
// the password at any time, so a deployed node never needs a reset to unlock.

#include <Arduino.h>
#include <Print.h>
#include <SerialAuthConfig.h>
#include <Stream.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>

// Set to 0 by a build flag to compile the wireless channel out for a hardened
// deployment. The LogTee below stays real; it then fans out to a no-op sink.
#ifndef PUMP_BT_CONSOLE
#define PUMP_BT_CONSOLE 1
#endif

// Fans log output to every attached sink. Each sink gates itself, so a locked or
// absent channel discards its copy instead of blocking the other.
//
// The secondary sink receives whole lines: bytes are buffered and forwarded in
// one call when a newline or the buffer limit is reached. Bluetooth Serial
// raises one packet per write call, so forwarding per byte would flood its
// 32-slot TX queue and stall the control loop for up to SPP_TX_QUEUE_TIMEOUT.
class LogTee : public Print {
 public:
  LogTee(Print& primary, Print& secondary)
      : primary_(primary), secondary_(secondary) {}

  size_t write(uint8_t byte) override {
    const size_t written = primary_.write(byte);
    appendToSecondary(byte);
    return written;
  }

  size_t write(const uint8_t* buffer, size_t size) override {
    const size_t written = primary_.write(buffer, size);
    for (size_t index = 0; index < size; ++index) {
      appendToSecondary(buffer[index]);
    }
    return written;
  }
  using Print::write;

  size_t printf(const char* format, ...)
      __attribute__((format(printf, 2, 3))) {
    char buffer[256];
    va_list arguments;
    va_start(arguments, format);
    const int written = vsnprintf(buffer, sizeof(buffer), format, arguments);
    va_end(arguments);
    if (written <= 0) return 0;
    const size_t length = static_cast<size_t>(written);
    return write(reinterpret_cast<const uint8_t*>(buffer),
                 length < sizeof(buffer) ? length : sizeof(buffer) - 1);
  }

  void flush() override {
    primary_.flush();
    drainSecondary();
  }

 private:
  static constexpr size_t kSecondaryLineCapacity = 256;

  Print& primary_;
  Print& secondary_;
  uint8_t secondaryLine_[kSecondaryLineCapacity]{};
  size_t secondaryLength_{0};

  void appendToSecondary(uint8_t byte) {
    secondaryLine_[secondaryLength_++] = byte;
    if (byte == '\n' || secondaryLength_ >= sizeof(secondaryLine_)) {
      drainSecondary();
    }
  }

  void drainSecondary() {
    if (secondaryLength_ == 0) return;
    secondary_.write(secondaryLine_, secondaryLength_);
    secondaryLength_ = 0;
  }
};

// Included unconditionally, ahead of the PUMP_BT_CONSOLE branch: PlatformIO's
// dependency finder only scans src/, so main.cpp includes it as well to make
// LDF pull in the framework BluetoothSerial library. The type is referenced
// only in the PUMP_BT_CONSOLE branch below, so a disabled build links none of
// it.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include <BluetoothSerial.h>
#pragma GCC diagnostic pop

#if PUMP_BT_CONSOLE

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

// Bluetooth Classic SPP console gated by the shared password. The password is
// requested once per client connection and accepted at any time while
// connected; disconnecting relocks it.
class WirelessConsole final : public Stream {
 public:
  // Starts Bluetooth Classic SPP. BLE is disabled to save RAM. Give the device
  // a per-unit name, for example "PumpNode-A1B2".
  bool begin(const char* deviceName) {
    clearInput();
    unlocked_ = false;
    hadClient_ = false;
    return port_.begin(deviceName, false, /*disableBLE=*/true);
  }

  // Detects connect/disconnect, prompts on a new connection, and reads the
  // password while locked. Safe to call every loop iteration.
  void poll() {
    const bool clientConnected = port_.hasClient();
    if (clientConnected != hadClient_) {
      hadClient_ = clientConnected;
      clearInput();
      unlocked_ = false;
      if (clientConnected) {
        rawPrint(F("[WIRELESS AUTH] Diagnostics locked. Enter password: "));
      }
    }
    if (!clientConnected || unlocked_) return;
    processLockedInput();
  }

  bool clientConnected() { return port_.hasClient(); }
  bool unlocked() const { return unlocked_; }

  int available() override {
    poll();
    return unlocked_ ? port_.available() : 0;
  }

  int read() override {
    poll();
    return unlocked_ ? port_.read() : -1;
  }

  int peek() override {
    poll();
    return unlocked_ ? port_.peek() : -1;
  }

  void flush() override {
    // BluetoothSerial::flush() busy-waits without a timeout and can hang the
    // control loop on a congested link. Never call it.
  }

  size_t write(uint8_t byte) override {
    if (!unlocked_ || !port_.hasClient()) return 0;
    return port_.write(byte);
  }

  size_t write(const uint8_t* buffer, size_t size) override {
    if (!unlocked_ || !port_.hasClient()) return 0;
    return port_.write(buffer, size);
  }
  using Print::write;

 private:
  static constexpr size_t kInputCapacity = SERIAL_AUTH_MAX_INPUT_LENGTH + 1;

  BluetoothSerial port_;
  char input_[kInputCapacity]{};
  size_t inputLength_{0};
  bool inputOverflow_{false};
  bool unlocked_{false};
  bool hadClient_{false};

  void clearInput() {
    input_[0] = '\0';
    inputLength_ = 0;
    inputOverflow_ = false;
  }

  void processLockedInput() {
    while (port_.available() > 0) {
      const int value = port_.read();
      if (value < 0) return;
      const char character = static_cast<char>(value);

      if (character == '\r' || character == '\n') {
        if (inputLength_ > 0 || inputOverflow_) acceptOrRejectPassword();
        continue;
      }

      if (inputLength_ + 1 < kInputCapacity) {
        input_[inputLength_++] = character;
        input_[inputLength_] = '\0';
      } else {
        inputOverflow_ = true;
      }
    }
  }

  void acceptOrRejectPassword() {
    if (!inputOverflow_ && passwordMatches()) {
      unlocked_ = true;
      rawPrint(F("[WIRELESS AUTH] unlocked for this connection.\n"));
    } else {
      rawPrint(
          F("[WIRELESS AUTH] wrong password.\n[WIRELESS AUTH] Enter password: "));
    }
    clearInput();
  }

  bool passwordMatches() const {
    const char* const password = SERIAL_AUTH_PASSWORD;
    const size_t passwordLength = strlen(password);
    if (passwordLength != inputLength_) return false;

    uint8_t difference = 0;
    for (size_t index = 0; index < passwordLength; ++index) {
      difference |= static_cast<uint8_t>(input_[index] ^ password[index]);
    }
    return difference == 0;
  }

  void rawPrint(const __FlashStringHelper* text) { port_.print(text); }
  void rawPrint(const char* text) { port_.print(text); }
};

#pragma GCC diagnostic pop

#else  // PUMP_BT_CONSOLE

// Compiled out: a sink that owns no radio and never unlocks.
class WirelessConsole final : public Stream {
 public:
  bool begin(const char* deviceName) {
    (void)deviceName;
    return false;
  }
  void poll() {}
  bool clientConnected() { return false; }
  bool unlocked() const { return false; }
  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
  void flush() override {}
  size_t write(uint8_t) override { return 0; }
  size_t write(const uint8_t*, size_t) override { return 0; }
  using Print::write;
};

#endif  // PUMP_BT_CONSOLE
