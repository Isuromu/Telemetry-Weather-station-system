#ifndef PRINTCONTROLLER_H
#define PRINTCONTROLLER_H

#include <Arduino.h>

/*
  PrintController

  Purpose:
    Small wrapper around Arduino Print-compatible outputs
    with a global enable flag and per-call override flag.

  Important:
    - Works with HardwareSerial
    - Works with ESP32-S3 USB CDC Serial (HWCDC)
    - Works with any compatible Print-based output that also has flush()

  Behavior:
    - If global enable == true, all print/println calls work normally
    - If global enable == false, a call prints only when enableOverride == true
    - println() without arguments now respects global enable
*/

class PrintController {
private:
  Print &printPort;
  void *flushContext;
  void (*flushCallback)(void *);
  bool enable;

  template <typename T>
  static void flushAdapter(void *ctx) {
    static_cast<T *>(ctx)->flush();
  }

public:
  template <typename T>
  PrintController(T &port, bool enableDefault = false)
      : printPort(port),
        flushContext(static_cast<void *>(&port)),
        flushCallback(&PrintController::flushAdapter<T>),
        enable(enableDefault) {}

  void print(const __FlashStringHelper *data, bool enableOverride = false,
             const char *endChar = "");
  void print(const char *data, bool enableOverride = false,
             const char *endChar = "");
  void print(char data, bool enableOverride = false,
             const char *endChar = "");
  void print(unsigned char data, bool enableOverride = false,
             const char *endChar = "", int base = DEC);
  void print(int data, bool enableOverride = false,
             const char *endChar = "", int base = DEC);
  void print(unsigned int data, bool enableOverride = false,
             const char *endChar = "", int base = DEC);
  void print(long data, bool enableOverride = false,
             const char *endChar = "", int base = DEC);
  void print(unsigned long data, bool enableOverride = false,
             const char *endChar = "", int base = DEC);
  void print(double data, bool enableOverride = false,
             const char *endChar = "", int decimalPlaces = 2);

  void println(const __FlashStringHelper *data, bool enableOverride = false,
               const char *endChar = "");
  void println(const char *data, bool enableOverride = false,
               const char *endChar = "");
  void println(char data, bool enableOverride = false,
               const char *endChar = "");
  void println(unsigned char data, bool enableOverride = false,
               const char *endChar = "", int base = DEC);
  void println(int data, bool enableOverride = false,
               const char *endChar = "", int base = DEC);
  void println(unsigned int data, bool enableOverride = false,
               const char *endChar = "", int base = DEC);
  void println(long data, bool enableOverride = false,
               const char *endChar = "", int base = DEC);
  void println(unsigned long data, bool enableOverride = false,
               const char *endChar = "", int base = DEC);
  void println(double data, bool enableOverride = false,
               const char *endChar = "", int decimalPlaces = 2);

  size_t write(uint8_t byte, bool enableOverride);
  size_t write(const uint8_t *buffer, size_t size, bool enableOverride);

  void println();
  void flush();

  void setEnable(bool enableStatus);
  bool getEnable() const;
};

#endif