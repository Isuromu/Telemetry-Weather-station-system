#pragma once

#include <stddef.h>
#include <stdint.h>

#include <PressureNodeTypes.h>

namespace irrigation::pressure_node {

struct FlowMeterWordOrderProbe {
  static constexpr size_t RAW_DATA_LENGTH = 12;

  ReadingStatus status{ReadingStatus::NotInitialized};
  uint8_t rawData[RAW_DATA_LENGTH]{};
  float highWordFirstFlowRateM3PerHour{0.0F};
  float highWordFirstVelocityMetersPerSecond{0.0F};
  float lowWordFirstFlowRateM3PerHour{0.0F};
  float lowWordFirstVelocityMetersPerSecond{0.0F};
  bool highWordFirstDecoded{false};
  bool lowWordFirstDecoded{false};

  constexpr bool hasResponse() const {
    return status == ReadingStatus::ValidUncalibrated;
  }
};

class FlowMeter {
 public:
  virtual ~FlowMeter() = default;
  virtual bool begin() = 0;
  virtual bool available() const = 0;
  virtual FlowReading read() = 0;
  virtual FlowTotalReading readTotals() = 0;
  virtual FlowMeterWordOrderProbe probeWordOrder() {
    FlowMeterWordOrderProbe probe{};
    probe.status = ReadingStatus::ConfigurationMissing;
    return probe;
  }
};

class UnavailableFlowMeter final : public FlowMeter {
 public:
  bool begin() override { return false; }
  bool available() const override { return false; }
  FlowReading read() override {
    return {ReadingStatus::ConfigurationMissing, 0.0F, FlowUnit::Unknown};
  }
  FlowTotalReading readTotals() override {
    FlowTotalReading reading{};
    reading.status = ReadingStatus::ConfigurationMissing;
    return reading;
  }
};

}  // namespace irrigation::pressure_node
