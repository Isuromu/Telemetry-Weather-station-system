#include "DelixiCDIE100.h"

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

namespace {

uint16_t toRaw(float value, float unitsPerCount) {
  return static_cast<uint16_t>(lroundf(value / unitsPerCount));
}

uint16_t delixiDataFormat(irrigation::SerialFrame frame) {
  switch (frame) {
    case irrigation::SerialFrame::EightN2:
      return 0;
    case irrigation::SerialFrame::EightE1:
      return 1;
    case irrigation::SerialFrame::EightO1:
      return 2;
    case irrigation::SerialFrame::EightN1:
    default:
      return 3;
  }
}

}  // namespace

DelixiCDIE100::DelixiCDIE100(
    RS485Bus &bus, PrintController &logger,
    const irrigation::InverterProfile &profile)
    : bus_(bus),
      logger_(logger),
      profile_(profile),
      begun_(false),
      debug_(false),
      lastCommunicationStatus_(Rs485Status::NotInitialized) {}

bool DelixiCDIE100::begin() {
  begun_ = bus_.initialized();
  if (!begun_) logger_.println(F("[VFD][ERROR] RS-485 is not initialized."), true);
  return begun_;
}

bool DelixiCDIE100::ping() {
  uint16_t state = 0;
  const bool success = readRegister(delixi::protocol::RUN_STATE, state);
  logger_.println(success ? F("[VFD] Communication OK")
                          : F("[VFD][ERROR] Communication failed"),
                  true);
  return success;
}

bool DelixiCDIE100::writeRunCommand(uint16_t command) {
  return writeRegister(delixi::protocol::RUN_COMMAND, command);
}

bool DelixiCDIE100::runForward() {
  return writeRunCommand(delixi::protocol::COMMAND_FORWARD_RUN);
}

bool DelixiCDIE100::stop() {
  return writeRunCommand(delixi::protocol::COMMAND_DECELERATION_STOP);
}

bool DelixiCDIE100::freeStop() {
  return writeRunCommand(delixi::protocol::COMMAND_FREE_STOP);
}

bool DelixiCDIE100::resetFault() {
  return writeRunCommand(delixi::protocol::COMMAND_FAULT_RESET);
}

bool DelixiCDIE100::setFrequencyHz(float hz) {
  if (hz < 0.0f || hz > profile_.maximumFrequencyHz) return false;
  const uint16_t raw = delixi::protocol::frequencyHzToRawPercent(
      hz, profile_.maximumFrequencyHz);
  return writeRegister(delixi::protocol::FREQUENCY_COMMAND, raw);
}

bool DelixiCDIE100::setFrequencyPercent(float percent) {
  if (percent < 0.0f || percent > 100.0f) return false;
  return writeRegister(delixi::protocol::FREQUENCY_COMMAND,
                       delixi::protocol::frequencyPercentToRaw(percent));
}

bool DelixiCDIE100::readScaled(uint16_t address, float scale, float &value) {
  uint16_t raw = 0;
  if (!readRegister(address, raw)) return false;
  value = static_cast<float>(raw) * scale;
  return true;
}

bool DelixiCDIE100::readOutputFrequency(float &hz) {
  return readScaled(delixi::protocol::MONITOR_OUTPUT_FREQUENCY, 0.01f, hz);
}

bool DelixiCDIE100::readReferenceFrequency(float &hz) {
  return readScaled(delixi::protocol::MONITOR_REFERENCE_FREQUENCY, 0.01f,
                    hz);
}

bool DelixiCDIE100::readOutputCurrent(float &amps) {
  return readScaled(delixi::protocol::MONITOR_OUTPUT_CURRENT, 0.01f, amps);
}

bool DelixiCDIE100::readOutputVoltage(float &volts) {
  return readScaled(delixi::protocol::MONITOR_OUTPUT_VOLTAGE, 1.0f, volts);
}

bool DelixiCDIE100::readCommunicationSetPercent(float &percent) {
  return readScaled(delixi::protocol::MONITOR_COMMUNICATION_SET_VALUE, 0.01f,
                    percent);
}

bool DelixiCDIE100::readRunState(DelixiRunState &state) {
  uint16_t raw = 0;
  if (!readRegister(delixi::protocol::RUN_STATE, raw)) {
    state = DelixiRunState::Unknown;
    return false;
  }
  state = raw <= 3 ? static_cast<DelixiRunState>(raw)
                   : DelixiRunState::Unknown;
  return true;
}

bool DelixiCDIE100::readFaultCode(uint16_t &faultCode) {
  return readRegister(delixi::protocol::FAULT_CODE, faultCode);
}

