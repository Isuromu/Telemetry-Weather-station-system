#include "PrintController.h"

void PrintController::print(const __FlashStringHelper *data,
                            bool overrideEnable, const char *end) {
  if (!canPrint(overrideEnable)) return;
  printPort_.print(data);
  printPort_.print(end);
}

void PrintController::print(const char *data, bool overrideEnable,
                            const char *end) {
  if (!canPrint(overrideEnable)) return;
  printPort_.print(data);
  printPort_.print(end);
}

void PrintController::print(char data, bool overrideEnable, const char *end) {
  if (!canPrint(overrideEnable)) return;
  printPort_.print(data);
  printPort_.print(end);
}

void PrintController::print(unsigned char data, bool overrideEnable,
                            const char *end, int base) {
  printNumber(data, overrideEnable, end, base);
}

void PrintController::print(int data, bool overrideEnable, const char *end,
                            int base) {
  printNumber(data, overrideEnable, end, base);
}

void PrintController::print(unsigned int data, bool overrideEnable,
                            const char *end, int base) {
  printNumber(data, overrideEnable, end, base);
}

void PrintController::print(long data, bool overrideEnable, const char *end,
                            int base) {
  printNumber(data, overrideEnable, end, base);
}

void PrintController::print(unsigned long data, bool overrideEnable,
                            const char *end, int base) {
  printNumber(data, overrideEnable, end, base);
}

void PrintController::print(double data, bool overrideEnable, const char *end,
                            int decimalPlaces) {
  if (!canPrint(overrideEnable)) return;
  printPort_.print(data, decimalPlaces);
  printPort_.print(end);
}

#define PRINTCONTROLLER_PRINTLN(TYPE)                                      \
  void PrintController::println(TYPE data, bool overrideEnable,            \
                                const char *end) {                          \
    print(data, overrideEnable, end);                                       \
    if (canPrint(overrideEnable)) printPort_.println();                     \
  }

PRINTCONTROLLER_PRINTLN(const __FlashStringHelper *)
PRINTCONTROLLER_PRINTLN(const char *)
PRINTCONTROLLER_PRINTLN(char)

#undef PRINTCONTROLLER_PRINTLN

void PrintController::println(unsigned char data, bool overrideEnable,
                              const char *end, int base) {
  print(data, overrideEnable, end, base);
  if (canPrint(overrideEnable)) printPort_.println();
}

void PrintController::println(int data, bool overrideEnable, const char *end,
                              int base) {
  print(data, overrideEnable, end, base);
  if (canPrint(overrideEnable)) printPort_.println();
}

void PrintController::println(unsigned int data, bool overrideEnable,
                              const char *end, int base) {
  print(data, overrideEnable, end, base);
  if (canPrint(overrideEnable)) printPort_.println();
}

void PrintController::println(long data, bool overrideEnable, const char *end,
                              int base) {
  print(data, overrideEnable, end, base);
  if (canPrint(overrideEnable)) printPort_.println();
}

void PrintController::println(unsigned long data, bool overrideEnable,
                              const char *end, int base) {
  print(data, overrideEnable, end, base);
  if (canPrint(overrideEnable)) printPort_.println();
}

void PrintController::println(double data, bool overrideEnable,
                              const char *end, int decimalPlaces) {
  print(data, overrideEnable, end, decimalPlaces);
  if (canPrint(overrideEnable)) printPort_.println();
}

void PrintController::println(bool overrideEnable) {
  if (canPrint(overrideEnable)) printPort_.println();
}

size_t PrintController::write(uint8_t byte, bool overrideEnable) {
  return canPrint(overrideEnable) ? printPort_.write(byte) : 0;
}

size_t PrintController::write(const uint8_t *buffer, size_t size,
                              bool overrideEnable) {
  return canPrint(overrideEnable) ? printPort_.write(buffer, size) : 0;
}

void PrintController::flush() {
  if (flushCallback_ != nullptr) flushCallback_(flushContext_);
}

void PrintController::setEnable(bool enabled) { enabled_ = enabled; }
bool PrintController::getEnable() const { return enabled_; }
void PrintController::setOverrideEnable(bool enabled) {
  overrideEnabled_ = enabled;
}
bool PrintController::getOverrideEnable() const { return overrideEnabled_; }
bool PrintController::canPrint(bool overrideEnable) const {
  return enabled_ || (overrideEnable && overrideEnabled_);
}

