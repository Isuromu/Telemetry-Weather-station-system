#include "SolarControllerEpLs1024B.h"

#include <Arduino.h>

#include <string.h>

namespace irrigation::pressure_node {

namespace protocol = epever_ls1024b::protocol;

namespace {

// The controller needs a moment to store the block before it reports it back.
inline constexpr uint16_t VOLTAGE_BLOCK_SETTLE_MS = 250;

}  // namespace

SolarControllerEpLs1024B::SolarControllerEpLs1024B(
    RS485Bus &transport, SolarControllerConfiguration configuration)
    : transport_(transport), configuration_(configuration) {}

bool SolarControllerEpLs1024B::communicationConfigurationValid() const {
  return configuration_.slaveAddress >= 1 &&
         configuration_.slaveAddress <= 247 &&
         configuration_.responseTimeoutMs > 0;
}

bool SolarControllerEpLs1024B::begin() {
  transportReady_ = communicationConfigurationValid() && transport_.initialized();
  // The driver is usable as soon as the transport and address are valid; the
  // 0x9000 settings map being unconfirmed does not block measurement reads.
  initialized_ = transportReady_;
  return initialized_;
}

ReadingStatus SolarControllerEpLs1024B::mapTransportStatus(
    Rs485Status status) const {
  if (status == Rs485Status::NotInitialized)
    return ReadingStatus::NotInitialized;
  if (status == Rs485Status::Timeout) return ReadingStatus::Timeout;
  return ReadingStatus::ReadError;
}

bool SolarControllerEpLs1024B::readRegisters(uint8_t function,
                                             uint16_t startAddress,
                                             uint16_t registerCount,
                                             uint8_t expectedByteCount,
                                             const uint8_t *&data,
                                             ReadingStatus &failureStatus) {
  return readRegistersAt(configuration_.slaveAddress, function, startAddress,
                         registerCount, expectedByteCount, data, failureStatus);
}

bool SolarControllerEpLs1024B::readRegistersAt(
    uint8_t address, uint8_t function, uint16_t startAddress,
    uint16_t registerCount, uint8_t expectedByteCount, const uint8_t *&data,
    ReadingStatus &failureStatus) {
  const auto request = protocol::makeReadRegistersRequest(
      address, function, startAddress, registerCount);
  const Rs485Result result = transport_.transact(
      request.bytes, sizeof(request.bytes), configuration_.responseTimeoutMs,
      configuration_.debug);
  lastExceptionCode_ = result.exceptionCode;
  if (!result.ok()) {
    failureStatus = mapTransportStatus(result.status);
    return false;
  }

  const uint8_t *frame = transport_.rawData();
  const size_t expectedFrameLength =
      static_cast<size_t>(expectedByteCount) + 5U;
  if (frame == nullptr || transport_.rawLength() != expectedFrameLength ||
      frame[0] != address || frame[1] != function ||
      frame[2] != expectedByteCount) {
    failureStatus = ReadingStatus::ReadError;
    return false;
  }
  data = &frame[3];
  return true;
}

bool SolarControllerEpLs1024B::readRegister(uint8_t function,
                                            uint16_t registerAddress,
                                            uint16_t &rawValue) {
  if (!communicationConfigurationValid()) return false;
  if (!transport_.initialized()) return false;

  ReadingStatus failureStatus = ReadingStatus::ReadError;
  const uint8_t *data = nullptr;
  if (!readRegisters(function, registerAddress, 1, 2, data, failureStatus))
    return false;
  rawValue = protocol::decodeUint16(data);
  return true;
}

SolarControllerMeasurements SolarControllerEpLs1024B::readMeasurements() {
  SolarControllerMeasurements reading{};
  if (!communicationConfigurationValid()) {
    reading.status = ReadingStatus::ConfigurationMissing;
    return reading;
  }
  if (!initialized_ || !transport_.initialized()) {
    reading.status = ReadingStatus::NotInitialized;
    return reading;
  }

  // Battery voltage sits in this block, so a failure here is a failure to
  // measure at all.
  ReadingStatus failureStatus = ReadingStatus::ReadError;
  const uint8_t *block = nullptr;
  if (!readRegisters(protocol::READ_INPUT_REGISTERS,
                     protocol::PV_BATTERY_BLOCK_START,
                     protocol::PV_BATTERY_BLOCK_COUNT,
                     protocol::PV_BATTERY_BLOCK_BYTE_COUNT, block,
                     failureStatus)) {
    reading.status = failureStatus;
    return reading;
  }
  const auto at = [&block](uint16_t reg, uint16_t blockStart) -> const uint8_t * {
    return &block[reg - blockStart];
  };
  reading.pvAndBatteryBlockAvailable = true;
  reading.pvVoltageV = protocol::decodeScaledUint16(
      at(protocol::REG_PV_VOLTAGE, protocol::PV_BATTERY_BLOCK_START),
      protocol::VOLTAGE_SCALE);
  reading.pvCurrentA = protocol::decodeScaledUint16(
      at(protocol::REG_PV_CURRENT, protocol::PV_BATTERY_BLOCK_START),
      protocol::CURRENT_SCALE);
  reading.batteryVoltageV = protocol::decodeScaledUint16(
      at(protocol::REG_BATTERY_VOLTAGE, protocol::PV_BATTERY_BLOCK_START),
      protocol::VOLTAGE_SCALE);
  reading.chargingCurrentA = protocol::decodeScaledUint16(
      at(protocol::REG_CHARGING_CURRENT, protocol::PV_BATTERY_BLOCK_START),
      protocol::CURRENT_SCALE);

  // Load and temperatures are a second transfer and stay optional, so a partial
  // map is reported as unavailable rather than as measured zeroes.
  const uint8_t *loadBlock = nullptr;
  if (readRegisters(protocol::READ_INPUT_REGISTERS,
                    protocol::LOAD_TEMPERATURE_BLOCK_START,
                    protocol::LOAD_TEMPERATURE_BLOCK_COUNT,
                    protocol::LOAD_TEMPERATURE_BLOCK_BYTE_COUNT, loadBlock,
                    failureStatus)) {
    reading.loadAndTemperatureBlockAvailable = true;
    reading.loadVoltageV = protocol::decodeScaledUint16(
        at(protocol::REG_LOAD_VOLTAGE, protocol::LOAD_TEMPERATURE_BLOCK_START),
        protocol::VOLTAGE_SCALE);
    reading.loadCurrentA = protocol::decodeScaledUint16(
        at(protocol::REG_LOAD_CURRENT, protocol::LOAD_TEMPERATURE_BLOCK_START),
        protocol::CURRENT_SCALE);
    reading.batteryTemperatureC = protocol::decodeScaledInt16(
        at(protocol::REG_BATTERY_TEMPERATURE,
           protocol::LOAD_TEMPERATURE_BLOCK_START),
        protocol::TEMPERATURE_SCALE);
    reading.deviceTemperatureC = protocol::decodeScaledInt16(
        at(protocol::REG_DEVICE_TEMPERATURE,
           protocol::LOAD_TEMPERATURE_BLOCK_START),
        protocol::TEMPERATURE_SCALE);
  }

  const uint8_t *soc = nullptr;
  if (readRegisters(protocol::READ_INPUT_REGISTERS, protocol::REG_BATTERY_SOC,
                    1, 2, soc, failureStatus)) {
    reading.batterySocAvailable = true;
    reading.batterySocPercent = static_cast<uint16_t>(
        protocol::decodeScaledUint16(soc, protocol::SOC_SCALE));
  }

  const uint8_t *statusBlock = nullptr;
  if (readRegisters(protocol::READ_INPUT_REGISTERS,
                    protocol::STATUS_BLOCK_START,
                    protocol::STATUS_BLOCK_COUNT,
                    protocol::STATUS_BLOCK_BYTE_COUNT, statusBlock,
                    failureStatus)) {
    reading.statusAvailable = true;
    reading.batteryStatusRaw = protocol::decodeUint16(statusBlock);
    reading.chargingStatusRaw = protocol::decodeUint16(&statusBlock[2]);
  }

  reading.status = ReadingStatus::ValidUncalibrated;
  return reading;
}

SolarControllerSettings SolarControllerEpLs1024B::readSettings() {
  SolarControllerSettings settings{};
  if (!communicationConfigurationValid()) {
    settings.status = ReadingStatus::ConfigurationMissing;
    return settings;
  }
  if (!initialized_ || !transport_.initialized()) {
    settings.status = ReadingStatus::NotInitialized;
    return settings;
  }

  ReadingStatus failureStatus = ReadingStatus::ReadError;
  const uint8_t *block = nullptr;
  if (!readRegisters(protocol::READ_HOLDING_REGISTERS,
                     protocol::SETTINGS_BLOCK_START,
                     protocol::SETTINGS_BLOCK_COUNT,
                     protocol::SETTINGS_BLOCK_BYTE_COUNT, block,
                     failureStatus)) {
    settings.status = failureStatus;
    return settings;
  }
  const auto at = [&block](uint16_t reg) -> const uint8_t * {
    return &block[reg - protocol::SETTINGS_BLOCK_START];
  };

  settings.batteryType = protocol::decodeUint16(at(protocol::REG_BATTERY_TYPE));
  settings.batteryCapacityAh =
      protocol::decodeUint16(at(protocol::REG_BATTERY_CAPACITY));
  settings.temperatureCompensationRaw =
      protocol::decodeUint16(at(protocol::REG_TEMPERATURE_COMPENSATION));
  settings.overVoltageDisconnectV = protocol::decodeScaledUint16(
      at(protocol::REG_OVER_VOLTAGE_DISCONNECT), protocol::VOLTAGE_SCALE);
  settings.chargingLimitV = protocol::decodeScaledUint16(
      at(protocol::REG_CHARGING_LIMIT_VOLTAGE), protocol::VOLTAGE_SCALE);
  settings.overVoltageReconnectV = protocol::decodeScaledUint16(
      at(protocol::REG_OVER_VOLTAGE_RECONNECT), protocol::VOLTAGE_SCALE);
  settings.equalizeChargingV = protocol::decodeScaledUint16(
      at(protocol::REG_EQUALIZE_CHARGING_VOLTAGE), protocol::VOLTAGE_SCALE);
  settings.boostChargingV = protocol::decodeScaledUint16(
      at(protocol::REG_BOOST_CHARGING_VOLTAGE), protocol::VOLTAGE_SCALE);
  settings.floatChargingV = protocol::decodeScaledUint16(
      at(protocol::REG_FLOAT_CHARGING_VOLTAGE), protocol::VOLTAGE_SCALE);
  settings.boostReconnectV = protocol::decodeScaledUint16(
      at(protocol::REG_BOOST_RECONNECT_VOLTAGE), protocol::VOLTAGE_SCALE);
  settings.lowVoltageReconnectV = protocol::decodeScaledUint16(
      at(protocol::REG_LOW_VOLTAGE_RECONNECT), protocol::VOLTAGE_SCALE);
  settings.underVoltageRecoverV = protocol::decodeScaledUint16(
      at(protocol::REG_UNDER_VOLTAGE_RECOVER), protocol::VOLTAGE_SCALE);

  // The last three setpoints need their own transfer.
  const uint8_t *tail = nullptr;
  if (readRegisters(protocol::READ_HOLDING_REGISTERS,
                    protocol::SETTINGS_TAIL_START,
                    protocol::SETTINGS_TAIL_COUNT,
                    protocol::SETTINGS_TAIL_BYTE_COUNT, tail,
                    failureStatus)) {
    const auto tailAt = [&tail](uint16_t reg) -> const uint8_t * {
      return &tail[reg - protocol::SETTINGS_TAIL_START];
    };
    settings.tailAvailable = true;
    settings.underVoltageWarningV = protocol::decodeScaledUint16(
        tailAt(protocol::REG_UNDER_VOLTAGE_WARNING), protocol::VOLTAGE_SCALE);
    settings.lowVoltageDisconnectV = protocol::decodeScaledUint16(
        tailAt(protocol::REG_LOW_VOLTAGE_DISCONNECT), protocol::VOLTAGE_SCALE);
    settings.dischargingLimitV = protocol::decodeScaledUint16(
        tailAt(protocol::REG_DISCHARGING_LIMIT), protocol::VOLTAGE_SCALE);
  }

  // Load control mode, rated voltage level, and maximum charging current sit
  // outside the setpoint block. The installed controller may not answer all
  // three, so a failure here is reported per field instead of invalidating the
  // block.
  uint16_t raw = 0;
  if (readRegister(protocol::READ_HOLDING_REGISTERS,
                   protocol::REG_LOAD_CONTROL_MODE, raw)) {
    settings.loadControlModeAvailable = true;
    settings.loadControlModeRaw = raw;
  }
  if (readRegister(protocol::READ_HOLDING_REGISTERS,
                   protocol::REG_RATED_VOLTAGE_LEVEL, raw)) {
    settings.ratedVoltageLevelAvailable = true;
    settings.ratedVoltageLevelRaw = raw;
  }
  if (readRegister(protocol::READ_HOLDING_REGISTERS,
                   protocol::REG_MAX_CHARGING_CURRENT, raw)) {
    settings.maxChargingCurrentAvailable = true;
    settings.maxChargingCurrentA =
        static_cast<float>(raw) * protocol::CURRENT_SCALE;
  }

  settings.status = ReadingStatus::ValidUncalibrated;
  return settings;
}

bool SolarControllerEpLs1024B::readVoltageBlock(
    uint16_t (&values)[protocol::VOLTAGE_BLOCK_COUNT]) {
  if (!communicationConfigurationValid()) return false;
  if (!initialized_ || !transport_.initialized()) return false;

  ReadingStatus failureStatus = ReadingStatus::ReadError;
  const uint8_t *block = nullptr;
  if (!readRegisters(protocol::READ_HOLDING_REGISTERS,
                     protocol::VOLTAGE_BLOCK_START,
                     protocol::VOLTAGE_BLOCK_COUNT,
                     protocol::VOLTAGE_BLOCK_BYTE_COUNT, block,
                     failureStatus)) {
    return false;
  }
  for (size_t i = 0; i < protocol::VOLTAGE_BLOCK_COUNT; ++i) {
    values[i] = protocol::decodeUint16(&block[i * 2]);
  }
  return true;
}

ReadingStatus SolarControllerEpLs1024B::probeAddress(uint8_t address,
                                                     float &batteryVoltageV) {
  if (address < 1 || address > 247) return ReadingStatus::ConfigurationMissing;
  if (!transport_.initialized()) return ReadingStatus::NotInitialized;
  if (configuration_.responseTimeoutMs == 0)
    return ReadingStatus::ConfigurationMissing;

  ReadingStatus failureStatus = ReadingStatus::ReadError;
  const uint8_t *data = nullptr;
  if (!readRegistersAt(address, protocol::READ_INPUT_REGISTERS,
                       protocol::REG_BATTERY_VOLTAGE, 1, 2, data,
                       failureStatus)) {
    return failureStatus;
  }
  batteryVoltageV = protocol::decodeScaledUint16(data, protocol::VOLTAGE_SCALE);
  return ReadingStatus::ValidUncalibrated;
}

SolarWriteStatus SolarControllerEpLs1024B::writeVoltageBlock(
    const uint16_t (&values)[protocol::VOLTAGE_BLOCK_COUNT]) {
#if SOLAR_CONTROLLER_WRITES_ENABLED
  const auto request =
      protocol::makeWriteVoltageBlockRequest(configuration_.slaveAddress,
                                             values);
  const Rs485Result result = transport_.transact(
      request.bytes, sizeof(request.bytes), configuration_.responseTimeoutMs,
      configuration_.debug);
  lastExceptionCode_ = result.exceptionCode;
  if (!result.ok()) {
    return result.status == Rs485Status::Timeout ? SolarWriteStatus::Timeout
                                                 : SolarWriteStatus::BlockWriteRejected;
  }

  // FC10 answers with an eight-byte echo of the address, function, first
  // register, and register count. A mismatch is not an accepted write.
  const uint8_t *frame = transport_.rawData();
  if (frame == nullptr || transport_.rawLength() != 8 || frame[0] != request.bytes[0] ||
      frame[1] != protocol::WRITE_MULTIPLE_REGISTERS ||
      frame[2] != request.bytes[2] || frame[3] != request.bytes[3] ||
      frame[4] != 0 || frame[5] != static_cast<uint8_t>(protocol::VOLTAGE_BLOCK_COUNT)) {
    return SolarWriteStatus::BlockWriteRejected;
  }
  return SolarWriteStatus::Applied;
#else
  (void)values;
  return SolarWriteStatus::WritesDisabled;
#endif
}

SolarVoltageBlockWriteReport SolarControllerEpLs1024B::applyVoltageBlock(
    const protocol::SolarVoltageBlockProfile &profile) {
  SolarVoltageBlockWriteReport report{};
  for (size_t i = 0; i < protocol::VOLTAGE_BLOCK_COUNT; ++i) {
    report.results[i].reg = protocol::VOLTAGE_BLOCK_FIELDS[i].reg;
    report.results[i].requested = profile.values[i];
  }

#if SOLAR_CONTROLLER_WRITES_ENABLED
  if (!communicationConfigurationValid()) {
    report.status = SolarWriteStatus::ConfigurationMissing;
    return report;
  }
  if (!initialized_ || !transport_.initialized()) {
    report.status = SolarWriteStatus::NotInitialized;
    return report;
  }

  // The controller rejects mutually inconsistent setpoints, so nothing is sent
  // unless the block is ordered the way the controller expects.
  if (!protocol::voltageBlockOrdered(profile.values)) {
    report.status = SolarWriteStatus::SetpointsInvalid;
    return report;
  }

  // Preconditions: the setpoints mean "user-defined 12 V battery" setpoints, so
  // they are never written onto another battery type or system voltage.
  uint16_t batteryType = 0;
  uint16_t ratedVoltageLevel = 0;
  if (!readRegister(protocol::READ_HOLDING_REGISTERS,
                    protocol::REG_BATTERY_TYPE, batteryType) ||
      !readRegister(protocol::READ_HOLDING_REGISTERS,
                    protocol::REG_RATED_VOLTAGE_LEVEL, ratedVoltageLevel)) {
    report.status = SolarWriteStatus::ReadError;
    return report;
  }
  report.preconditionsChecked = true;
  report.observedBatteryType = batteryType;
  report.observedRatedVoltageLevel = ratedVoltageLevel;
  if (batteryType != configuration_.expectedBatteryType ||
      ratedVoltageLevel != configuration_.expectedRatedVoltageLevel) {
    report.status = SolarWriteStatus::PreconditionFailed;
    return report;
  }

  uint16_t existing[protocol::VOLTAGE_BLOCK_COUNT] = {};
  if (!readVoltageBlock(existing)) {
    report.status = SolarWriteStatus::ReadError;
    return report;
  }
  bool differs = false;
  for (size_t i = 0; i < protocol::VOLTAGE_BLOCK_COUNT; ++i) {
    report.results[i].readBack = existing[i];
    report.results[i].unchanged = existing[i] == profile.values[i];
    if (!report.results[i].unchanged) differs = true;
  }
  if (!differs) {
    // The block already matches, so the EEPROM is left alone.
    for (size_t i = 0; i < protocol::VOLTAGE_BLOCK_COUNT; ++i)
      report.results[i].verified = true;
    report.verified = protocol::VOLTAGE_BLOCK_COUNT;
    report.status = SolarWriteStatus::Applied;
    return report;
  }

  report.writeAttempted = true;
  const SolarWriteStatus writeStatus = writeVoltageBlock(profile.values);
  if (writeStatus != SolarWriteStatus::Applied) {
    report.status = writeStatus;
    return report;
  }
  report.writeAccepted = true;

  // The echo is not proof that the setpoints took effect, so the whole block is
  // read back before anything is reported as applied.
  delay(VOLTAGE_BLOCK_SETTLE_MS);
  uint16_t verified[protocol::VOLTAGE_BLOCK_COUNT] = {};
  if (!readVoltageBlock(verified)) {
    report.status = SolarWriteStatus::ReadError;
    return report;
  }
  for (size_t i = 0; i < protocol::VOLTAGE_BLOCK_COUNT; ++i) {
    report.results[i].readBack = verified[i];
    report.results[i].verified = verified[i] == profile.values[i];
    if (report.results[i].verified) ++report.verified;
  }
  report.status = report.allVerified() ? SolarWriteStatus::Applied
                                       : SolarWriteStatus::VerificationMismatch;
  return report;
#else
  (void)profile;
  report.status = SolarWriteStatus::WritesDisabled;
  return report;
#endif
}

SolarServiceExchange SolarControllerEpLs1024B::captureServiceExchange(
    const protocol::ServiceRequest &request) {
  SolarServiceExchange exchange{};
  if (!initialized_ || !transport_.initialized()) return exchange;

  // The proprietary command is treated as fire-and-forget: whether a response
  // arrives is recorded, never required, and never interpreted.
  const Rs485Result result = transport_.transact(
      request.bytes, sizeof(request.bytes), configuration_.responseTimeoutMs,
      configuration_.debug);
  exchange.frameSent = true;
  exchange.responseReceived = result.ok();

  const uint8_t *frame = transport_.rawData();
  const size_t rawLength = transport_.rawLength();
  if (frame != nullptr && rawLength > 0) {
    const size_t captured = rawLength < sizeof(exchange.response)
                                ? rawLength
                                : sizeof(exchange.response);
    memcpy(exchange.response, frame, captured);
    exchange.responseLength = captured;
    exchange.responseTruncated = captured < rawLength;
  }
  return exchange;
}

SolarServiceExchange SolarControllerEpLs1024B::sendServiceFindId() {
  // Read-only query: the find marker does not change the stored address, so it
  // stays available in builds that compile the writes out.
  return captureServiceExchange(protocol::makeServiceFindIdRequest());
}

SolarServiceExchange SolarControllerEpLs1024B::sendServiceSetId(
    uint8_t newAddress) {
#if SOLAR_CONTROLLER_WRITES_ENABLED
  if (newAddress < 1 || newAddress > 247) return {};
  return captureServiceExchange(protocol::makeServiceSetIdRequest(newAddress));
#else
  (void)newAddress;
  return {};
#endif
}

SolarAddressChangeReport SolarControllerEpLs1024B::changeAddress(
    uint8_t newAddress, uint16_t settleMs) {
  SolarAddressChangeReport report{};
  report.previousAddress = configuration_.slaveAddress;
  report.newAddress = newAddress;

#if SOLAR_CONTROLLER_WRITES_ENABLED
  if (newAddress < 1 || newAddress > 247 ||
      newAddress == configuration_.slaveAddress ||
      !communicationConfigurationValid() || !initialized_ ||
      !transport_.initialized()) {
    return report;
  }
  report.requestAccepted = true;

  report.find = captureServiceExchange(protocol::makeServiceFindIdRequest());
  report.setId = captureServiceExchange(
      protocol::makeServiceSetIdRequest(newAddress));
  if (!report.setId.frameSent) {
    report.requestAccepted = false;
    return report;
  }

  // The controller needs a moment before it answers on the new address.
  delay(settleMs);
  report.settled = true;

  float newVoltage = 0.0F;
  report.newAddressProbe = probeAddress(newAddress, newVoltage);
  report.newAddressBatteryVoltageV = newVoltage;

  float previousVoltage = 0.0F;
  report.previousAddressProbe =
      probeAddress(report.previousAddress, previousVoltage);
  return report;
#else
  (void)settleMs;
  return report;
#endif
}

}  // namespace irrigation::pressure_node
