#include "CommandProcessor.h"

#include <ProjectConfig.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

CommandProcessor::CommandProcessor(PumpController &pump, DelixiCDIE100 &vfd,
                                   PrintController &logger)
    : pump_(pump), vfd_(vfd), logger_(logger) {}

void CommandProcessor::processLine(const char *line) {
  if (line == nullptr) return;
  char working[MAX_LINE_LENGTH];
  strncpy(working, line, sizeof(working) - 1);
  working[sizeof(working) - 1] = '\0';

  char *argv[MAX_ARGUMENTS];
  int argc = 0;
  char *save = nullptr;
  for (char *token = strtok_r(working, " \t", &save);
       token != nullptr && argc < static_cast<int>(MAX_ARGUMENTS);
       token = strtok_r(nullptr, " \t", &save)) {
    argv[argc++] = token;
  }
  if (argc == 0) return;

  if (strcmp(argv[0], "help") == 0) {
    printHelp();
  } else if (strcmp(argv[0], "pump") == 0) {
    handlePump(argc, argv);
  } else if (strcmp(argv[0], "vfd") == 0) {
    handleVfd(argc, argv);
  } else if (strcmp(argv[0], "modbus") == 0) {
    handleModbus(argc, argv);
  } else {
    logger_.println(F("[CMD][ERROR] Unknown command. Type: help"), true);
  }
}

void CommandProcessor::printHelp() {
  logger_.println(F("Commands:"), true);
  logger_.println(F("  help"), true);
  logger_.println(F("  pump start | stop | estop | status | telemetry"), true);
  logger_.println(F("  pump freq <10.0..50.0>"), true);
  logger_.println(F("  pump fault | reset"), true);
  logger_.println(F("  vfd ping | info | config | config check"), true);
  logger_.println(F("  vfd config apply CONFIRM"), true);
  logger_.println(F("  vfd read <0x0000..0xFFFF>"), true);
  logger_.println(F("  vfd param read <P0.0.17>"), true);
  logger_.println(F("  modbus debug on | off"), true);
  logger_.println(F("  modbus raw <hex bytes without CRC>"), true);
  logger_.println(F("  modbus rawcrc <complete hex frame including CRC>"), true);
}

void CommandProcessor::handlePump(int argc, char *argv[]) {
  if (argc < 2) {
    logger_.println(F("[CMD][ERROR] Missing pump operation."), true);
    return;
  }
  if (strcmp(argv[1], "start") == 0) {
    logger_.println(pump_.start() ? F("[PUMP] Start command accepted.")
                                  : F("[PUMP][ERROR] Start failed."),
                    true);
  } else if (strcmp(argv[1], "stop") == 0) {
    logger_.println(pump_.stop() ? F("[PUMP] Deceleration stop accepted.")
                                 : F("[PUMP][ERROR] Stop failed."),
                    true);
  } else if (strcmp(argv[1], "estop") == 0) {
    logger_.println(pump_.emergencyStop()
                        ? F("[PUMP] Free-stop command accepted.")
                        : F("[PUMP][ERROR] Free stop failed."),
                    true);
  } else if (strcmp(argv[1], "freq") == 0 && argc == 3) {
    char *end = nullptr;
    errno = 0;
    const float hz = strtof(argv[2], &end);
    if (errno != 0 || end == argv[2] || *end != '\0') {
      logger_.println(F("[CMD][ERROR] Invalid frequency."), true);
      return;
    }
    if (pump_.setSpeedHz(hz)) {
      logger_.print(F("[PUMP] Frequency command: "), true);
      logger_.print(hz, true, "", 2);
      logger_.println(F(" Hz"), true);
    }
  } else if (strcmp(argv[1], "status") == 0) {
    printPumpStatus(false);
  } else if (strcmp(argv[1], "telemetry") == 0) {
    printPumpStatus(true);
  } else if (strcmp(argv[1], "fault") == 0) {
    uint16_t code = 0;
    if (vfd_.readFaultCode(code)) {
      logger_.print(F("[VFD] Fault "), true);
      logger_.print(static_cast<unsigned long>(code), true);
      logger_.print(F(": "), true);
      logger_.println(DelixiCDIE100::faultName(code), true);
    } else {
      logger_.println(F("[VFD][ERROR] Fault read failed."), true);
    }
  } else if (strcmp(argv[1], "reset") == 0) {
    logger_.println(pump_.resetFault() ? F("[VFD] Fault reset sent.")
                                      : F("[VFD][ERROR] Fault reset failed."),
                    true);
  } else {
    logger_.println(F("[CMD][ERROR] Invalid pump command."), true);
  }
}

