#pragma once

#include <PressureNodeTypes.h>
#include <RS485ModBus.h>

#include "EpeverLs1024bProtocol.h"

namespace irrigation::pressure_node {

struct SolarControllerConfiguration {
  uint8_t slaveAddress{0};
  uint16_t responseTimeoutMs{300};
  bool debug{false};
  // The charge profile is only applied to a controller that is already set up as
  // a user-defined 12 V battery. Anything else aborts before a write, because the
  // 0x9003..0x900E setpoints mean different things under a factory battery type.
  uint16_t expectedBatteryType{epever_ls1024b::protocol::BATTERY_TYPE_USER};
  uint16_t expectedRatedVoltageLevel{
      epever_ls1024b::protocol::RATED_VOLTAGE_12V};
};

// Point-in-time view of the controller's real-time input registers.
struct SolarControllerMeasurements {
  ReadingStatus status{ReadingStatus::NotSampled};
  bool pvAndBatteryBlockAvailable{false};
  float pvVoltageV{0.0F};
  float pvCurrentA{0.0F};
  float batteryVoltageV{0.0F};
  float chargingCurrentA{0.0F};
  // The load and temperature registers sit in a second transfer, so a failure
  // there must not be reported as measured zeroes.
  bool loadAndTemperatureBlockAvailable{false};
  float loadVoltageV{0.0F};
  float loadCurrentA{0.0F};
  float batteryTemperatureC{0.0F};
  float deviceTemperatureC{0.0F};
  // The state-of-charge and status registers sit outside both blocks and may not
  // answer on every firmware revision.
  bool batterySocAvailable{false};
  uint16_t batterySocPercent{0};
  bool statusAvailable{false};
  uint16_t batteryStatusRaw{0};
  uint16_t chargingStatusRaw{0};

  constexpr bool hasSample() const { return readingHasSample(status); }
};

// The whole settings area as it currently stands on the controller. Only
// 0x9003..0x900E is ever written; the rest is reported so a bench session can
// confirm the preconditions and compare against the controller's own display.
struct SolarControllerSettings {
  ReadingStatus status{ReadingStatus::NotSampled};
  uint16_t batteryType{0};
  bool tailAvailable{false};
  uint16_t batteryCapacityAh{0};
  uint16_t temperatureCompensationRaw{0};
  float overVoltageDisconnectV{0.0F};
  float chargingLimitV{0.0F};
  float overVoltageReconnectV{0.0F};
  float equalizeChargingV{0.0F};
  float boostChargingV{0.0F};
  float floatChargingV{0.0F};
  float boostReconnectV{0.0F};
  float lowVoltageReconnectV{0.0F};
  float underVoltageRecoverV{0.0F};
  float underVoltageWarningV{0.0F};
  float lowVoltageDisconnectV{0.0F};
  float dischargingLimitV{0.0F};
  bool loadControlModeAvailable{false};
  uint16_t loadControlModeRaw{0};
  bool ratedVoltageLevelAvailable{false};
  uint16_t ratedVoltageLevelRaw{0};
  bool maxChargingCurrentAvailable{false};
  float maxChargingCurrentA{0.0F};

  constexpr bool hasSample() const { return readingHasSample(status); }
};

enum class SolarWriteStatus : uint8_t {
  NotAttempted,
  Applied,
  VerificationMismatch,
  // The controller answered the write but not with a matching FC10 echo.
  BlockWriteRejected,
  // The local ordering rule refused the setpoints; nothing was sent.
  SetpointsInvalid,
  // Battery type or rated voltage does not match this installation.
  PreconditionFailed,
  Timeout,
  ReadError,
  NotInitialized,
  ConfigurationMissing,
  WritesDisabled
};

constexpr const char *solarWriteStatusName(SolarWriteStatus status) {
  switch (status) {
    case SolarWriteStatus::NotAttempted:
      return "NOT_ATTEMPTED";
    case SolarWriteStatus::Applied:
      return "APPLIED";
    case SolarWriteStatus::VerificationMismatch:
      return "VERIFICATION_MISMATCH";
    case SolarWriteStatus::BlockWriteRejected:
      return "BLOCK_WRITE_REJECTED";
    case SolarWriteStatus::SetpointsInvalid:
      return "SETPOINTS_INVALID";
    case SolarWriteStatus::PreconditionFailed:
      return "PRECONDITION_FAILED";
    case SolarWriteStatus::Timeout:
      return "TIMEOUT";
    case SolarWriteStatus::ReadError:
      return "READ_ERROR";
    case SolarWriteStatus::NotInitialized:
      return "NOT_INITIALIZED";
    case SolarWriteStatus::ConfigurationMissing:
      return "CONFIGURATION_MISSING";
    case SolarWriteStatus::WritesDisabled:
      return "WRITES_DISABLED";
  }
  return "UNKNOWN";
}

struct SolarBlockRegisterResult {
  uint16_t reg{0};
  uint16_t requested{0};
  uint16_t readBack{0};
  // True when the controller already held the requested value, so the block
  // write was skipped entirely.
  bool unchanged{false};
  bool verified{false};
};

struct SolarVoltageBlockWriteReport {
  SolarWriteStatus status{SolarWriteStatus::NotAttempted};
  bool preconditionsChecked{false};
  uint16_t observedBatteryType{0};
  uint16_t observedRatedVoltageLevel{0};
  bool writeAttempted{false};
  bool writeAccepted{false};
  size_t verified{0};
  SolarBlockRegisterResult results[epever_ls1024b::protocol::VOLTAGE_BLOCK_COUNT]{};

