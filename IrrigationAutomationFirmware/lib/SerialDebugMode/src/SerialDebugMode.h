#pragma once

#include <Arduino.h>

namespace irrigation::serial_debug {

enum class Mode : uint8_t {
  Simple,
  Full,
};

void setMode(Mode mode);
Mode mode();
bool full();
const __FlashStringHelper* modeName();

}  // namespace irrigation::serial_debug
