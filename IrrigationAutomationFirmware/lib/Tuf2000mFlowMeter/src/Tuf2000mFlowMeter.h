#pragma once

#include <FlowMeter.h>
#include <RS485ModBus.h>

#include "Tuf2000mProtocol.h"

namespace irrigation::pressure_node {

struct Tuf2000mConfiguration {
  uint8_t slaveAddress{0};
  uint16_t responseTimeoutMs{300};
  tuf2000m::FloatWordOrder floatWordOrder{
      tuf2000m::FloatWordOrder::Unspecified};
  bool debug{false};
};

class Tuf2000mFlowMeter final : public FlowMeter {
 public:
  explicit Tuf2000mFlowMeter(
      RS485Bus &transport,
      Tuf2000mConfiguration configuration = Tuf2000mConfiguration{});

  bool begin() override;
  bool available() const override { return initialized_; }
  FlowReading read() override;
  FlowTotalReading readTotals() override;
  FlowMeterWordOrderProbe probeWordOrder() override;

  static constexpr bool protocolImplemented() { return true; }
  bool configurationValid() const;
  bool communicationConfigurationValid() const;

 private:
  RS485Bus &transport_;
  Tuf2000mConfiguration configuration_;
  bool transportReady_{false};
  bool initialized_{false};

  ReadingStatus mapTransportStatus(Rs485Status status) const;
  bool readRegisters(uint16_t startAddress, uint16_t registerCount,
                     uint8_t expectedByteCount, const uint8_t *&data,
                     ReadingStatus &failureStatus);
};

}  // namespace irrigation::pressure_node
