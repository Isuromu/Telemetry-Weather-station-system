#pragma once

#include <stdint.h>

namespace irrigation::diagnostics {

inline const char *modbusExceptionMeaning(uint8_t code) {
  switch (code) {
    case 0x01: return "illegal function";
    case 0x02: return "illegal data address";
    case 0x03: return "illegal data value";
    case 0x04: return "server device failure";
    case 0x05: return "acknowledgement";
    case 0x06: return "server device busy";
    case 0x08: return "memory parity error";
    case 0x0A: return "gateway path unavailable";
    case 0x0B: return "gateway target device failed to respond";
    default: return "unclassified Modbus exception";
  }
}

}  // namespace irrigation::diagnostics
