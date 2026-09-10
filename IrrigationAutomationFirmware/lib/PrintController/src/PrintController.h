#pragma once

#include <Arduino.h>

class PrintController {
 public:
  template <typename T>
  explicit PrintController(T &port, bool enableDefault = false)
      : printPort_(port),
        flushContext_(static_cast<void *>(&port)),
        flushCallback_(&PrintController::flushAdapter<T>),
        enabled_(enableDefault),
        overrideEnabled_(true) {}

  void print(const __FlashStringHelper *data, bool overrideEnable = false,
             const char *end = "");
  void print(const char *data, bool overrideEnable = false,
             const char *end = "");
  void print(char data, bool overrideEnable = false, const char *end = "");
  void print(unsigned char data, bool overrideEnable = false,
             const char *end = "", int base = DEC);
  void print(int data, bool overrideEnable = false, const char *end = "",
             int base = DEC);
  void print(unsigned int data, bool overrideEnable = false,
             const char *end = "", int base = DEC);
  void print(long data, bool overrideEnable = false, const char *end = "",
             int base = DEC);
  void print(unsigned long data, bool overrideEnable = false,
             const char *end = "", int base = DEC);
  void print(double data, bool overrideEnable = false, const char *end = "",
             int decimalPlaces = 2);

  void println(const __FlashStringHelper *data, bool overrideEnable = false,
               const char *end = "");
  void println(const char *data, bool overrideEnable = false,
               const char *end = "");
  void println(char data, bool overrideEnable = false, const char *end = "");
  void println(unsigned char data, bool overrideEnable = false,
               const char *end = "", int base = DEC);
  void println(int data, bool overrideEnable = false, const char *end = "",
               int base = DEC);
  void println(unsigned int data, bool overrideEnable = false,
               const char *end = "", int base = DEC);
  void println(long data, bool overrideEnable = false, const char *end = "",
               int base = DEC);
  void println(unsigned long data, bool overrideEnable = false,
               const char *end = "", int base = DEC);
  void println(double data, bool overrideEnable = false, const char *end = "",
               int decimalPlaces = 2);
  void println(bool overrideEnable = false);

  size_t write(uint8_t byte, bool overrideEnable = false);
  size_t write(const uint8_t *buffer, size_t size,
               bool overrideEnable = false);
  void flush();

  void setEnable(bool enabled);
  bool getEnable() const;
  void setOverrideEnable(bool enabled);
  bool getOverrideEnable() const;
  bool canPrint(bool overrideEnable) const;

 private:
  Print &printPort_;
  void *flushContext_;
  void (*flushCallback_)(void *);
  bool enabled_;
  bool overrideEnabled_;

  template <typename T>
  static void flushAdapter(void *context) {
    static_cast<T *>(context)->flush();
  }

  template <typename T>
  void printNumber(T value, bool overrideEnable, const char *end, int base) {
    if (!canPrint(overrideEnable)) return;
    if (base == HEX && value >= 0 && value <= 0x0F) printPort_.print('0');
    printPort_.print(value, base);
    printPort_.print(end);
  }
};

