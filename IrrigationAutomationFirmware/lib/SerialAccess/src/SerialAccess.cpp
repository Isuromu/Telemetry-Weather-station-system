#include "SerialAccess.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

#if defined(ARDUINO_ARCH_ESP32)
#include <esp_system.h>
#endif

namespace irrigation {
namespace {

#if defined(ARDUINO_ARCH_ESP32)
RTC_DATA_ATTR bool serialAuthUnlockedAfterDeepSleep = false;
#endif

constexpr bool authGateEnabled() {
  return ENABLE_SERIAL_AUTH_GATE != 0;
}

}  // namespace

SerialAccess::SerialAccess(HardwareSerial& port) : port_(port) {}

void SerialAccess::begin(unsigned long baud) {
  port_.begin(baud);
  beginAuthState();
}

void SerialAccess::begin(unsigned long baud, uint32_t config) {
  port_.begin(baud, config);
  beginAuthState();
}

void SerialAccess::setTimeout(unsigned long timeout) {
  Stream::setTimeout(timeout);
  port_.setTimeout(timeout);
}

void SerialAccess::beginAuthState() {
  clearInput();
  authStartedMs_ = millis();
  authExpired_ = false;

  if (!authGateEnabled()) {
    unlocked_ = SERIAL_DEBUG_DEFAULT != 0;
    return;
  }

#if defined(ARDUINO_ARCH_ESP32)
  const bool deepSleepWake = esp_reset_reason() == ESP_RST_DEEPSLEEP;
  unlocked_ = deepSleepWake && serialAuthUnlockedAfterDeepSleep;
  if (!deepSleepWake) serialAuthUnlockedAfterDeepSleep = false;
  if (deepSleepWake && !unlocked_) {
    // A locked low-power node must not spend the authentication window awake
    // on every timer wake. Authenticate after a cold reset instead.
    authExpired_ = true;
    return;
  }
#else
  unlocked_ = false;
#endif

  if (unlocked_) {
    rawPrint(F("[SERIAL AUTH] restored after timer deep sleep.\n"));
  } else {
    rawPrint(F("[SERIAL AUTH] Diagnostics locked. Enter password: "));
  }
}

void SerialAccess::poll() {
  if (unlocked_ || !authGateEnabled()) return;

  if (SERIAL_AUTH_TIMEOUT_MS != 0 &&
      static_cast<uint32_t>(millis() - authStartedMs_) >=
          SERIAL_AUTH_TIMEOUT_MS) {
    if (!authExpired_) {
      authExpired_ = true;
      clearInput();
      while (port_.available() > 0) (void)port_.read();
      rawPrint(F("[SERIAL AUTH] timeout; reset to retry.\n"));
    }
    return;
  }

  processLockedInput();
}

bool SerialAccess::waitForAuthentication() {
  if (!authGateEnabled() || unlocked_) return unlocked_;

  while (!unlocked_ && !authExpired_) {
    poll();
    if (!unlocked_ && !authExpired_) delay(10);
  }
  return unlocked_;
}

bool SerialAccess::unlocked() const { return unlocked_; }

int SerialAccess::available() {
  poll();
  return unlocked_ ? port_.available() : 0;
}

int SerialAccess::read() {
  poll();
  return unlocked_ ? port_.read() : -1;
}

int SerialAccess::peek() {
  poll();
  return unlocked_ ? port_.peek() : -1;
}

void SerialAccess::flush() {
  if (unlocked_) port_.flush();
}

size_t SerialAccess::write(uint8_t byte) {
  poll();
  return unlocked_ ? port_.write(byte) : 0;
}

size_t SerialAccess::printf(const char* format, ...) {
  poll();
  if (!unlocked_) return 0;

  char buffer[256];
  va_list arguments;
  va_start(arguments, format);
  const int written = vsnprintf(buffer, sizeof(buffer), format, arguments);
  va_end(arguments);
  if (written <= 0) return 0;

  const size_t length = static_cast<size_t>(written);
  return port_.write(reinterpret_cast<const uint8_t*>(buffer),
                     length < sizeof(buffer) ? length : sizeof(buffer) - 1);
}

void SerialAccess::processLockedInput() {
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

void SerialAccess::clearInput() {
  input_[0] = '\0';
  inputLength_ = 0;
  inputOverflow_ = false;
}

void SerialAccess::acceptOrRejectPassword() {
  if (!inputOverflow_ && passwordMatches()) {
    unlocked_ = true;
#if defined(ARDUINO_ARCH_ESP32)
    serialAuthUnlockedAfterDeepSleep = true;
#endif
    rawPrint(F("[SERIAL AUTH] unlocked for this boot; debug=simple.\n"));
  } else {
    rawPrint(F("[SERIAL AUTH] wrong password.\n[SERIAL AUTH] Enter password: "));
  }
  clearInput();
}

bool SerialAccess::passwordMatches() const {
  const char* const password = SERIAL_AUTH_PASSWORD;
  const size_t passwordLength = strlen(password);
  if (passwordLength != inputLength_) return false;

  uint8_t difference = 0;
  for (size_t index = 0; index < passwordLength; ++index) {
    difference |= static_cast<uint8_t>(input_[index] ^ password[index]);
  }
  return difference == 0;
}

void SerialAccess::rawPrint(const __FlashStringHelper* text) { port_.print(text); }
void SerialAccess::rawPrint(const char* text) { port_.print(text); }

}  // namespace irrigation
