#include "JXBS_GasSensor.h"
#include <math.h>

JXBS_GasSensor::JXBS_GasSensor(RS485Bus& bus,
                               const char* sensorId,
                               const char* gasName,
                               uint8_t address,
                               double scaleDivisor,
                               double maxGas_ppm,
                               bool debugEnable,
                               uint8_t powerLineIndex,
                               uint8_t interfaceIndex,
                               uint16_t sampleRateMin,
                               uint32_t warmUpTimeMs,
                               uint8_t maxConsecutiveErrors,
                               uint32_t minUsefulPowerOffMs)
    : JXCTModbusSensorBase(bus,
                           sensorId,
                           address,
                           0x0006,
                           debugEnable,
                           powerLineIndex,
                           interfaceIndex,
                           sampleRateMin,
                           warmUpTimeMs,
                           maxConsecutiveErrors,
                           minUsefulPowerOffMs),
      gas_raw(0),
      gas_ppm(0.0),
      _gasName(gasName ? gasName : "gas"),
      _scaleDivisor(scaleDivisor > 0.0 ? scaleDivisor : 1.0),
      _maxGas_ppm(maxGas_ppm > 0.0 ? maxGas_ppm : 2000.0) {}

void JXBS_GasSensor::setFallbackValues() {
  gas_raw = 0;
  gas_ppm = -99.0;
}

bool JXBS_GasSensor::readGas(uint8_t driverRetries,
                             uint16_t readTimeoutMs,
                             uint16_t afterReqDelayMs) {
  uint16_t word = 0;
  if (!readRegisterBlock(0x0006, 1, &word, driverRetries, readTimeoutMs, afterReqDelayMs)) {
    return false;
  }

  gas_raw = word;
  gas_ppm = scaleU16(gas_raw, _scaleDivisor);
  logGasReading();
  return validateGas();
}

bool JXBS_GasSensor::validateGas() const {
  if (isFaultRaw16(gas_raw)) return false;
  if (isnan(gas_ppm) || gas_ppm < 0.0 || gas_ppm > _maxGas_ppm) return false;
  return true;
}

void JXBS_GasSensor::logGasReading() const {
  if (!_bus.getLogger() || !_debugEnable) return;

  _bus.getLogger()->print(F("[DRV][JXBS_GasSensor] "), true);
  _bus.getLogger()->print(_gasName, true);
  _bus.getLogger()->print(F(" raw = "), true);
  _bus.getLogger()->print((unsigned int)gas_raw, true, "", DEC);
  _bus.getLogger()->print(F(" | value = "), true);
  _bus.getLogger()->print(gas_ppm, true, " ppm", 2);
  _bus.getLogger()->println("", true);
}

bool JXBS_GasSensor::readData() {
  markReadTime(millis());
  if (readGas()) {
    markSuccess();
    return true;
  }
  if (!_lastParsedFrame) setFallbackValues();
  markFailure();
  return false;
}
