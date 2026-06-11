#include "JXBS_GasSO2NO2PressureShield.h"
#include <math.h>

static const uint16_t SHIELD_GAS_START_REGISTER = 0x0006;
static const uint16_t SHIELD_NO2_REGISTER = 0x0006;
static const uint16_t SHIELD_SO2_REGISTER = 0x0007;
static const uint16_t SHIELD_PRESSURE_HIGH_REGISTER = 0x0012;
static const uint16_t SHIELD_PRESSURE_LOW_REGISTER = 0x0013;
static const uint8_t SHIELD_MEASUREMENT_BLOCK_WORDS =
    (uint8_t)(SHIELD_PRESSURE_LOW_REGISTER - SHIELD_GAS_START_REGISTER + 1);
static const uint8_t SHIELD_NO2_OFFSET =
    (uint8_t)(SHIELD_NO2_REGISTER - SHIELD_GAS_START_REGISTER);
static const uint8_t SHIELD_SO2_OFFSET =
    (uint8_t)(SHIELD_SO2_REGISTER - SHIELD_GAS_START_REGISTER);
static const uint8_t SHIELD_PRESSURE_HIGH_OFFSET =
    (uint8_t)(SHIELD_PRESSURE_HIGH_REGISTER - SHIELD_GAS_START_REGISTER);
static const uint8_t SHIELD_PRESSURE_LOW_OFFSET =
    (uint8_t)(SHIELD_PRESSURE_LOW_REGISTER - SHIELD_GAS_START_REGISTER);

JXBS_GasSO2NO2PressureShield::JXBS_GasSO2NO2PressureShield(RS485Bus& bus,
                                                           const char* sensorId,
                                                           uint8_t address,
                                                           bool debugEnable,
                                                           double no2ScaleDivisor,
                                                           double so2ScaleDivisor,
                                                           double maxNO2_ppm,
                                                           double maxSO2_ppm,
                                                           uint8_t powerLineIndex,
                                                           uint8_t interfaceIndex,
                                                           uint16_t sampleRateMin,
                                                           uint32_t warmUpTimeMs,
                                                           uint8_t maxConsecutiveErrors,
                                                           uint32_t minUsefulPowerOffMs)
    : JXCTModbusSensorBase(bus, sensorId, address, SHIELD_GAS_START_REGISTER, debugEnable, powerLineIndex, interfaceIndex,
                            sampleRateMin, warmUpTimeMs, maxConsecutiveErrors, minUsefulPowerOffMs),
      no2_raw(0),
      so2_raw(0),
      pressure_raw(0),
      no2_ppm(0.0),
      so2_ppm(0.0),
      pressure_mbar(0.0),
      _no2ScaleDivisor(no2ScaleDivisor > 0.0 ? no2ScaleDivisor : 10.0),
      _so2ScaleDivisor(so2ScaleDivisor > 0.0 ? so2ScaleDivisor : 10.0),
      _maxNO2_ppm(maxNO2_ppm > 0.0 ? maxNO2_ppm : 2000.0),
      _maxSO2_ppm(maxSO2_ppm > 0.0 ? maxSO2_ppm : 2000.0) {}

void JXBS_GasSO2NO2PressureShield::setFallbackValues() {
  no2_raw = 0;
  so2_raw = 0;
  pressure_raw = 0;
  no2_ppm = -99.0;
  so2_ppm = -99.0;
  pressure_mbar = -99.0;
}

bool JXBS_GasSO2NO2PressureShield::readGasBlock(uint8_t driverRetries,
                                                uint16_t readTimeoutMs,
                                                uint16_t afterReqDelayMs) {
  uint16_t words[2] = {0};
  if (!readRegisterBlock(SHIELD_GAS_START_REGISTER, 2, words, driverRetries, readTimeoutMs, afterReqDelayMs)) {
    return false;
  }

  no2_raw = words[0];
  so2_raw = words[1];
  no2_ppm = scaleU16(no2_raw, _no2ScaleDivisor);
  so2_ppm = scaleU16(so2_raw, _so2ScaleDivisor);

  logParsedU16(F("JXBS_GasSO2NO2PressureShield"), F("NO2"), no2_raw, no2_ppm, F("ppm"), 2);
  logParsedU16(F("JXBS_GasSO2NO2PressureShield"), F("SO2"), so2_raw, so2_ppm, F("ppm"), 2);

  return validateGases();
}

