#pragma once

#include <stdint.h>

namespace irrigation::pressure_node {

enum class ReadingStatus : uint8_t {
  NotSampled,
  Valid,
  ValidUncalibrated,
  NotInitialized,
  NotFound,
  ReadError,
  Timeout,
  ConfigurationMissing
};

constexpr bool readingHasSample(ReadingStatus status) {
  return status == ReadingStatus::Valid ||
         status == ReadingStatus::ValidUncalibrated;
}

constexpr const char *readingStatusName(ReadingStatus status) {
  switch (status) {
    case ReadingStatus::NotSampled:
      return "NOT_SAMPLED";
    case ReadingStatus::Valid:
      return "VALID";
    case ReadingStatus::ValidUncalibrated:
      return "VALID_UNCALIBRATED";
    case ReadingStatus::NotInitialized:
      return "NOT_INITIALIZED";
    case ReadingStatus::NotFound:
      return "NOT_FOUND";
    case ReadingStatus::ReadError:
      return "READ_ERROR";
    case ReadingStatus::Timeout:
      return "TIMEOUT";
    case ReadingStatus::ConfigurationMissing:
      return "CONFIGURATION_MISSING";
  }
  return "UNKNOWN";
}

struct PressureReading {
  ReadingStatus status{ReadingStatus::NotSampled};
  float pressureBar{0.0F};
  float temperatureC{0.0F};
  uint8_t address{0};

  constexpr bool hasSample() const { return readingHasSample(status); }
  constexpr bool engineeringUnitsValidated() const {
    return status == ReadingStatus::Valid;
  }
};

struct PressurePairReading {
  PressureReading upstream{};
  PressureReading downstream{};
};

enum class BatterySocStatus : uint8_t {
  Unknown,
  Estimated,
  Validated
};

constexpr const char *batterySocStatusName(BatterySocStatus status) {
  switch (status) {
    case BatterySocStatus::Unknown:
      return "UNKNOWN";
    case BatterySocStatus::Estimated:
      return "ESTIMATED";
    case BatterySocStatus::Validated:
      return "VALIDATED";
  }
  return "UNKNOWN";
}

struct BatteryReading {
  ReadingStatus status{ReadingStatus::NotSampled};
  float voltageV{0.0F};
  float adcVoltageV{0.0F};
  BatterySocStatus socStatus{BatterySocStatus::Unknown};
  float socPercent{0.0F};

  constexpr bool hasVoltage() const { return readingHasSample(status); }
  constexpr bool hasSoc() const {
    return socStatus != BatterySocStatus::Unknown;
  }
};

enum class FlowUnit : uint8_t {
  Unknown,
  LitersPerSecond,
  LitersPerMinute,
  CubicMetersPerHour
};

constexpr const char *flowUnitName(FlowUnit unit) {
  switch (unit) {
    case FlowUnit::Unknown:
      return "UNKNOWN";
    case FlowUnit::LitersPerSecond:
      return "L/s";
    case FlowUnit::LitersPerMinute:
      return "L/min";
    case FlowUnit::CubicMetersPerHour:
      return "m3/h";
  }
  return "UNKNOWN";
}

struct FlowReading {
  ReadingStatus status{ReadingStatus::NotSampled};
  float value{0.0F};
  FlowUnit unit{FlowUnit::Unknown};
  float velocityMetersPerSecond{0.0F};
  uint16_t deviceErrorBits{0};
  bool velocityAvailable{false};
  bool diagnosticsAvailable{false};

  constexpr bool hasSample() const { return readingHasSample(status); }
  constexpr bool hasVelocity() const {
    return hasSample() && velocityAvailable;
  }
};

struct FlowTotalReading {
  ReadingStatus status{ReadingStatus::NotSampled};
  float meterNetCubicMeters{0.0F};
  float meterPositiveCubicMeters{0.0F};
  float meterNegativeCubicMeters{0.0F};
  float sinceResetCubicMeters{0.0F};
  bool sinceResetAvailable{false};

