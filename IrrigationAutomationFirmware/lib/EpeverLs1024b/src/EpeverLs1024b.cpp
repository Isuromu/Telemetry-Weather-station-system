#include "EpeverLs1024b.h"

namespace irrigation::power {
namespace {

constexpr uint16_t BATTERY_VOLTAGE_REGISTER = 0x3104;
constexpr uint16_t LOAD_CURRENT_REGISTER = 0x310D;
constexpr uint16_t BATTERY_TEMPERATURE_REGISTER = 0x3110;
constexpr uint16_t BATTERY_STATUS_REGISTER = 0x3200;
constexpr uint8_t READ_INPUT_REGISTERS = 0x04;

}  // namespace

EpeverLs1024b::EpeverLs1024b(
    RS485Bus &transport, EpeverLs1024bConfiguration configuration)
    : transport_(transport), configuration_(configuration) {}

bool EpeverLs1024b::begin() {
  initialized_ = transport_.initialized() &&
                 configuration_.slaveAddress >= 1 &&
                 configuration_.slaveAddress <= 247 &&
                 configuration_.responseTimeoutMs > 0;
  return initialized_;
}

bool EpeverLs1024b::readInputRegister(uint16_t address, uint16_t &value) {
  if (!initialized_) return false;
  const uint8_t request[] = {
      configuration_.slaveAddress,
      READ_INPUT_REGISTERS,
      static_cast<uint8_t>(address >> 8U),
      static_cast<uint8_t>(address),
      0x00,
      0x01,
  };
  const Rs485Result result = transport_.transact(
      request, sizeof(request), configuration_.responseTimeoutMs,
      configuration_.debug);
  if (!result.ok() || transport_.rawLength() != 7) return false;

  const uint8_t *response = transport_.rawData();
  if (response[2] != 2) return false;
  value = (static_cast<uint16_t>(response[3]) << 8U) | response[4];
  return true;
}

EpeverLs1024bReading EpeverLs1024b::read() {
  EpeverLs1024bReading reading{};
  uint16_t raw = 0;

  reading.batteryVoltageValid =
      readInputRegister(BATTERY_VOLTAGE_REGISTER, raw);
  if (reading.batteryVoltageValid) reading.batteryVoltage = raw * 0.01F;

  reading.loadCurrentValid = readInputRegister(LOAD_CURRENT_REGISTER, raw);
  if (reading.loadCurrentValid) reading.loadCurrentA = raw * 0.01F;

  reading.batteryStatusValid =
      readInputRegister(BATTERY_STATUS_REGISTER, raw);
  if (reading.batteryStatusValid) reading.batteryStatus = raw;

  reading.batteryTemperatureValid =
      readInputRegister(BATTERY_TEMPERATURE_REGISTER, raw);
  if (reading.batteryTemperatureValid)
    reading.batteryTemperatureC = static_cast<int16_t>(raw) * 0.01F;

  return reading;
}

}  // namespace irrigation::power
