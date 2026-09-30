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

  // A REAL4 float is 4 bytes, but Modbus carries 2 bytes per register, so each
  // value arrives as two 16-bit halves. The meter decides which half comes
  // first: LOW_WORD_FIRST means the low-value half. Decoding them backwards does
  // not give a slightly wrong number -- 2.5 m3/h reads as about 2.3e-41, which
  // displays as zero -- so a wrong guess looks like "no water flowing" rather
  // than like a fault.
  //
  // These two fields record which order this build is configured to decode with,
  // and whether that choice has been confirmed against real hardware. Code that
  // reports a word-order verdict must read both rather than assuming
  // LOW_WORD_FIRST: the same source also builds for meters whose order is not
  // yet known.
  bool configuredLowWordFirst{false};
  bool configuredOrderValidated{false};

  // Meter diagnostics read alongside the measurement. Without these the probe
  // cannot tell a good decode from a decodable-looking one; the meter reports
  // poor signal and empty pipe in REG0072 rather than in the flow registers.
  uint16_t deviceErrorBits{0};
  bool diagnosticsAvailable{false};
  // deviceErrorBits tested against the driver's flow-validity mask. Kept here
  // as a verdict so the protocol's bit layout stays inside the driver.
  bool flowSampleValid{false};

  // REG0092-0094: working step / quality factor and both transducer signal
  // strengths. Q drops toward zero when the receive signal degrades.
  uint8_t signalQuality{0};
  uint16_t upstreamSignalStrength{0};
  uint16_t downstreamSignalStrength{0};
  bool signalQualityAvailable{false};

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
