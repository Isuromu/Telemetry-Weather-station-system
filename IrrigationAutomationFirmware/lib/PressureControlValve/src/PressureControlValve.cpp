#include "PressureControlValve.h"

namespace irrigation::pressure_node {

PressureControlValve::PressureControlValve(
    PressureControlValveConfiguration configuration)
    : configuration_(configuration) {}

bool PressureControlValve::configurationValid() const {
  return configuration_.pulseMs > 0 &&
         areOppositePolarities(configuration_.openPolarity,
                               configuration_.closePolarity);
}

bool PressureControlValve::begin() {
  // Configure the direction first: the Arduino-ESP32 core rejects
  // digitalWrite() on a pin that is not yet set as a GPIO, which would make
  // the safe-level writes below silently do nothing.
  pinMode(configuration_.in1Pin, OUTPUT);
  pinMode(configuration_.in2Pin, OUTPUT);
  pinMode(configuration_.powerEnablePin, OUTPUT);

  // Bring the H-bridge and its high-side power switch up in their idle state
  // before any command can be applied.
  digitalWrite(configuration_.in1Pin, LOW);
  digitalWrite(configuration_.in2Pin, LOW);
  digitalWrite(configuration_.powerEnablePin,
               configuration_.powerEnableActiveHigh ? LOW : HIGH);

  setBridgeIdle();
  setPower(false);
  stateModel_.reset();
  initialized_ = configurationValid();
  return initialized_;
}

ValveActuationStatus PressureControlValve::commandOpen() {
  return command(PressureControlValveState::Open);
}

ValveActuationStatus PressureControlValve::commandClose() {
  return command(PressureControlValveState::Closed);
}

ValveActuationStatus PressureControlValve::command(
    PressureControlValveState desiredState) {
  if (!configurationValid()) {
    return ValveActuationStatus::InvalidConfiguration;
  }
  if (!initialized_) return ValveActuationStatus::NotInitialized;
  if (desiredState == PressureControlValveState::Unknown) {
    return ValveActuationStatus::InvalidCommand;
  }

  setBridgeIdle();
  setPower(true);
  delay(configuration_.powerSettleMs);
  setBridge(desiredState == PressureControlValveState::Open
                ? configuration_.openPolarity
                : configuration_.closePolarity);
  delay(configuration_.pulseMs);
  setBridgeIdle();
  delay(configuration_.postPulseMs);
  setPower(false);

  stateModel_.recordSuccessfulCommand(desiredState);
  stateModel_.markVerificationUnavailable();
  return ValveActuationStatus::Ok;
}

bool PressureControlValve::restoreLastCommandedState(
    PressureControlValveState state) {
  if (!initialized_ || state == PressureControlValveState::Unknown)
    return false;
  stateModel_.recordSuccessfulCommand(state);
  stateModel_.markVerificationUnavailable();
  return true;
}

void PressureControlValve::forceSafeIdle() {
  setBridgeIdle();
  setPower(false);
}

void PressureControlValve::setPower(bool enabled) {
  const bool outputHigh =
      enabled ? configuration_.powerEnableActiveHigh
              : !configuration_.powerEnableActiveHigh;
  digitalWrite(configuration_.powerEnablePin, outputHigh ? HIGH : LOW);
}

void PressureControlValve::setBridgeIdle() {
  digitalWrite(configuration_.in1Pin, LOW);
  digitalWrite(configuration_.in2Pin, LOW);
}

void PressureControlValve::setBridge(BridgePolarity polarity) {
  digitalWrite(configuration_.in1Pin, polarity.in1High ? HIGH : LOW);
  digitalWrite(configuration_.in2Pin, polarity.in2High ? HIGH : LOW);
}

}  // namespace irrigation::pressure_node
