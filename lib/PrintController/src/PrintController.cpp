#include "PrintController.h"

PrintController::PrintController(Print& out, bool enabled, Verbosity v)
: _out(out), _enabled(enabled), _verbosity(v) {}

void PrintController::setEnabled(bool en) { _enabled = en; }
bool PrintController::isEnabled() const { return _enabled; }

void PrintController::setVerbosity(Verbosity v) { _verbosity = v; }
PrintController::Verbosity PrintController::verbosity() const { return _verbosity; }

// ---- print / println wrappers ----
void PrintController::print(const __FlashStringHelper* s){ if(_enabled) _out.print(s); }
void PrintController::print(const char* s){ if(_enabled) _out.print(s); }
void PrintController::print(char c){ if(_enabled) _out.print(c); }
void PrintController::print(unsigned char v, int base){ if(_enabled) _out.print(v, base); }
void PrintController::print(int v, int base){ if(_enabled) _out.print(v, base); }
void PrintController::print(unsigned int v, int base){ if(_enabled) _out.print(v, base); }
void PrintController::print(long v, int base){ if(_enabled) _out.print(v, base); }
void PrintController::print(unsigned long v, int base){ if(_enabled) _out.print(v, base); }
void PrintController::print(double v, int digits){ if(_enabled) _out.print(v, digits); }
void PrintController::print(const String& s){ if(_enabled) _out.print(s); }

void PrintController::println(){ if(_enabled) _out.println(); }
void PrintController::println(const __FlashStringHelper* s){ if(_enabled) _out.println(s); }
void PrintController::println(const char* s){ if(_enabled) _out.println(s); }
void PrintController::println(char c){ if(_enabled) _out.println(c); }
void PrintController::println(unsigned char v, int base){ if(_enabled) _out.println(v, base); }
void PrintController::println(int v, int base){ if(_enabled) _out.println(v, base); }
void PrintController::println(unsigned int v, int base){ if(_enabled) _out.println(v, base); }
void PrintController::println(long v, int base){ if(_enabled) _out.println(v, base); }
void PrintController::println(unsigned long v, int base){ if(_enabled) _out.println(v, base); }
void PrintController::println(double v, int digits){ if(_enabled) _out.println(v, digits); }
void PrintController::println(const String& s){ if(_enabled) _out.println(s); }

// ---- level-gated logs ----
void PrintController::log(Verbosity lvl, const __FlashStringHelper* s){
  if(_enabled && _verbosity >= lvl) _out.print(s);
}
void PrintController::log(Verbosity lvl, const char* s){
  if(_enabled && _verbosity >= lvl) _out.print(s);
}
void PrintController::logln(Verbosity lvl, const __FlashStringHelper* s){
  if(_enabled && _verbosity >= lvl) _out.println(s);
}
void PrintController::logln(Verbosity lvl, const char* s){
  if(_enabled && _verbosity >= lvl) _out.println(s);
}

// ---- hex helpers ----
void PrintController::printHexByte(uint8_t b){
  if(!_enabled) return;
  if (b < 0x10) _out.print('0');
  _out.print(b, HEX);
}

void PrintController::printHexU16(uint16_t v){
  if(!_enabled) return;
  uint8_t hi = (uint8_t)((v >> 8) & 0xFF);
  uint8_t lo = (uint8_t)(v & 0xFF);
  printHexByte(hi);
  _out.print(' ');
  printHexByte(lo);
}

void PrintController::dumpHexInline(const uint8_t* data, size_t len){
  if(!_enabled) return;
  for(size_t i=0;i<len;i++){
    printHexByte(data[i]);
    if(i + 1 < len) _out.print(' ');
  }
}

void PrintController::dumpHexLines(const uint8_t* data, size_t len, size_t bytesPerLine){
  if(!_enabled) return;
  if(bytesPerLine == 0) bytesPerLine = 16;
  for(size_t i=0;i<len;i++){
    if(i % bytesPerLine == 0){
      _out.println();
    }
    printHexByte(data[i]);
    _out.print(' ');
  }
  _out.println();
}