Rs485Result DelixiCDIE100::executeWithRetries(const uint8_t *request,
                                              size_t length) {
  Rs485Result result;
  const uint8_t retries = profile_.requestRetries == 0 ? 1 : profile_.requestRetries;
  for (uint8_t attempt = 0; attempt < retries; ++attempt) {
    result = bus_.transact(request, length, profile_.requestTimeoutMs, debug_);
    lastCommunicationStatus_ = result.status;
    if (result.ok()) return result;
    if (attempt + 1 < retries) delay(25U * (attempt + 1U));
  }
  return result;
}

bool DelixiCDIE100::readRegister(uint16_t address, uint16_t &value) {
  return readRegisters(address, 1, &value, 1);
}

bool DelixiCDIE100::readRegisters(uint16_t startAddress, uint16_t count,
                                  uint16_t *values, size_t valueCapacity) {
  if (!begun_ || values == nullptr || count == 0 || valueCapacity < count ||
      count > 120)
    return false;
  const uint8_t request[] = {
      profile_.modbusAddress,
      delixi::protocol::READ_HOLDING_REGISTERS,
      static_cast<uint8_t>(startAddress >> 8U),
      static_cast<uint8_t>(startAddress & 0xFFU),
      static_cast<uint8_t>(count >> 8U),
      static_cast<uint8_t>(count & 0xFFU)};
  const Rs485Result result = executeWithRetries(request, sizeof(request));
  if (!result.ok()) return false;
  const uint8_t *response = bus_.rawData();
  const size_t responseLength = bus_.rawLength();
  const size_t expectedDataBytes = static_cast<size_t>(count) * 2U;
  if (responseLength != expectedDataBytes + 5U ||
      response[2] != expectedDataBytes)
    return false;
  for (uint16_t i = 0; i < count; ++i) {
    values[i] = (static_cast<uint16_t>(response[3U + i * 2U]) << 8U) |
                response[4U + i * 2U];
  }
  return true;
}

bool DelixiCDIE100::writeRegister(uint16_t address, uint16_t value) {
  if (!begun_) return false;
  const uint8_t request[] = {
      profile_.modbusAddress,
      delixi::protocol::WRITE_SINGLE_REGISTER,
      static_cast<uint8_t>(address >> 8U),
      static_cast<uint8_t>(address & 0xFFU),
      static_cast<uint8_t>(value >> 8U),
      static_cast<uint8_t>(value & 0xFFU)};
  const Rs485Result result = executeWithRetries(request, sizeof(request));
  if (!result.ok() || bus_.rawLength() != 8) return false;
  const uint8_t *response = bus_.rawData();
  return memcmp(request, response, sizeof(request)) == 0;
}

bool DelixiCDIE100::parseParameterCode(const char *parameterCode,
                                      uint16_t &address, bool volatileRam) {
  if (parameterCode == nullptr ||
      (parameterCode[0] != 'P' && parameterCode[0] != 'p'))
    return false;
  char *end = nullptr;
  const unsigned long group = strtoul(parameterCode + 1, &end, 10);
  if (end == parameterCode + 1 || *end != '.' || group > 9) return false;
  const char *levelStart = end + 1;
  const unsigned long level = strtoul(levelStart, &end, 10);
  if (end == levelStart || *end != '.' || level > 9) return false;
  const char *indexStart = end + 1;
  const unsigned long index = strtoul(indexStart, &end, 10);
  if (end == indexStart || *end != '\0' || index > 255) return false;
  if (volatileRam && group == 9) return false;
  address = delixi::protocol::parameterAddress(
      static_cast<uint8_t>(group), static_cast<uint8_t>(level),
      static_cast<uint8_t>(index), volatileRam);
  return true;
}

bool DelixiCDIE100::readParameter(const char *parameterCode,
                                  uint16_t &value) {
  uint16_t address = 0;
  return parseParameterCode(parameterCode, address, false) &&
         readRegister(address, value);
}

bool DelixiCDIE100::writeParameter(const char *parameterCode, uint16_t value,
                                   bool persistToEeprom) {
  if (parameterCode == nullptr ||
      (parameterCode[1] == '9' && parameterCode[2] == '.'))
    return false;
  uint16_t address = 0;
  return parseParameterCode(parameterCode, address, !persistToEeprom) &&
         writeRegister(address, value);
}

