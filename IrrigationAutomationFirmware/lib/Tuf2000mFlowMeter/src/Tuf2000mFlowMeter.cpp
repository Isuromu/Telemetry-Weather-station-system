#include "Tuf2000mFlowMeter.h"

#include <math.h>

namespace irrigation::pressure_node {

Tuf2000mFlowMeter::Tuf2000mFlowMeter(
    RS485Bus &transport, Tuf2000mConfiguration configuration)
    : transport_(transport), configuration_(configuration) {}

bool Tuf2000mFlowMeter::configurationValid() const {
  return communicationConfigurationValid() &&
         configuration_.floatWordOrder !=
             tuf2000m::FloatWordOrder::Unspecified;
}

bool Tuf2000mFlowMeter::communicationConfigurationValid() const {
  return configuration_.slaveAddress >= 1 &&
         configuration_.slaveAddress <= 247 &&
         configuration_.responseTimeoutMs > 0;
}

bool Tuf2000mFlowMeter::begin() {
  transportReady_ =
      communicationConfigurationValid() && transport_.initialized();
  initialized_ = transportReady_ && configurationValid();
  return initialized_;
}

ReadingStatus Tuf2000mFlowMeter::mapTransportStatus(Rs485Status status) const {
  if (status == Rs485Status::NotInitialized)
    return ReadingStatus::NotInitialized;
  if (status == Rs485Status::Timeout) return ReadingStatus::Timeout;
  return ReadingStatus::ReadError;
}

bool Tuf2000mFlowMeter::readRegisters(uint16_t startAddress,
                                      uint16_t registerCount,
                                      uint8_t expectedByteCount,
                                      const uint8_t *&data,
                                      ReadingStatus &failureStatus) {
  const auto request =
      tuf2000m::protocol::makeReadHoldingRegistersRequest(
          configuration_.slaveAddress, startAddress, registerCount);
  const Rs485Result result = transport_.transact(
      request.bytes, sizeof(request.bytes), configuration_.responseTimeoutMs,
      configuration_.debug);
  if (!result.ok()) {
    failureStatus = mapTransportStatus(result.status);
    return false;
  }

  const uint8_t *frame = transport_.rawData();
  const size_t expectedFrameLength = static_cast<size_t>(expectedByteCount) + 5U;
  if (frame == nullptr || transport_.rawLength() != expectedFrameLength ||
      frame[0] != configuration_.slaveAddress ||
      frame[1] != tuf2000m::protocol::READ_HOLDING_REGISTERS ||
      frame[2] != expectedByteCount) {
    failureStatus = ReadingStatus::ReadError;
    return false;
  }
  data = &frame[3];
  return true;
}

FlowReading Tuf2000mFlowMeter::read() {
  FlowReading reading{};
  if (!configurationValid()) {
    reading.status = ReadingStatus::ConfigurationMissing;
    return reading;
  }
  if (!initialized_ || !transport_.initialized()) {
    reading.status = ReadingStatus::NotInitialized;
    return reading;
  }

  ReadingStatus failureStatus = ReadingStatus::ReadError;
  const uint8_t *measurementData = nullptr;
  if (!readRegisters(tuf2000m::protocol::FLOW_RATE_REGISTER,
                     tuf2000m::protocol::FLOW_AND_VELOCITY_REGISTER_COUNT,
                     tuf2000m::protocol::FLOW_AND_VELOCITY_BYTE_COUNT,
                     measurementData, failureStatus)) {
    reading.status = failureStatus;
    return reading;
  }

  if (!tuf2000m::protocol::decodeFlowAndVelocity(
          measurementData,
          tuf2000m::protocol::FLOW_AND_VELOCITY_BYTE_COUNT,
          configuration_.floatWordOrder, reading.value,
          reading.velocityMetersPerSecond) ||
      !isfinite(reading.value) || !isfinite(reading.velocityMetersPerSecond)) {
    reading.status = ReadingStatus::ReadError;
    return reading;
  }
  reading.unit = FlowUnit::CubicMetersPerHour;
  reading.velocityAvailable = true;

  const uint8_t *diagnosticData = nullptr;
  if (!readRegisters(tuf2000m::protocol::ERROR_CODE_REGISTER,
                     tuf2000m::protocol::ERROR_CODE_REGISTER_COUNT,
                     tuf2000m::protocol::ERROR_CODE_BYTE_COUNT,
                     diagnosticData, failureStatus)) {
    reading.status = failureStatus;
    return reading;
  }
  reading.deviceErrorBits =
      tuf2000m::protocol::decodeUint16(diagnosticData);
  reading.diagnosticsAvailable = true;
  reading.status =
      (reading.deviceErrorBits &
       tuf2000m::protocol::FLOW_VALIDITY_ERROR_MASK) == 0
          ? ReadingStatus::Valid
          : ReadingStatus::ReadError;
  return reading;
}

FlowTotalReading Tuf2000mFlowMeter::readTotals() {
  FlowTotalReading reading{};
  if (!configurationValid()) {
    reading.status = ReadingStatus::ConfigurationMissing;
    return reading;
  }
  if (!initialized_ || !transport_.initialized()) {
    reading.status = ReadingStatus::NotInitialized;
    return reading;
  }

  ReadingStatus failureStatus = ReadingStatus::ReadError;
  const uint8_t *totalData = nullptr;
  if (!readRegisters(tuf2000m::protocol::FLOW_TOTALS_REGISTER,
                     tuf2000m::protocol::FLOW_TOTALS_REGISTER_COUNT,
                     tuf2000m::protocol::FLOW_TOTALS_BYTE_COUNT, totalData,
                     failureStatus)) {
    reading.status = failureStatus;
    return reading;
  }

  if (!tuf2000m::protocol::decodeFlowTotals(
          totalData, tuf2000m::protocol::FLOW_TOTALS_BYTE_COUNT,
          configuration_.floatWordOrder, reading.meterNetCubicMeters,
          reading.meterPositiveCubicMeters,
          reading.meterNegativeCubicMeters) ||
      !isfinite(reading.meterNetCubicMeters) ||
      !isfinite(reading.meterPositiveCubicMeters) ||
      !isfinite(reading.meterNegativeCubicMeters)) {
    reading.status = ReadingStatus::ReadError;
    return reading;
  }

  reading.status = ReadingStatus::Valid;
  return reading;
}

FlowMeterWordOrderProbe Tuf2000mFlowMeter::probeWordOrder() {
  FlowMeterWordOrderProbe probe{};
  if (!communicationConfigurationValid()) {
    probe.status = ReadingStatus::ConfigurationMissing;
    return probe;
  }
  if (!transportReady_ || !transport_.initialized()) {
    probe.status = ReadingStatus::NotInitialized;
    return probe;
  }

  ReadingStatus failureStatus = ReadingStatus::ReadError;
  const uint8_t *measurementData = nullptr;
  if (!readRegisters(tuf2000m::protocol::FLOW_RATE_REGISTER,
                     tuf2000m::protocol::FLOW_AND_VELOCITY_REGISTER_COUNT,
                     tuf2000m::protocol::FLOW_AND_VELOCITY_BYTE_COUNT,
                     measurementData, failureStatus)) {
    probe.status = failureStatus;
    return probe;
  }

  memcpy(probe.rawData, measurementData, sizeof(probe.rawData));
  probe.highWordFirstDecoded =
      tuf2000m::protocol::decodeFlowAndVelocity(
          measurementData,
          tuf2000m::protocol::FLOW_AND_VELOCITY_BYTE_COUNT,
          tuf2000m::FloatWordOrder::HighWordFirst,
          probe.highWordFirstFlowRateM3PerHour,
          probe.highWordFirstVelocityMetersPerSecond) &&
      isfinite(probe.highWordFirstFlowRateM3PerHour) &&
      isfinite(probe.highWordFirstVelocityMetersPerSecond);
  probe.lowWordFirstDecoded =
      tuf2000m::protocol::decodeFlowAndVelocity(
          measurementData,
          tuf2000m::protocol::FLOW_AND_VELOCITY_BYTE_COUNT,
          tuf2000m::FloatWordOrder::LowWordFirst,
          probe.lowWordFirstFlowRateM3PerHour,
          probe.lowWordFirstVelocityMetersPerSecond) &&
      isfinite(probe.lowWordFirstFlowRateM3PerHour) &&
      isfinite(probe.lowWordFirstVelocityMetersPerSecond);
  probe.status = ReadingStatus::ValidUncalibrated;
  return probe;
}

}  // namespace irrigation::pressure_node
