#ifndef PRINTCONTROLLER_H
#define PRINTCONTROLLER_H

#include <Arduino.h>

class PrintController {
private:
  HardwareSerial &serialPort;
  bool enable;

public:
  // Constructor
  PrintController(HardwareSerial &port, bool enableDefault = false);

  // Methods to print
  void print(const __FlashStringHelper *data, bool enable = false,
             const char *endChar = "");
  void print(const char *data, bool enable = false, const char *endChar = "");
  void print(char data, bool enable = false, const char *endChar = "");
  void print(unsigned char data, bool enable = false, const char *endChar = "",
             int base = DEC);
  void print(int data, bool enable = false, const char *endChar = "",
             int base = DEC);
  void print(unsigned int data, bool enable = false, const char *endChar = "",
             int base = DEC);
  void print(long data, bool enable = false, const char *endChar = "",
             int base = DEC);
  void print(unsigned long data, bool enable = false, const char *endChar = "",
             int base = DEC);
  void print(double data, bool enable = false, const char *endChar = "",
             int decimalPlaces = 2);

  void println(const __FlashStringHelper *data, bool enable = false,
               const char *endChar = "");
  void println(const char *data, bool enable = false, const char *endChar = "");
  void println(char data, bool enable = false, const char *endChar = "");
  void println(unsigned char data, bool enable = false,
               const char *endChar = "", int base = DEC);
  void println(int data, bool enable = false, const char *endChar = "",
               int base = DEC);
  void println(unsigned int data, bool enable = false, const char *endChar = "",
               int base = DEC);
  void println(long data, bool enable = false, const char *endChar = "",
               int base = DEC);
  void println(unsigned long data, bool enable = false,
               const char *endChar = "", int base = DEC);
  void println(double data, bool enable = false, const char *endChar = "",
               int decimalPlaces = 2);
  size_t write(uint8_t byte, bool enableOverride);
  size_t write(const uint8_t *buffer, size_t size, bool enableOverride);
  void println();
  void flush();

  // Methods to control and retrieve enable status
  void setEnable(bool enableStatus);
  bool getEnable() const;
};

#endif // PRINTCONTROLLER_H