  constexpr bool hasMeterTotals() const { return readingHasSample(status); }
  constexpr bool hasSinceResetVolume() const {
    return hasMeterTotals() && sinceResetAvailable;
  }
};

struct FlowTotalResetResult {
  FlowTotalReading totals{};
  bool applied{false};
};

enum class PressureControlValveState : uint8_t {
  Unknown,
  Open,
  Closed
};

constexpr const char *valveStateName(PressureControlValveState state) {
  switch (state) {
    case PressureControlValveState::Unknown:
      return "UNKNOWN";
    case PressureControlValveState::Open:
      return "OPEN";
    case PressureControlValveState::Closed:
      return "CLOSED";
  }
  return "UNKNOWN";
}

enum class VerificationState : uint8_t {
  NotAvailable,
  Unverified,
  Verified,
  Failed
};

constexpr const char *verificationStateName(VerificationState state) {
  switch (state) {
    case VerificationState::NotAvailable:
      return "NOT_AVAILABLE";
    case VerificationState::Unverified:
      return "UNVERIFIED";
    case VerificationState::Verified:
      return "VERIFIED";
    case VerificationState::Failed:
      return "FAILED";
  }
  return "NOT_AVAILABLE";
}

struct ValveStatus {
  PressureControlValveState lastCommanded{PressureControlValveState::Unknown};
  PressureControlValveState verifiedOrInferred{
      PressureControlValveState::Unknown};
  VerificationState verification{VerificationState::NotAvailable};
};

class PressureControlValveStateModel {
 public:
  constexpr PressureControlValveStateModel() = default;

  constexpr void reset() { status_ = {}; }

  constexpr void recordSuccessfulCommand(PressureControlValveState state) {
    if (state != PressureControlValveState::Unknown) {
      status_.lastCommanded = state;
      status_.verifiedOrInferred = PressureControlValveState::Unknown;
      status_.verification = VerificationState::Unverified;
    }
  }

  constexpr void markVerificationUnavailable() {
    status_.verifiedOrInferred = PressureControlValveState::Unknown;
    status_.verification = VerificationState::NotAvailable;
  }

  constexpr void recordVerification(PressureControlValveState state,
                                    bool succeeded) {
    status_.verifiedOrInferred =
        succeeded ? state : PressureControlValveState::Unknown;
    status_.verification = succeeded ? VerificationState::Verified
                                     : VerificationState::Failed;
  }

  constexpr const ValveStatus &status() const { return status_; }

 private:
  ValveStatus status_{};
};

struct BridgePolarity {
  bool in1High{false};
  bool in2High{false};
};

constexpr bool isActiveBridgePolarity(BridgePolarity polarity) {
  return polarity.in1High != polarity.in2High;
}

constexpr bool areOppositePolarities(BridgePolarity first,
                                     BridgePolarity second) {
  return isActiveBridgePolarity(first) && isActiveBridgePolarity(second) &&
         first.in1High == second.in2High &&
         first.in2High == second.in1High;
}

enum class ValveActuationStatus : uint8_t {
  Ok,
  NotInitialized,
  InvalidConfiguration,
  InvalidCommand
};

constexpr const char *valveActuationStatusName(ValveActuationStatus status) {
  switch (status) {
    case ValveActuationStatus::Ok:
      return "OK";
    case ValveActuationStatus::NotInitialized:
      return "NOT_INITIALIZED";
    case ValveActuationStatus::InvalidConfiguration:
      return "INVALID_CONFIGURATION";
    case ValveActuationStatus::InvalidCommand:
      return "INVALID_COMMAND";
  }
  return "INVALID_COMMAND";
}

enum class PowerMode : uint8_t {
  Active,
  Idle,
  SleepReady
};

constexpr const char *powerModeName(PowerMode mode) {
  switch (mode) {
    case PowerMode::Active:
      return "ACTIVE";
    case PowerMode::Idle:
      return "IDLE";
    case PowerMode::SleepReady:
      return "SLEEP_READY";
  }
  return "IDLE";
}

struct PowerPolicyConfiguration {
  bool periodicSamplingEnabled{false};
  uint32_t sampleIntervalMs{0};
  uint32_t telemetryIntervalMs{0};
  bool deepSleepEnabled{false};
};

class PowerPolicy {
 public:
  explicit constexpr PowerPolicy(PowerPolicyConfiguration configuration)
      : configuration_(configuration) {}

  constexpr void markActive() { mode_ = PowerMode::Active; }
  constexpr void markIdle() { mode_ = PowerMode::Idle; }

  constexpr bool markSleepReady() {
    if (!configuration_.deepSleepEnabled) return false;
    mode_ = PowerMode::SleepReady;
    return true;
  }

  constexpr PowerMode mode() const { return mode_; }
  constexpr const PowerPolicyConfiguration &configuration() const {
    return configuration_;
  }

 private:
  PowerPolicyConfiguration configuration_{};
  PowerMode mode_{PowerMode::Idle};
};

struct PressureControlNodeStatus {
  ValveStatus valve{};
  PressureReading upstreamPressure{};
  PressureReading downstreamPressure{};
  BatteryReading battery{};
  FlowReading flow{};
  FlowTotalReading flowTotal{};
  PowerMode powerMode{PowerMode::Idle};
};

}  // namespace irrigation::pressure_node