size_t DelixiCDIE100::buildExpectedParameters(
    const irrigation::MotorProfile &motor, ExpectedParameter *parameters,
    size_t capacity) const {
  if (parameters == nullptr || capacity < 19) return 0;
  size_t i = 0;
  parameters[i++] = {"P0.0.02", "V/F control mode", 0,
                     DelixiConfigSeverity::Error, true};
  parameters[i++] = {"P0.0.03", "communication operation control", 2,
                     DelixiConfigSeverity::Warning, true};
  parameters[i++] = {"P0.0.04", "communication frequency source", 9,
                     DelixiConfigSeverity::Warning, true};
  parameters[i++] = {"P0.0.07", "maximum frequency",
                     toRaw(profile_.maximumFrequencyHz, 0.01f),
                     DelixiConfigSeverity::Error, true};
  parameters[i++] = {"P0.0.08", "upper frequency",
                     toRaw(profile_.upperFrequencyHz, 0.01f),
                     DelixiConfigSeverity::Error, true};
  parameters[i++] = {"P0.0.11", "acceleration time",
                     toRaw(profile_.accelerationSeconds, 0.1f),
                     DelixiConfigSeverity::Warning, true};
  parameters[i++] = {"P0.0.12", "deceleration time",
                     toRaw(profile_.decelerationSeconds, 0.1f),
                     DelixiConfigSeverity::Warning, true};
  parameters[i++] = {"P0.0.13", "asynchronous common motor", 0,
                     DelixiConfigSeverity::Error, true};
  parameters[i++] = {"P0.0.14", "motor rated power",
                     toRaw(motor.ratedPowerKw, 0.1f),
                     DelixiConfigSeverity::Error, true};
  parameters[i++] = {"P0.0.15", "motor rated frequency",
                     toRaw(motor.ratedFrequencyHz, 0.01f),
                     DelixiConfigSeverity::Error, true};
  parameters[i++] = {"P0.0.16", "motor rated voltage", motor.ratedVoltageV,
                     DelixiConfigSeverity::Error, true};
  parameters[i++] = {"P0.0.17", "motor rated current",
                     toRaw(motor.ratedCurrentA, 0.01f),
                     DelixiConfigSeverity::Error, true};
  parameters[i++] = {"P0.0.18", "motor rated speed", motor.ratedRpm,
                     DelixiConfigSeverity::Error, true};
  parameters[i++] = {"P0.0.24", "parameter identification idle", 0,
                     DelixiConfigSeverity::Error, false};
  parameters[i++] = {"P1.0.00", "straight-line V/F curve", 0,
                     DelixiConfigSeverity::Warning, true};
  parameters[i++] = {"P4.1.00", "9600 baud", 3,
                     DelixiConfigSeverity::Error, true};
  parameters[i++] = {"P4.1.01", "serial data format",
                     delixiDataFormat(profile_.modbusFrame),
                     DelixiConfigSeverity::Error, true};
  parameters[i++] = {"P4.1.02", "Modbus address", profile_.modbusAddress,
                     DelixiConfigSeverity::Error, true};
  parameters[i++] = {"P4.1.03", "response delay", profile_.responseDelayMs,
                     DelixiConfigSeverity::Warning, true};
  if (capacity >= 22) {
    parameters[i++] = {"P4.1.04", "communication timeout",
                       toRaw(profile_.communicationTimeoutSeconds, 0.1f),
                       DelixiConfigSeverity::Warning,
                       profile_.communicationTimeoutSeconds > 0.0f};
    parameters[i++] = {"P4.1.05", "Modbus RTU", 1,
                       DelixiConfigSeverity::Error, true};
    parameters[i++] = {"P4.1.06", "Modbus replies enabled", 0,
                       DelixiConfigSeverity::Error, true};
  }
  return i;
}

void DelixiCDIE100::printConfigLine(DelixiConfigSeverity severity,
                                    const char *code, const char *name,
                                    uint16_t actual, uint16_t expected) {
  logger_.print(F("[VFD]["), true);
  logger_.print(severity == DelixiConfigSeverity::Ok
                    ? "OK"
                    : (severity == DelixiConfigSeverity::Warning ? "WARN"
                                                                 : "ERROR"),
                true);
  logger_.print(F("] "), true);
  logger_.print(code, true);
  logger_.print(F(" "), true);
  logger_.print(name, true);
  logger_.print(F(": actual="), true);
  logger_.print(static_cast<unsigned long>(actual), true);
  logger_.print(F(", expected="), true);
  logger_.println(static_cast<unsigned long>(expected), true);
}

bool DelixiCDIE100::checkOne(const ExpectedParameter &parameter,
                             DelixiConfigurationReport &report,
                             bool printOk) {
  ++report.checked;
  uint16_t actual = 0;
  if (!readParameter(parameter.code, actual)) {
    ++report.errors;
    report.communicationOk = false;
    logger_.print(F("[VFD][ERROR] Cannot read "), true);
    logger_.println(parameter.code, true);
    return false;
  }
  DelixiConfigSeverity severity = DelixiConfigSeverity::Ok;
  if (strcmp(parameter.code, "P4.1.04") == 0) {
    if (actual == 0) {
      severity = DelixiConfigSeverity::Warning;
    } else if (parameter.expected == 0) {
      severity = DelixiConfigSeverity::Ok;
    } else if (actual != parameter.expected) {
      severity = parameter.mismatchSeverity;
    }
  } else if (actual != parameter.expected) {
    severity = parameter.mismatchSeverity;
  }
  if (severity == DelixiConfigSeverity::Ok) {
    ++report.ok;
    if (printOk) printConfigLine(severity, parameter.code, parameter.name,
                                 actual, parameter.expected);
  } else if (severity == DelixiConfigSeverity::Warning) {
    ++report.warnings;
    printConfigLine(severity, parameter.code, parameter.name, actual,
                    parameter.expected);
  } else {
    ++report.errors;
    printConfigLine(severity, parameter.code, parameter.name, actual,
                    parameter.expected);
  }
  return true;
}