void CommandProcessor::printPumpStatus(bool telemetry) {
  pump_.poll(true);
  const PumpStatus &status = pump_.status();
  logger_.print(F("[PUMP] State: "), true);
  logger_.println(DelixiCDIE100::runStateName(status.runState), true);
  logger_.print(F("[PUMP] Communication: "), true);
  logger_.println(status.communicationOk ? "OK" : "ERROR", true);
  logger_.print(F("[PUMP] Configuration: "), true);
  logger_.println(status.configurationValid ? "valid" : "blocked", true);
  logger_.print(F("[PUMP] Commanded frequency: "), true);
  logger_.print(status.commandedFrequencyHz, true, "", 2);
  logger_.println(F(" Hz"), true);
  logger_.print(F("[PUMP] Actual frequency: "), true);
  logger_.print(status.actualFrequencyHz, true, "", 2);
  logger_.println(F(" Hz"), true);
  logger_.print(F("[PUMP] Fault: "), true);
  logger_.print(static_cast<unsigned long>(status.vfdFaultCode), true);
  logger_.print(F(" - "), true);
  logger_.println(DelixiCDIE100::faultName(status.vfdFaultCode), true);
  if (!telemetry) return;
  logger_.print(F("[PUMP] Reference frequency: "), true);
  logger_.print(status.referenceFrequencyHz, true, "", 2);
  logger_.println(F(" Hz"), true);
  logger_.print(F("[PUMP] Motor current: "), true);
  logger_.print(status.motorCurrentA, true, "", 2);
  logger_.println(F(" A"), true);
  logger_.print(F("[PUMP] Output voltage: "), true);
  logger_.print(status.outputVoltageV, true, "", 1);
  logger_.println(F(" V"), true);
  logger_.print(F("[PUMP] Estimated motor speed: "), true);
  logger_.print(status.estimatedMotorRpm, true, "", 0);
  logger_.println(F(" rpm (frequency-based estimate)"), true);
  logger_.print(F("[PUMP] Uptime: "), true);
  logger_.print(static_cast<unsigned long>(status.uptimeSeconds), true);
  logger_.println(F(" s"), true);
}

void CommandProcessor::handleVfd(int argc, char *argv[]) {
  if (argc < 2) {
    logger_.println(F("[CMD][ERROR] Missing VFD operation."), true);
    return;
  }
  if (strcmp(argv[1], "ping") == 0) {
    vfd_.ping();
  } else if (strcmp(argv[1], "info") == 0) {
    const irrigation::InverterProfile &vfdProfile = vfd_.profile();
    const irrigation::MotorProfile &motor = pump_.motor();
    logger_.print(F("[VFD] "), true);
    logger_.print(vfdProfile.manufacturer, true);
    logger_.print(F(" "), true);
    logger_.println(vfdProfile.model, true);
    logger_.print(F("[VFD] 380 V, rated power/current: "), true);
    logger_.print(vfdProfile.ratedPowerKw, true, " kW, ", 1);
    logger_.print(vfdProfile.ratedOutputCurrentA, true, "", 1);
    logger_.println(F(" A"), true);
    logger_.print(F("[MOTOR] "), true);
    logger_.print(motor.manufacturer, true);
    logger_.print(F(" "), true);
    logger_.println(motor.model, true);
    logger_.print(F("[MOTOR] Rated current: "), true);
    logger_.print(motor.ratedCurrentA, true, "", 2);
    logger_.println(F(" A"), true);
  } else if (strcmp(argv[1], "config") == 0) {
    if (argc == 2 || (argc == 3 && strcmp(argv[2], "check") == 0)) {
      const DelixiConfigurationReport report =
          vfd_.checkConfiguration(pump_.motor());
      pump_.setConfigurationValid(report.valid());
    } else if (argc >= 3 && strcmp(argv[2], "apply") == 0) {
      if (argc != 4 || strcmp(argv[3], "CONFIRM") != 0) {
        logger_.println(
            F("[VFD] No settings changed. Use: vfd config apply CONFIRM"),
            true);
        return;
      }
      const bool applied = vfd_.applyConfiguration(pump_.motor());
      const DelixiConfigurationReport report =
          vfd_.checkConfiguration(pump_.motor());
      pump_.setConfigurationValid(applied && report.valid());
    } else {
      logger_.println(F("[CMD][ERROR] Invalid VFD config command."), true);
    }
  } else if (strcmp(argv[1], "read") == 0 && argc == 3) {
    uint16_t address = 0;
    uint16_t value = 0;
    if (!parseUInt16(argv[2], address) || !vfd_.readRegister(address, value)) {
      logger_.println(F("[VFD][ERROR] Register read failed."), true);
      return;
    }
    logger_.print(F("[VFD] Register 0x"), true);
    logger_.print(static_cast<unsigned long>(address), true, "", HEX);
    logger_.print(F(" = 0x"), true);
    logger_.print(static_cast<unsigned long>(value), true, "", HEX);
    logger_.print(F(" ("), true);
    logger_.print(static_cast<unsigned long>(value), true);
    logger_.println(F(")"), true);
  } else if (strcmp(argv[1], "param") == 0 && argc == 4 &&
             strcmp(argv[2], "read") == 0) {
    uint16_t value = 0;
    if (!vfd_.readParameter(argv[3], value)) {
      logger_.println(F("[VFD][ERROR] Parameter read failed."), true);
      return;
    }
    logger_.print(F("[VFD] "), true);
    logger_.print(argv[3], true);
    logger_.print(F(" = "), true);
    logger_.println(static_cast<unsigned long>(value), true);
  } else {
    logger_.println(F("[CMD][ERROR] Invalid VFD command."), true);
  }
}

