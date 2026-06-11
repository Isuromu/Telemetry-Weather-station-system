#include "JXBS_GasO3CONH3Shield.h"
#include <math.h>

// Shield label map. CO at 0x0006 was field-confirmed with smoke testing;
// 0x000E can move with smoke but is not the direct ppm gas register.
static const uint16_t GAS_O3_CO_NH3_BLOCK_START_REGISTER = 0x0006;
static const uint8_t GAS_O3_CO_NH3_BLOCK_WORDS = 3;

JXBS_GasO3CONH3Shield::JXBS_GasO3CONH3Shield(RS485Bus& bus,
                                             const char* sensorId,
                                             uint8_t address,
                                             bool debugEnable,
                                             double maxCO_ppm,
                                             double maxO3_ppm,
                                             double maxNH3_ppm,
                                             uint8_t powerLineIndex,
                                             uint8_t interfaceIndex,
                                             uint16_t sampleRateMin,
                                             uint32_t warmUpTimeMs,
                                             uint8_t maxConsecutiveErrors,
                                             uint32_t minUsefulPowerOffMs)
    : JXCTModbusSensorBase(bus, sensorId, address, GAS_O3_CO_NH3_BLOCK_START_REGISTER, debugEnable, powerLineIndex, interfaceIndex,
                            sampleRateMin, warmUpTimeMs, maxConsecutiveErrors, minUsefulPowerOffMs),
      co_raw(0),
      o3_raw(0),
      nh3_raw(0),
      co_ppm(0.0),
      o3_ppm(0.0),
      nh3_ppm(0.0),
      _maxCO_ppm(maxCO_ppm > 0.0 ? maxCO_ppm : 2000.0),
      _maxO3_ppm(maxO3_ppm > 0.0 ? maxO3_ppm : 100.0),
      _maxNH3_ppm(maxNH3_ppm > 0.0 ? maxNH3_ppm : 5000.0) {}

void JXBS_GasO3CONH3Shield::setFallbackValues() {
  co_raw = 0;
  o3_raw = 0;
  nh3_raw = 0;
  co_ppm = -99.0;
  o3_ppm = -99.0;
  nh3_ppm = -99.0;
}

bool JXBS_GasO3CONH3Shield::readGasBlock(uint8_t driverRetries,
                                         uint16_t readTimeoutMs,
                                         uint16_t afterReqDelayMs) {
  uint16_t words[3] = {0};
  if (!readRegisterBlock(GAS_O3_CO_NH3_BLOCK_START_REGISTER,
                         GAS_O3_CO_NH3_BLOCK_WORDS,
                         words,
                         driverRetries,
                         readTimeoutMs,
                         afterReqDelayMs)) {
    return false;
  }

  co_raw = words[0];
  o3_raw = words[1];
  nh3_raw = words[2];
  co_ppm = scaleU16(co_raw, 10.0);
  o3_ppm = scaleU16(o3_raw, 100.0);
  nh3_ppm = scaleU16(nh3_raw, 10.0);

  logParsedU16(F("JXBS_GasO3CONH3Shield"), F("CO"), co_raw, co_ppm, F("ppm"), 1);
  logParsedU16(F("JXBS_GasO3CONH3Shield"), F("O3"), o3_raw, o3_ppm, F("ppm"), 2);
  logParsedU16(F("JXBS_GasO3CONH3Shield"), F("NH3"), nh3_raw, nh3_ppm, F("ppm"), 1);

  return validateGases();
}

bool JXBS_GasO3CONH3Shield::validateGases() const {
  if (isFaultRaw16(co_raw) || isnan(co_ppm) || co_ppm < 0.0 || co_ppm > _maxCO_ppm) return false;
  if (isFaultRaw16(o3_raw) || isnan(o3_ppm) || o3_ppm < 0.0 || o3_ppm > _maxO3_ppm) return false;
  if (isFaultRaw16(nh3_raw) || isnan(nh3_ppm) || nh3_ppm < 0.0 || nh3_ppm > _maxNH3_ppm) return false;
  return true;
}

bool JXBS_GasO3CONH3Shield::readData() {
  markReadTime(millis());
  if (readGasBlock()) {
    markSuccess();
    return true;
  }
  if (!_lastParsedFrame) setFallbackValues();
  markFailure();
  return false;
}
