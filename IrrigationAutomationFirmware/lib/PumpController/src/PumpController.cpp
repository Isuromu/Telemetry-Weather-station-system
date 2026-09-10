#include "PumpController.h"

PumpController::PumpController(DelixiCDIE100 &vfd, PrintController &logger,
                               const irrigation::MotorProfile &motor)
    : vfd_(vfd),
      logger_(logger),
      motor_(motor),
      lastPollMs_(0),
      pollIntervalMs_(1000) {}

bool PumpController::begin() {
  status_ = {};
  status_.commandedFrequencyHz = motor_.minRunFrequencyHz;
  status_.lastCommunicationError = Rs485Status::NotInitialized;
  return true;
}

void PumpController::setConfigurationValid(bool valid) {
  status_.configurationValid = valid;
}

void PumpController::updateCommunicationState(bool success) {
  status_.communicationOk = success;
  status_.lastCommunicationError = vfd_.lastCommunicationStatus();
}

bool PumpController::setSpeedHz(float hz) {
  if (hz < motor_.minRunFrequencyHz || hz > motor_.maxRunFrequencyHz) {
    logger_.print(F("[PUMP][ERROR] Frequency must be in range "), true);
    logger_.print(motor_.minRunFrequencyHz, true, "", 2);
    logger_.print(F(".."), true);
    logger_.print(motor_.maxRunFrequencyHz, true, "", 2);
    logger_.println(F(" Hz."), true);
    return false;
  }
  const bool success = vfd_.setFrequencyHz(hz);
  updateCommunicationState(success);
  if (success) {
    status_.commandedFrequencyHz = hz;
    status_.frequencyArmed = true;
  }
  return success;
}

bool PumpController::start() {
  if (!status_.configurationValid) {
    logger_.println(
        F("[PUMP][ERROR] Start blocked: VFD configuration has errors or was not checked."),
        true);
    return false;
  }
  if (!status_.frequencyArmed) {
    logger_.println(
        F("[PUMP][ERROR] Start blocked: set an explicit frequency after boot."),
        true);
    return false;
  }
  uint16_t fault = 0;
  if (!vfd_.readFaultCode(fault)) {
    updateCommunicationState(false);
    return false;
  }
  status_.vfdFaultCode = fault;
  if (fault != 0) {
    logger_.print(F("[PUMP][ERROR] Start blocked by VFD fault: "), true);
    logger_.println(DelixiCDIE100::faultName(fault), true);
    return false;
  }
  DelixiRunState state = DelixiRunState::Unknown;
  if (!vfd_.readRunState(state)) {
    updateCommunicationState(false);
    return false;
  }
  updateCommunicationState(true);
  if (state == DelixiRunState::Reverse) {
    logger_.println(F("[PUMP][ERROR] Start blocked: reverse state detected."),
                    true);
    return false;
  }
  if (!vfd_.setFrequencyHz(status_.commandedFrequencyHz) ||
      !vfd_.runForward()) {
    updateCommunicationState(false);
    return false;
  }
  updateCommunicationState(true);
  status_.runState = DelixiRunState::Forward;
  status_.running = true;
  return true;
}

bool PumpController::stop() {
  const bool success = vfd_.stop();
  updateCommunicationState(success);
  if (success) {
    status_.running = false;
    status_.runState = DelixiRunState::Stopped;
  }
  return success;
}

bool PumpController::emergencyStop() {
  const bool success = vfd_.freeStop();
  updateCommunicationState(success);
  status_.running = false;
  status_.frequencyArmed = false;
  if (success) status_.runState = DelixiRunState::Stopped;
  logger_.println(
      F("[PUMP] Software emergency stop uses CDI-E free stop. It is not a physical emergency-stop circuit."),
      true);
  return success;
}

bool PumpController::resetFault() {
  const bool success = vfd_.resetFault();
  updateCommunicationState(success);
  if (success) status_.vfdFaultCode = 0;
  return success;
}

bool PumpController::poll(bool force) {
  const uint32_t now = millis();
  status_.uptimeSeconds = now / 1000U;
  if (!force && (now - lastPollMs_) < pollIntervalMs_) return true;
  lastPollMs_ = now;

  if (!vfd_.readRunState(status_.runState) ||
      !vfd_.readFaultCode(status_.vfdFaultCode) ||
      !vfd_.readOutputFrequency(status_.actualFrequencyHz) ||
      !vfd_.readReferenceFrequency(status_.referenceFrequencyHz) ||
      !vfd_.readOutputCurrent(status_.motorCurrentA) ||
      !vfd_.readOutputVoltage(status_.outputVoltageV)) {
    updateCommunicationState(false);
    return false;
  }

  status_.running = status_.runState == DelixiRunState::Forward;
  status_.estimatedMotorRpm =
      motor_.ratedFrequencyHz > 0.0f
          ? status_.actualFrequencyHz * motor_.ratedRpm /
                motor_.ratedFrequencyHz
          : 0.0f;
  updateCommunicationState(true);
  return true;
}
