#pragma once

#include <Arduino.h>
#include <PressureNodeTypes.h>

namespace irrigation::pressure_node {

struct PressureControlValveConfiguration {
  uint8_t in1Pin;
  uint8_t in2Pin;
  uint8_t powerEnablePin;
  bool powerEnableActiveHigh;
  uint16_t powerSettleMs;
  uint16_t pulseMs;
  uint16_t postPulseMs;
  uint16_t hydraulicSettleMs;
  BridgePolarity openPolarity;
  BridgePolarity closePolarity;
};

class PressureControlValve {
 public:
  explicit PressureControlValve(
      PressureControlValveConfiguration configuration);

  bool begin();
  ValveActuationStatus commandOpen();
  ValveActuationStatus commandClose();
  ValveActuationStatus command(PressureControlValveState desiredState);
  bool restoreLastCommandedState(PressureControlValveState state);
  void forceSafeIdle();

  const ValveStatus &status() const { return stateModel_.status(); }
  uint16_t hydraulicSettleMs() const {
    return configuration_.hydraulicSettleMs;
  }
  bool configurationValid() const;

 private:
  PressureControlValveConfiguration configuration_;
  PressureControlValveStateModel stateModel_{};
  bool initialized_{false};

  void setPower(bool enabled);
  void setBridgeIdle();
  void setBridge(BridgePolarity polarity);
};

}  // namespace irrigation::pressure_node
