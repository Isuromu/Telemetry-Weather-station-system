#pragma once
#include <Arduino.h>

/*
  PrintController
  - Mode 0 (SUMMARY): short, high-level logs
  - Mode 1 (IO): show TX/RX frames (concise hex dumps)
  - Mode 2 (TRACE): full dumps + internal details (raw buffers, indexes, etc.)

  This logger is intentionally simple and dependency-free.
*/

class PrintController {
public:
  enum Verbosity : uint8_t {
    SUMMARY = 0,
    IO      = 1,
    TRACE   = 2
  };

  explicit PrintController(Print& out, bool enabled = true, Verbosity v = SUMMARY);

  void setEnabled(bool en);
  bool isEnabled() const;

  void setVerbosity(Verbosity v);
  Verbosity verbosity() const;

  // Generic print helpers (guarded by enabled flag)
  void print(const __FlashStringHelper* s);
  void print(const char* s);
  void print(char c);
  void print(unsigned char v, int base = DEC);
  void print(int v, int base = DEC);
  void print(unsigned int v, int base = DEC);
  void print(long v, int base = DEC);
  void print(unsigned long v, int base = DEC);
  void print(double v, int digits = 2);
  void print(const String& s);

  void println();
  void println(const __FlashStringHelper* s);
  void println(const char* s);
  void println(char c);
  void println(unsigned char v, int base = DEC);
  void println(int v, int base = DEC);
  void println(unsigned int v, int base = DEC);
  void println(long v, int base = DEC);
  void println(unsigned long v, int base = DEC);
  void println(double v, int digits = 2);
  void println(const String& s);

  // Level-gated logging helpers
  void log(Verbosity lvl, const __FlashStringHelper* s);
  void log(Verbosity lvl, const char* s);
  void logln(Verbosity lvl, const __FlashStringHelper* s);
  void logln(Verbosity lvl, const char* s);

  // Hex helpers
  void printHexByte(uint8_t b);
  void printHexU16(uint16_t v); // prints as 2 bytes
  void dumpHexInline(const uint8_t* data, size_t len);
  void dumpHexLines(const uint8_t* data, size_t len, size_t bytesPerLine = 16);

private:
  Print& _out;
  bool _enabled;
  Verbosity _verbosity;
};
