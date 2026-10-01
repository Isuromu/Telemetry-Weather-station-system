#include "SerialDebugMode.h"

namespace irrigation::serial_debug {
namespace {

Mode currentMode = Mode::Simple;

}  // namespace

void setMode(Mode mode) { currentMode = mode; }

Mode mode() { return currentMode; }

bool full() { return currentMode == Mode::Full; }

const __FlashStringHelper* modeName() {
  return full() ? F("full") : F("simple");
}

}  // namespace irrigation::serial_debug
