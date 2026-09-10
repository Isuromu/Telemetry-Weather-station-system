#pragma once

#include <Arduino.h>
#include <ConfigTypes.h>
#include <DelixiCDIE100.h>
#include <PrintController.h>

struct PumpStatus {
  bool communicationOk{false};
  bool configurationValid{false};
  bool running{false};
  bool frequencyArmed{false};
  DelixiRunState runState{DelixiRunState::Unknown};
  float commandedFrequencyHz{0.0f};
  float actualFrequencyHz{0.0f};
  float referenceFrequencyHz{0.0f};
  float motorCurrentA{0.0f};
  float outputVoltageV{0.0f};
  float estimatedMotorRpm{0.0f};
  uint16_t vfdFaultCode{0};
  Rs485Status lastCommunicationError{Rs485Status::NotInitialized};
  uint32_t uptimeSeconds{0};
};

class PumpController {
 public:
  PumpController(DelixiCDIE100 &vfd, PrintController &logger,
                 const irrigation::MotorProfile &motor);

  bool begin();
  bool start();
  bool stop();
  bool emergencyStop();
  bool resetFault();
  bool setSpeedHz(float hz);

  void setConfigurationValid(bool valid);
  bool poll(bool force = false);
  const PumpStatus &status() const { return status_; }
  const irrigation::MotorProfile &motor() const { return motor_; }

 private:
  DelixiCDIE100 &vfd_;
  PrintController &logger_;
  const irrigation::MotorProfile &motor_;
  PumpStatus status_;
  uint32_t lastPollMs_;
  uint16_t pollIntervalMs_;

  void updateCommunicationState(bool success);
};