bool JXBS_GasSO2NO2PressureShield::readPressure(uint8_t driverRetries,
                                                uint16_t readTimeoutMs,
                                                uint16_t afterReqDelayMs) {
  uint16_t words[2] = {0};
  if (!readRegisterBlock(SHIELD_PRESSURE_HIGH_REGISTER, 2, words, driverRetries, readTimeoutMs, afterReqDelayMs)) {
    return false;
  }

  pressure_raw = joinU32(words[0], words[1]);
  pressure_mbar = (double)pressure_raw / 100.0;

  if (_bus.getLogger() && _debugEnable) {
    _bus.getLogger()->print(F("[DRV][JXBS_GasSO2NO2PressureShield] Pressure raw = "), true);
    _bus.getLogger()->print((unsigned long)pressure_raw, true, " | value = ", DEC);
    _bus.getLogger()->print(pressure_mbar, true, " mbar", 2);
    _bus.getLogger()->println("", true);
  }

  return validatePressure();
}

bool JXBS_GasSO2NO2PressureShield::readMeasurementBlock(uint8_t driverRetries,
                                                        uint16_t readTimeoutMs,
                                                        uint16_t afterReqDelayMs) {
  uint16_t words[SHIELD_MEASUREMENT_BLOCK_WORDS] = {0};
  if (!readRegisterBlock(SHIELD_GAS_START_REGISTER,
                         SHIELD_MEASUREMENT_BLOCK_WORDS,
                         words,
                         driverRetries,
                         readTimeoutMs,
                         afterReqDelayMs)) {
    return false;
  }

  no2_raw = words[SHIELD_NO2_OFFSET];
  so2_raw = words[SHIELD_SO2_OFFSET];
  pressure_raw = joinU32(words[SHIELD_PRESSURE_HIGH_OFFSET],
                         words[SHIELD_PRESSURE_LOW_OFFSET]);

  no2_ppm = scaleU16(no2_raw, _no2ScaleDivisor);
  so2_ppm = scaleU16(so2_raw, _so2ScaleDivisor);
  pressure_mbar = (double)pressure_raw / 100.0;

  logParsedU16(F("JXBS_GasSO2NO2PressureShield"), F("NO2"), no2_raw, no2_ppm, F("ppm"), 2);
  logParsedU16(F("JXBS_GasSO2NO2PressureShield"), F("SO2"), so2_raw, so2_ppm, F("ppm"), 2);

  if (_bus.getLogger() && _debugEnable) {
    _bus.getLogger()->print(F("[DRV][JXBS_GasSO2NO2PressureShield] Pressure raw = "), true);
    _bus.getLogger()->print((unsigned long)pressure_raw, true, " | value = ", DEC);
    _bus.getLogger()->print(pressure_mbar, true, " mbar", 2);
    _bus.getLogger()->println("", true);
  }

  return validateGases() && validatePressure();
}

bool JXBS_GasSO2NO2PressureShield::validateGases() const {
  if (isFaultRaw16(no2_raw) || isnan(no2_ppm) || no2_ppm < 0.0 || no2_ppm > _maxNO2_ppm) return false;
  if (isFaultRaw16(so2_raw) || isnan(so2_ppm) || so2_ppm < 0.0 || so2_ppm > _maxSO2_ppm) return false;
  return true;
}

bool JXBS_GasSO2NO2PressureShield::validatePressure() const {
  if (pressure_raw == 0xFFFFFFFFUL || pressure_raw == 0x7FFFFFFFUL || pressure_raw == 0x80000000UL) return false;
  if (isnan(pressure_mbar) || pressure_mbar < 10.0 || pressure_mbar > 1200.0) return false;
  return true;
}

bool JXBS_GasSO2NO2PressureShield::readData() {
  markReadTime(millis());
  if (readMeasurementBlock(1, SENSOR_DEFAULT_READ_TIMEOUT_MS, SENSOR_DEFAULT_AFTER_REQ_MS)) {
    markSuccess();
    return true;
  }
  if (!_lastParsedFrame) setFallbackValues();
  markFailure();
  return false;
}