  constexpr bool allVerified() const {
    return verified == epever_ls1024b::protocol::VOLTAGE_BLOCK_COUNT;
  }
};

// One captured service exchange. The response format of the proprietary 0x45
// command is undocumented, so it is recorded verbatim and never parsed.
struct SolarServiceExchange {
  bool frameSent{false};
  bool responseReceived{false};
  bool responseTruncated{false};
  size_t responseLength{0};
  uint8_t response[epever_ls1024b::protocol::SERVICE_MAX_CAPTURED_RESPONSE]{};
};

struct SolarAddressChangeReport {
  bool requestAccepted{false};
  uint8_t previousAddress{0};
  uint8_t newAddress{0};
  SolarServiceExchange find{};
  SolarServiceExchange setId{};
  ReadingStatus newAddressProbe{ReadingStatus::NotSampled};
  ReadingStatus previousAddressProbe{ReadingStatus::NotSampled};
  float newAddressBatteryVoltageV{0.0F};
  bool settled{false};

  // Mirrors the verification verdict of the official PC tool: the change is only
  // confirmed when the new address answers and the old one stops answering.
  constexpr bool confirmed() const {
    return readingHasSample(newAddressProbe) &&
           !readingHasSample(previousAddressProbe);
  }
  constexpr bool ambiguous() const {
    return readingHasSample(newAddressProbe) &&
           readingHasSample(previousAddressProbe);
  }
};

class SolarControllerEpLs1024B {
 public:
  explicit SolarControllerEpLs1024B(
      RS485Bus &transport,
      SolarControllerConfiguration configuration = SolarControllerConfiguration{});

  bool begin();
  bool available() const { return initialized_; }
  bool configurationValid() const { return communicationConfigurationValid(); }
  bool communicationConfigurationValid() const;
  uint8_t slaveAddress() const { return configuration_.slaveAddress; }
  // Exception code of the most recent rejected transaction, 0 when the last
  // exchange was clean.
  uint8_t lastExceptionCode() const { return lastExceptionCode_; }

  SolarControllerMeasurements readMeasurements();
  SolarControllerSettings readSettings();
  // Current 0x9003..0x900E values, in protocol::VOLTAGE_BLOCK_FIELDS order.
  bool readVoltageBlock(uint16_t (&values)[epever_ls1024b::protocol::VOLTAGE_BLOCK_COUNT]);

  // Bench diagnostics: read one register with an explicit function code.
  bool readRegister(uint8_t function, uint16_t registerAddress,
                    uint16_t &rawValue);

  // Reads battery voltage from an arbitrary address. Used to confirm an address
  // change without touching the configured slave address.
  ReadingStatus probeAddress(uint8_t address, float &batteryVoltageV);

  // Applies the twelve mutually constrained setpoints as one FC10 block write,
  // after checking the local ordering rule and the configured battery type and
  // rated voltage. Read back before any register is reported as applied.
  SolarVoltageBlockWriteReport applyVoltageBlock(
      const epever_ls1024b::protocol::SolarVoltageBlockProfile &profile);

  SolarServiceExchange sendServiceFindId();
  SolarServiceExchange sendServiceSetId(uint8_t newAddress);
  SolarAddressChangeReport changeAddress(uint8_t newAddress,
                                        uint16_t settleMs = 750);

  static constexpr bool writesCompiledIn() {
#if SOLAR_CONTROLLER_WRITES_ENABLED
    return true;
#else
    return false;
#endif
  }

 private:
  RS485Bus &transport_;
  SolarControllerConfiguration configuration_;
  bool transportReady_{false};
  bool initialized_{false};
  mutable uint8_t lastExceptionCode_{0};

  ReadingStatus mapTransportStatus(Rs485Status status) const;
  bool readRegisters(uint8_t function, uint16_t startAddress,
                     uint16_t registerCount, uint8_t expectedByteCount,
                     const uint8_t *&data, ReadingStatus &failureStatus);
  bool readRegistersAt(uint8_t address, uint8_t function,
                       uint16_t startAddress, uint16_t registerCount,
                       uint8_t expectedByteCount, const uint8_t *&data,
                       ReadingStatus &failureStatus);
  // Returns the status of the attempt: Applied when the controller echoed the
  // write, otherwise a failure status.
  SolarWriteStatus writeVoltageBlock(
      const uint16_t (&values)[epever_ls1024b::protocol::VOLTAGE_BLOCK_COUNT]);
  SolarServiceExchange captureServiceExchange(
      const epever_ls1024b::protocol::ServiceRequest &request);
};

}  // namespace irrigation::pressure_node
