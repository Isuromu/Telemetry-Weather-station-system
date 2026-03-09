#include "PrintController.h"

PrintController::PrintController(HardwareSerial &port, bool enableDefault)
    : serialPort(port), enable(enableDefault) {}

void PrintController::print(const __FlashStringHelper *data,
                            bool enableOverride, const char *endChar) {
  if (enable || enableOverride) {
    serialPort.print(data);
    serialPort.print(endChar);
  }
}

void PrintController::print(const char *data, bool enableOverride,
                            const char *endChar) {
  if (enable || enableOverride) {
    serialPort.print(data);
    serialPort.print(endChar);
  }
}

void PrintController::print(char data, bool enableOverride,
                            const char *endChar) {
  if (enable || enableOverride) {
    serialPort.print(data);
    serialPort.print(endChar);
  }
}

void PrintController::print(unsigned char data, bool enableOverride,
                            const char *endChar, int base) {
  if (enable || enableOverride) {
    if (base == HEX && data <= 0xF)
      serialPort.print('0'); // Add leading zero
    serialPort.print(data, base);
    serialPort.print(endChar);
  }
}

void PrintController::print(int data, bool enableOverride, const char *endChar,
                            int base) {
  if (enable || enableOverride) {
    if (base == HEX && data >= 0 && data <= 0xF)
      serialPort.print('0'); // Add leading zero
    serialPort.print(data, base);
    serialPort.print(endChar);
  }
}

void PrintController::print(unsigned int data, bool enableOverride,
                            const char *endChar, int base) {
  if (enable || enableOverride) {
    if (base == HEX && data <= 0xF)
      serialPort.print('0'); // Add leading zero
    serialPort.print(data, base);
    serialPort.print(endChar);
  }
}

void PrintController::print(long data, bool enableOverride, const char *endChar,
                            int base) {
  if (enable || enableOverride) {
    if (base == HEX && data >= 0 && data <= 0xF)
      serialPort.print('0'); // Add leading zero
    serialPort.print(data, base);
    serialPort.print(endChar);
  }
}

void PrintController::print(unsigned long data, bool enableOverride,
                            const char *endChar, int base) {
  if (enable || enableOverride) {
    if (base == HEX && data <= 0xF)
      serialPort.print('0'); // Add leading zero
    serialPort.print(data, base);
    serialPort.print(endChar);
  }
}

void PrintController::print(double data, bool enableOverride,
                            const char *endChar, int decimalPlaces) {
  if (enable || enableOverride) {
    serialPort.print(data, decimalPlaces);
    serialPort.print(endChar);
  }
}

void PrintController::println(const __FlashStringHelper *data,
                              bool enableOverride, const char *endChar) {
  print(data, enableOverride, endChar);
  if (enable || enableOverride)
    serialPort.println();
}

void PrintController::println(const char *data, bool enableOverride,
                              const char *endChar) {
  print(data, enableOverride, endChar);
  if (enable || enableOverride)
    serialPort.println();
}

void PrintController::println(char data, bool enableOverride,
                              const char *endChar) {
  print(data, enableOverride, endChar);
  if (enable || enableOverride)
    serialPort.println();
}

void PrintController::println(unsigned char data, bool enableOverride,
                              const char *endChar, int base) {
  print(data, enableOverride, endChar, base);
  if (enable || enableOverride)
    serialPort.println();
}

void PrintController::println(int data, bool enableOverride,
                              const char *endChar, int base) {
  print(data, enableOverride, endChar, base);
  if (enable || enableOverride)
    serialPort.println();
}

void PrintController::println(unsigned int data, bool enableOverride,
                              const char *endChar, int base) {
  print(data, enableOverride, endChar, base);
  if (enable || enableOverride)
    serialPort.println();
}

void PrintController::println(long data, bool enableOverride,
                              const char *endChar, int base) {
  print(data, enableOverride, endChar, base);
  if (enable || enableOverride)
    serialPort.println();
}

void PrintController::println(unsigned long data, bool enableOverride,
                              const char *endChar, int base) {
  print(data, enableOverride, endChar, base);
  if (enable || enableOverride)
    serialPort.println();
}

void PrintController::println(double data, bool enableOverride,
                              const char *endChar, int decimalPlaces) {
  print(data, enableOverride, endChar, decimalPlaces);
  if (enable || enableOverride)
    serialPort.println();
}

void PrintController::println() { serialPort.println(); }
void PrintController::flush() { serialPort.flush(); }

size_t PrintController::write(uint8_t byte, bool enableOverride) {
  if (enableOverride) {
    return serialPort.write(byte);
  }
  return 0;
}

size_t PrintController::write(const uint8_t *buffer, size_t size,
                              bool enableOverride) {
  if (enableOverride) {
    return serialPort.write(buffer, size);
  }
  return 0;
}

void PrintController::setEnable(bool enableStatus) { enable = enableStatus; }

bool PrintController::getEnable() const { return enable; }