void CommandProcessor::handleModbus(int argc, char *argv[]) {
  if (argc < 3) {
    logger_.println(F("[CMD][ERROR] Incomplete Modbus command."), true);
    return;
  }
  if (strcmp(argv[1], "debug") == 0) {
    if (strcmp(argv[2], "on") == 0) {
      vfd_.setDebug(true);
      logger_.println(F("[MODBUS] Debug enabled."), true);
    } else if (strcmp(argv[2], "off") == 0) {
      vfd_.setDebug(false);
      logger_.println(F("[MODBUS] Debug disabled."), true);
    } else {
      logger_.println(F("[CMD][ERROR] Use: modbus debug on|off"), true);
    }
    return;
  }

  const bool appendCrc = strcmp(argv[1], "raw") == 0;
  const bool fullFrame = strcmp(argv[1], "rawcrc") == 0;
  if (!appendCrc && !fullFrame) {
    logger_.println(F("[CMD][ERROR] Invalid Modbus command."), true);
    return;
  }

  bool confirmedWrite = false;
  int byteEnd = argc;
  if (strcmp(argv[argc - 1], "CONFIRM_WRITE") == 0) {
    confirmedWrite = true;
    --byteEnd;
  }
  uint8_t bytes[RS485Bus::TX_BUFFER_SIZE];
  size_t length = 0;
  for (int i = 2; i < byteEnd; ++i) {
    if (length >= sizeof(bytes) || !parseHexByte(argv[i], bytes[length])) {
      logger_.println(F("[CMD][ERROR] Raw bytes must be two-digit hex."),
                      true);
      return;
    }
    ++length;
  }
  const size_t minimum = fullFrame ? 4U : 2U;
  if (length < minimum) {
    logger_.println(F("[CMD][ERROR] Raw frame is too short."), true);
    return;
  }
  const bool writeFunction = bytes[1] != delixi::protocol::READ_HOLDING_REGISTERS;
  if (writeFunction) {
#if ENABLE_DANGEROUS_RAW_WRITES
    if (!confirmedWrite) {
      logger_.println(
          F("[MODBUS] Raw write blocked. Append CONFIRM_WRITE."), true);
      return;
    }
#else
    (void)confirmedWrite;
    logger_.println(
        F("[MODBUS] Raw writes are disabled at compile time. Validated VFD configuration apply remains available."),
        true);
    return;
#endif
  }
  const Rs485Result result = vfd_.rawTransaction(bytes, length, appendCrc);
  printRawResponse(result);
}

void CommandProcessor::printRawResponse(const Rs485Result &result) {
  logger_.print(F("[MODBUS] Status: "), true);
  logger_.println(RS485Bus::statusName(result.status), true);
  if (vfd_.rawResponseLength() == 0) return;
  logger_.print(F("[MODBUS] Response: "), true);
  const uint8_t *response = vfd_.rawResponse();
  for (size_t i = 0; i < vfd_.rawResponseLength(); ++i) {
    logger_.print(response[i], true, "", HEX);
    if (i + 1 < vfd_.rawResponseLength()) logger_.print(' ', true);
  }
  logger_.println(true);
}

bool CommandProcessor::parseUInt16(const char *text, uint16_t &value) {
  if (text == nullptr) return false;
  char *end = nullptr;
  errno = 0;
  const unsigned long parsed = strtoul(text, &end, 0);
  if (errno != 0 || end == text || *end != '\0' || parsed > 0xFFFFUL)
    return false;
  value = static_cast<uint16_t>(parsed);
  return true;
}

bool CommandProcessor::parseHexByte(const char *text, uint8_t &value) {
  if (text == nullptr || strlen(text) == 0 || strlen(text) > 2) return false;
  char *end = nullptr;
  errno = 0;
  const unsigned long parsed = strtoul(text, &end, 16);
  if (errno != 0 || end == text || *end != '\0' || parsed > 0xFFUL)
    return false;
  value = static_cast<uint8_t>(parsed);
  return true;
}

SerialCommandSource::SerialCommandSource(CommandProcessor &processor,
                                         PrintController &logger)
    : processor_(processor), logger_(logger), length_(0), overflow_(false) {
  memset(buffer_, 0, sizeof(buffer_));
}

void SerialCommandSource::poll(Stream &stream) {
  while (stream.available() > 0) {
    const int incoming = stream.read();
    if (incoming < 0) return;
    const char character = static_cast<char>(incoming);
    if (character == '\r') continue;
    if (character == '\n') {
      if (overflow_) {
        logger_.println(F("[CMD][ERROR] Command line is too long."), true);
      } else if (length_ > 0) {
        buffer_[length_] = '\0';
        processor_.processLine(buffer_);
      }
      length_ = 0;
      overflow_ = false;
      continue;
    }
    if (overflow_) continue;
    if (length_ + 1 >= BUFFER_SIZE) {
      overflow_ = true;
      continue;
    }
    buffer_[length_++] = character;
  }
}

