#pragma once

#include <RadioLib.h>
#include <stdint.h>

namespace irrigation::main_valve::config {

namespace lorawan {
inline constexpr const LoRaWANBand_t &REGION = EU868;
inline constexpr uint8_t SUB_BAND = 0;
}  // namespace lorawan

}  // namespace irrigation::main_valve::config
