#pragma once

#include <Arduino.h>
#include <SerialAuthConfig.h>

namespace irrigation {

// A Stream adapter that keeps local diagnostics and commands unavailable until
// a password is accepted on the physical Serial connection.
class SerialAccess final : public Stream {
 public:
  explicit SerialAccess(HardwareSerial& port);

  void begin(unsigned long baud);
  void begin(unsigned long baud, uint32_t config);
  void setTimeout(unsigned long timeout);
  void poll();
  bool waitForAuthentication();

  bool unlocked() const;

  int available() override;
  int read() override;
  int peek() override;
  void flush() override;
  size_t write(uint8_t byte) override;
  using Print::write;

  size_t printf(const char* format, ...)
      __attribute__((format(printf, 2, 3)));

 private:
  static constexpr size_t kInputCapacity = SERIAL_AUTH_MAX_INPUT_LENGTH + 1;

  HardwareSerial& port_;
  char input_[kInputCapacity]{};
  size_t inputLength_{0};
  bool inputOverflow_{false};
  bool unlocked_{false};
  uint32_t authStartedMs_{0};
  bool authExpired_{false};

  void beginAuthState();
  void processLockedInput();
  void clearInput();
  void acceptOrRejectPassword();
  bool passwordMatches() const;
  void rawPrint(const __FlashStringHelper* text);
  void rawPrint(const char* text);
};

}  // namespace irrigation
