#include "PressureSensorXDB401.h"

namespace irrigation::pressure_node {

PressureSensorXDB401::PressureSensorXDB401(
    TwoWire &bus, Xdb401Configuration configuration)
    : bus_(bus), configuration_(configuration) {}

bool PressureSensorXDB401::configurationValid() const {
  return configuration_.primaryAddress != 0 &&
         configuration_.alternateAddress != 0 &&
         configuration_.fullScaleBar > 0.0F &&
         configuration_.readyPollAttempts > 0 &&
         configuration_.readyPollIntervalMs > 0;
}

ReadingStatus PressureSensorXDB401::begin() {
  initialized_ = true;
  address_ = 0;
  if (!configurationValid()) return ReadingStatus::ConfigurationMissing;
  if (devicePresent(configuration_.primaryAddress)) {
    address_ = configuration_.primaryAddress;
  } else if (devicePresent(configuration_.alternateAddress)) {
    address_ = configuration_.alternateAddress;
  }
  return address_ == 0 ? ReadingStatus::NotFound : ReadingStatus::NotSampled;
}

bool PressureSensorXDB401::devicePresent(uint8_t address) {
  bus_.beginTransmission(address);
  return bus_.endTransmission() == 0;
}

bool PressureSensorXDB401::writeRegister(uint8_t reg, uint8_t value) {
  if (address_ == 0) return false;
  bus_.beginTransmission(address_);
  bus_.write(reg);
  bus_.write(value);
  return bus_.endTransmission() == 0;
}

bool PressureSensorXDB401::readRegister(uint8_t reg, uint8_t *data,
                                       size_t length) {
  if (address_ == 0 || data == nullptr || length == 0 || length > 255) {
    return false;
  }

  bus_.beginTransmission(address_);
  bus_.write(reg);
  if (bus_.endTransmission(false) != 0) return false;

  const size_t received =
      bus_.requestFrom(address_, static_cast<uint8_t>(length));
  if (received != length) return false;
  for (size_t index = 0; index < length; ++index) {
    const int value = bus_.read();
    if (value < 0) return false;
    data[index] = static_cast<uint8_t>(value);
  }
  return true;
}

PressureReading PressureSensorXDB401::read() {
  PressureReading reading{};
  reading.address = address_;

  if (!configurationValid()) {
    reading.status = ReadingStatus::ConfigurationMissing;
    return reading;
  }
  if (!initialized_) {
    reading.status = ReadingStatus::NotInitialized;
    return reading;
  }
  if (address_ == 0) {
    reading.status = ReadingStatus::NotFound;
    return reading;
  }
  if (!writeRegister(configuration_.measurementRegister,
                     configuration_.startMeasurementCommand)) {
    reading.status = ReadingStatus::ReadError;
    return reading;
  }

  bool measurementReady = false;
  for (uint8_t attempt = 0; attempt < configuration_.readyPollAttempts;
       ++attempt) {
    delay(configuration_.readyPollIntervalMs);
    uint8_t status = 0;
    if (!readRegister(configuration_.measurementRegister, &status, 1)) {
      reading.status = ReadingStatus::ReadError;
      return reading;
    }
    if ((status & configuration_.busyMask) == 0U) {
      measurementReady = true;
      break;
    }
  }
  if (!measurementReady) {
    reading.status = ReadingStatus::Timeout;
    return reading;
  }

  uint8_t pressureData[3] = {0};
  uint8_t temperatureData[2] = {0};
  if (!readRegister(configuration_.pressureRegister, pressureData,
                    sizeof(pressureData)) ||
      !readRegister(configuration_.temperatureRegister, temperatureData,
                    sizeof(temperatureData))) {
    reading.status = ReadingStatus::ReadError;
    return reading;
  }

  const uint32_t raw24 =
      (static_cast<uint32_t>(pressureData[0]) << 16U) |
      (static_cast<uint32_t>(pressureData[1]) << 8U) |
      static_cast<uint32_t>(pressureData[2]);
  const int32_t rawPressure =
      (raw24 & 0x800000UL) != 0UL
          ? static_cast<int32_t>(raw24 | 0xFF000000UL)
          : static_cast<int32_t>(raw24);
  const int16_t rawTemperature = static_cast<int16_t>(
      (static_cast<uint16_t>(temperatureData[0]) << 8U) |
      static_cast<uint16_t>(temperatureData[1]));

  // These formulas preserve the current prototype interpretation. The status
  // remains VALID_UNCALIBRATED until the exact sensor documentation and a
  // pressure reference validate the engineering scale.
  reading.pressureBar = static_cast<float>(rawPressure) / 8388608.0F *
                        configuration_.fullScaleBar;
  reading.temperatureC = static_cast<float>(rawTemperature) / 256.0F;
  reading.status = configuration_.engineeringScaleValidated
                       ? ReadingStatus::Valid
                       : ReadingStatus::ValidUncalibrated;
  return reading;
}

}  // namespace irrigation::pressure_node