DelixiConfigurationReport DelixiCDIE100::checkConfiguration(
    const irrigation::MotorProfile &motor) {
  DelixiConfigurationReport report;
  ExpectedParameter parameters[22];
  const size_t count =
      buildExpectedParameters(motor, parameters, sizeof(parameters) / sizeof(parameters[0]));
  logger_.println(F("[VFD] Checking configuration (read-only)..."), true);
  for (size_t i = 0; i < count; ++i) checkOne(parameters[i], report, false);
  logger_.print(F("[VFD] Configuration summary: ok="), true);
  logger_.print(static_cast<unsigned long>(report.ok), true);
  logger_.print(F(", warnings="), true);
  logger_.print(static_cast<unsigned long>(report.warnings), true);
  logger_.print(F(", errors="), true);
  logger_.println(static_cast<unsigned long>(report.errors), true);
  if (profile_.communicationTimeoutSeconds <= 0.0f)
    logger_.println(
        F("[VFD][WARN] Communication timeout is disabled for bring-up; enable it for deployment."),
        true);
  return report;
}

bool DelixiCDIE100::applyConfiguration(
    const irrigation::MotorProfile &motor) {
  DelixiRunState state = DelixiRunState::Unknown;
  if (!readRunState(state) || state != DelixiRunState::Stopped) {
    logger_.println(F("[VFD][ERROR] Configuration apply requires a stopped VFD."),
                    true);
    return false;
  }
  ExpectedParameter parameters[22];
  const size_t count =
      buildExpectedParameters(motor, parameters, sizeof(parameters) / sizeof(parameters[0]));
  logger_.println(F("[VFD] Applying explicitly confirmed configuration..."), true);
  bool success = true;
  for (size_t i = 0; i < count; ++i) {
    if (!parameters[i].apply) continue;
    uint16_t actual = 0;
    if (!readParameter(parameters[i].code, actual)) {
      success = false;
      continue;
    }
    if (actual == parameters[i].expected) continue;
    logger_.print(F("[VFD] Write "), true);
    logger_.print(parameters[i].code, true);
    logger_.print(F(": "), true);
    logger_.print(static_cast<unsigned long>(actual), true);
    logger_.print(F(" -> "), true);
    logger_.println(static_cast<unsigned long>(parameters[i].expected), true);
    if (!writeParameter(parameters[i].code, parameters[i].expected, true)) {
      logger_.println(F("[VFD][ERROR] Parameter write failed."), true);
      success = false;
    }
  }
  return success;
}

Rs485Result DelixiCDIE100::rawTransaction(const uint8_t *request,
                                          size_t length, bool appendCrc) {
  const Rs485Result result = bus_.rawTransaction(
      request, length, appendCrc, profile_.requestTimeoutMs, true);
  lastCommunicationStatus_ = result.status;
  return result;
}

const char *DelixiCDIE100::runStateName(DelixiRunState state) {
  switch (state) {
    case DelixiRunState::Forward:
      return "forward";
    case DelixiRunState::Reverse:
      return "reverse";
    case DelixiRunState::Stopped:
      return "stopped";
    default:
      return "unknown";
  }
}

const char *DelixiCDIE100::faultName(uint16_t code) {
  static const char *const names[] = {
      "No fault", "Over-current at constant speed",
      "Over-current at acceleration", "Over-current at deceleration",
      "Over-voltage at constant speed", "Over-voltage at acceleration",
      "Over-voltage at deceleration", "Module fault", "Undervoltage",
      "VFD overload", "Motor overload", "Input phase loss",
      "Output phase loss", "External fault", "Abnormal communication",
      "VFD overheat", "VFD hardware fault", "Motor earth short circuit",
      "Motor identification error", "Motor off-load", "PID feedback loss",
      "User-defined fault 1", "User-defined fault 2",
      "Power-on time reached", "Running time reached", "Encoder fault",
      "Parameter read/write abnormality", "Motor overheat",
      "Large speed deviation", "Motor overspeed", "Initial position error",
      "Current test fault", "Contactor fault", "Abnormal current test",
      "Fast current-limiting timeout", "Motor switch at running",
      "24V power fault", "Drive power supply fault"};
  if (code < sizeof(names) / sizeof(names[0])) return names[code];
  if (code == 40) return "Buffer resistance fault";
  return "Unknown fault";
}
