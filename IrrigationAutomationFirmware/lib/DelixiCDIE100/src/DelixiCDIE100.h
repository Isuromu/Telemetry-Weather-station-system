#pragma once

#include <Arduino.h>
#include <ConfigTypes.h>
#include <PrintController.h>
#include <RS485ModBus.h>

#include "DelixiProtocol.h"

enum class DelixiRunState : uint16_t {
  Unknown = 0,
  Forward = 1,
  Reverse = 2,
  Stopped = 3
};

enum class DelixiConfigSeverity : uint8_t {
  Ok,
  Warning,
  Error
};

struct DelixiConfigurationReport {
  uint16_t checked{0};
  uint16_t ok{0};
  uint16_t warnings{0};
  uint16_t errors{0};
  bool communicationOk{true};

  bool valid() const { return communicationOk && errors == 0; }
};

class DelixiCDIE100 {
 public:
  DelixiCDIE100(RS485Bus &bus, PrintController &logger,
                const irrigation::InverterProfile &profile);

  bool begin();
  bool ping();

  bool runForward();
  bool stop();
  bool freeStop();
  bool resetFault();

  bool setFrequencyHz(float hz);
  bool setFrequencyPercent(float percent);

  bool readOutputFrequency(float &hz);
  bool readReferenceFrequency(float &hz);
  bool readOutputCurrent(float &amps);
  bool readOutputVoltage(float &volts);
  bool readCommunicationSetPercent(float &percent);
  bool readRunState(DelixiRunState &state);
  bool readFaultCode(uint16_t &faultCode);

  bool readRegister(uint16_t address, uint16_t &value);
  bool readRegisters(uint16_t startAddress, uint16_t count,
                     uint16_t *values, size_t valueCapacity);
  bool writeRegister(uint16_t address, uint16_t value);

  bool readParameter(const char *parameterCode, uint16_t &value);
  bool writeParameter(const char *parameterCode, uint16_t value,
                      bool persistToEeprom = true);
  static bool parseParameterCode(const char *parameterCode,
                                 uint16_t &address,
                                 bool volatileRam = false);

  DelixiConfigurationReport checkConfiguration(
      const irrigation::MotorProfile &motor);
  bool applyConfiguration(const irrigation::MotorProfile &motor);

  Rs485Result rawTransaction(const uint8_t *request, size_t length,
                             bool appendCrc = true);
  const uint8_t *rawResponse() const { return bus_.rawData(); }
  size_t rawResponseLength() const { return bus_.rawLength(); }

  void setDebug(bool enabled) { debug_ = enabled; }
  bool debugEnabled() const { return debug_; }
  Rs485Status lastCommunicationStatus() const {
    return lastCommunicationStatus_;
  }
  uint8_t slaveAddress() const { return profile_.modbusAddress; }
  const irrigation::InverterProfile &profile() const { return profile_; }

  static const char *runStateName(DelixiRunState state);
  static const char *faultName(uint16_t faultCode);

 private:
  struct ExpectedParameter {
    const char *code;
    const char *name;
    uint16_t expected;
    DelixiConfigSeverity mismatchSeverity;
    bool apply;
  };

  RS485Bus &bus_;
  PrintController &logger_;
  const irrigation::InverterProfile &profile_;
  bool begun_;
  bool debug_;
  Rs485Status lastCommunicationStatus_;

  bool writeRunCommand(uint16_t command);
  bool readScaled(uint16_t address, float scale, float &value);
  Rs485Result executeWithRetries(const uint8_t *request, size_t length);
  size_t buildExpectedParameters(const irrigation::MotorProfile &motor,
                                 ExpectedParameter *parameters,
                                 size_t capacity) const;
  bool checkOne(const ExpectedParameter &parameter,
                DelixiConfigurationReport &report, bool printOk);
  void printConfigLine(DelixiConfigSeverity severity, const char *code,
                       const char *name, uint16_t actual,
                       uint16_t expected);
};

