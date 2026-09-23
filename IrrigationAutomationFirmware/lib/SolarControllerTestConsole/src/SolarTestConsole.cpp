// This library is linked only by the solar test target, but it stays guarded so
// a build that pulls the header in cannot grow the console unintentionally.
#if PCV_SOLAR_TEST

#include "SolarTestConsole.h"

#include <stdlib.h>
#include <string.h>

namespace irrigation::pressure_node {

namespace protocol = epever_ls1024b::protocol;

namespace {

bool parseHexAddress(const char *text, uint8_t &address) {
  if (text == nullptr || *text == '\0') return false;
  char *end = nullptr;
  const unsigned long value = strtoul(text, &end, 0);
  if (end == text || *end != '\0' || value < 1 || value > 247) return false;
  address = static_cast<uint8_t>(value);
  return true;
}

bool parseRegisterAddress(const char *text, uint16_t &address) {
  if (text == nullptr || *text == '\0') return false;
  char *end = nullptr;
  const unsigned long value = strtoul(text, &end, 16);
  if (end == text || *end != '\0' || value > 0xFFFFUL) return false;
  address = static_cast<uint16_t>(value);
  return true;
}

}  // namespace

SolarTestConsole::SolarTestConsole(PressureNodeCommandProcessor &delegate,
                                   SolarControllerEpLs1024B &controller,
                                   PrintController &logger,
                                   protocol::SolarVoltageBlockProfile profile)
    : delegate_(delegate),
      controller_(controller),
      logger_(logger),
      profile_(profile) {}

bool SolarTestConsole::poll(Stream &stream) {
  bool receivedInput = false;
  while (stream.available() > 0) {
    const int value = stream.read();
    if (value < 0) break;
    receivedInput = true;
    const char character = static_cast<char>(value);
    if (character == '\r' || character == '\n') {
      if (overflow_) {
        logger_.println(
            F("[CMD][ERROR] Command is too long. Type: help"), true);
      } else if (length_ > 0) {
        buffer_[length_] = '\0';
        processLine(buffer_);
      }
      length_ = 0;
      overflow_ = false;
      continue;
    }
    if (length_ + 1 >= BUFFER_SIZE) {
      overflow_ = true;
      continue;
    }
    buffer_[length_++] = character;
  }
  return receivedInput;
}

void SolarTestConsole::processLine(char *line) {
  for (char *cursor = line; *cursor != '\0'; ++cursor)
    *cursor = static_cast<char>(tolower(static_cast<unsigned char>(*cursor)));

  char *argv[MAX_TOKENS] = {};
  int argc = 0;
  char *save = nullptr;
  for (char *token = strtok_r(line, " ", &save);
       token != nullptr && argc < static_cast<int>(MAX_TOKENS);
       token = strtok_r(nullptr, " ", &save)) {
    argv[argc++] = token;
  }
  if (argc == 0) return;

  if (strcmp(argv[0], "solar") != 0) {
    delegate_.processLine(line);
    return;
  }

  if (argc == 1 || (argc == 2 && strcmp(argv[1], "read") == 0)) {
    printMeasurements();
  } else if (strcmp(argv[1], "settings") == 0) {
    printSettings();
  } else if (strcmp(argv[1], "profile") == 0) {
    printProfile();
  } else if (strcmp(argv[1], "write") == 0) {
    handleWrite(argc, argv);
  } else if (strcmp(argv[1], "find") == 0) {
    handleFind();
  } else if (strcmp(argv[1], "address") == 0) {
    handleAddress(argc, argv);
  } else if (strcmp(argv[1], "reg") == 0) {
    handleRegisterRead(argc, argv, false);
  } else if (strcmp(argv[1], "hreg") == 0) {
    handleRegisterRead(argc, argv, true);
  } else if (strcmp(argv[1], "help") == 0 || strcmp(argv[1], "?") == 0) {
    printHelp();
  } else {
    logger_.println(F("[SOLAR][ERROR] Unknown solar command. Type: help"),
                    true);
  }
}

void SolarTestConsole::printHelp() {
  logger_.println(F("[SOLAR] Solar controller (EPEVER LS1024B) commands:"),
                  true);
  logger_.println(F("  solar                 live PV/battery/load measurements"),
                  true);
  logger_.println(F("  solar settings        current settings, read-only"),
                  true);
  logger_.println(F("  solar profile         the 12 setpoints a write would send"),
                  true);
  logger_.println(
      F("  solar write confirm   FC10 block write of 0x9003..0x900E"),
      true);
  logger_.println(F("  solar find            send the proprietary find-ID frame"),
                  true);
  logger_.println(
      F("  solar address <n> confirm   change the controller Modbus address"),
      true);
  logger_.println(F("  solar reg <hex>       raw FC04 input register"), true);
  logger_.println(F("  solar hreg <hex>      raw FC03 holding register"), true);
  logger_.print(F("[SOLAR] Charge-setting writes: "), true);
  logger_.println(SolarControllerEpLs1024B::writesCompiledIn()
                      ? F("compiled in for this build.")
                      : F("DISABLED in this build."),
                  true);
}

void SolarTestConsole::printMeasurements() {
  const SolarControllerMeasurements reading = controller_.readMeasurements();
  logger_.print(F("[SOLAR] Measurement status: "), true);
  logger_.println(readingStatusName(reading.status), true);
  if (!reading.hasSample()) {
    printExceptionHint();
    return;
  }

  logger_.print(F("[SOLAR] PV: "), true);
  logger_.print(reading.pvVoltageV, true, "", 2);
  logger_.print(F(" V, "), true);
  logger_.print(reading.pvCurrentA, true, "", 2);
  logger_.println(F(" A"), true);

  logger_.print(F("[SOLAR] Battery: "), true);
  logger_.print(reading.batteryVoltageV, true, "", 2);
  logger_.print(F(" V, charging current: "), true);
  logger_.print(reading.chargingCurrentA, true, "", 2);
  logger_.println(F(" A"), true);

  if (reading.loadAndTemperatureBlockAvailable) {
    logger_.print(F("[SOLAR] Load: "), true);
    logger_.print(reading.loadVoltageV, true, "", 2);
    logger_.print(F(" V, "), true);
    logger_.print(reading.loadCurrentA, true, "", 2);
    logger_.println(F(" A"), true);

    logger_.print(F("[SOLAR] Battery temperature: "), true);
    logger_.print(reading.batteryTemperatureC, true, "", 2);
    logger_.print(F(" C, device temperature: "), true);
    logger_.print(reading.deviceTemperatureC, true, "", 2);
    logger_.println(F(" C"), true);
  } else {
    logger_.println(
        F("[SOLAR] Load and temperature block (0x310C..0x3111): NOT_ANSWERED"),
        true);
  }

  if (reading.batterySocAvailable) {
    logger_.print(F("[SOLAR] Battery SOC: "), true);
    logger_.print(reading.batterySocPercent, true);
    logger_.println(F(" %"), true);
  } else {
    logger_.println(F("[SOLAR] Battery SOC: NOT_ANSWERED"), true);
  }

  if (reading.statusAvailable) {
    logger_.print(F("[SOLAR] Battery status: 0x"), true);
    logger_.print(reading.batteryStatusRaw, true, "", HEX);
    logger_.print(F(", charging status: 0x"), true);
    logger_.println(reading.chargingStatusRaw, true, "", HEX);
  } else {
    logger_.println(F("[SOLAR] Battery/charging status: NOT_ANSWERED"), true);
  }
}

void SolarTestConsole::printSettings() {
  const SolarControllerSettings settings = controller_.readSettings();
  logger_.print(F("[SOLAR] Settings status: "), true);
  logger_.println(readingStatusName(settings.status), true);
  if (!settings.hasSample()) {
    printExceptionHint();
    logger_.println(
        F("[SOLAR] An exception here is the likely answer for an unconfirmed "
          "settings map: 0x02 is illegal data address, 0x03 illegal data value."),
        true);
    return;
  }

  logger_.println(
      F("[SOLAR][WARNING] The 0x9000 settings map comes from the EPEVER "
        "Tracer-AN G3 protocol and captured PC-tool frames, not from an "
        "LS1024B document. Compare these values with the controller display "
        "before writing anything."),
      true);
  logger_.print(F("[SOLAR] Battery type: "), true);
  logger_.print(settings.batteryType, true);
  logger_.println(F(" (0=user, 1=AGM, 2=GEL, 3=flooded)"), true);
  logger_.print(F("[SOLAR] Battery capacity: "), true);
  logger_.print(settings.batteryCapacityAh, true);
  logger_.println(F(" Ah"), true);
  logger_.print(F("[SOLAR] Temperature compensation raw: 0x"), true);
  logger_.println(settings.temperatureCompensationRaw, true, "", HEX);

  logger_.print(F("[SOLAR] 0x9003 over-voltage disconnect: "), true);
  logger_.println(settings.overVoltageDisconnectV, true, " V", 2);
  logger_.print(F("[SOLAR] 0x9004 charging limit: "), true);
  logger_.println(settings.chargingLimitV, true, " V", 2);
  logger_.print(F("[SOLAR] 0x9005 over-voltage reconnect: "), true);
  logger_.println(settings.overVoltageReconnectV, true, " V", 2);
  logger_.print(F("[SOLAR] 0x9006 equalize charging: "), true);
  logger_.println(settings.equalizeChargingV, true, " V", 2);
  logger_.print(F("[SOLAR] 0x9007 boost charging: "), true);
  logger_.println(settings.boostChargingV, true, " V", 2);
  logger_.print(F("[SOLAR] 0x9008 float charging: "), true);
  logger_.println(settings.floatChargingV, true, " V", 2);
  logger_.print(F("[SOLAR] 0x9009 boost reconnect: "), true);
  logger_.println(settings.boostReconnectV, true, " V", 2);
  logger_.print(F("[SOLAR] 0x900A low-voltage reconnect: "), true);
  logger_.println(settings.lowVoltageReconnectV, true, " V", 2);
  logger_.print(F("[SOLAR] 0x900B under-voltage recover: "), true);
  logger_.println(settings.underVoltageRecoverV, true, " V", 2);
  if (settings.tailAvailable) {
    logger_.print(F("[SOLAR] 0x900C under-voltage warning: "), true);
    logger_.println(settings.underVoltageWarningV, true, " V", 2);
    logger_.print(F("[SOLAR] 0x900D low-voltage disconnect: "), true);
    logger_.println(settings.lowVoltageDisconnectV, true, " V", 2);
    logger_.print(F("[SOLAR] 0x900E discharging limit: "), true);
    logger_.println(settings.dischargingLimitV, true, " V", 2);
  } else {
    logger_.println(
        F("[SOLAR] 0x900C..0x900E (under-voltage warning, low-voltage "
          "disconnect, discharging limit): NOT_ANSWERED"),
        true);
  }

  if (settings.loadControlModeAvailable) {
    logger_.print(F("[SOLAR] 0x903D load control mode: 0x"), true);
    logger_.println(settings.loadControlModeRaw, true, "", HEX);
  } else {
    logger_.println(F("[SOLAR] 0x903D load control mode: NOT_ANSWERED"), true);
  }
  if (settings.ratedVoltageLevelAvailable) {
    logger_.print(F("[SOLAR] 0x9067 rated voltage level: "), true);
    logger_.print(settings.ratedVoltageLevelRaw, true);
    logger_.println(F(" (0=auto, 1=12V, 2=24V)"), true);
  } else {
    logger_.println(F("[SOLAR] 0x9067 rated voltage level: NOT_ANSWERED"),
                    true);
  }
  if (settings.maxChargingCurrentAvailable) {
    logger_.print(F("[SOLAR] 0x90BF max charging current: "), true);
    logger_.println(settings.maxChargingCurrentA, true, " A", 2);
  } else {
    logger_.println(F("[SOLAR] 0x90BF max charging current: NOT_ANSWERED"),
                    true);
  }
  logger_.println(
      F("[SOLAR] Use 'solar hreg <hex>' to print any register as a raw word."),
      true);
}

void SolarTestConsole::printProfile() {
  logger_.println(
      F("[SOLAR] Configured 0x9003..0x900E setpoints for a single 12 V 9 Ah "
        "VRLA battery. This block is written in one FC10 request:"),
      true);
  for (size_t i = 0; i < protocol::VOLTAGE_BLOCK_COUNT; ++i) {
    const protocol::VoltageBlockField &field = protocol::VOLTAGE_BLOCK_FIELDS[i];
    const uint16_t raw = profile_.values[i];
    logger_.print(F("[SOLAR] 0x"), true);
    logger_.print(field.reg, true, "", HEX);
    logger_.print(F(" "), true);
    logger_.print(field.label, true);
    logger_.print(F(": "), true);
    logger_.print(protocol::scaleBlockValue(i, raw), true, "", 2);
    logger_.print(F(" (raw "), true);
    logger_.print(raw, true);
    logger_.println(F(")"), true);
  }
  logger_.print(F("[SOLAR] Local ordering check: "), true);
  logger_.println(
      protocol::voltageBlockOrdered(profile_.values)
          ? F("PASSED")
          : F("FAILED. The controller would reject these setpoints."),
      true);
}

void SolarTestConsole::handleWrite(int argc, char **argv) {
  if (argc != 3 || strcmp(argv[2], "confirm") != 0) {
    logger_.println(
        F("[SOLAR] No settings changed. Use: solar write confirm"), true);
    return;
  }
  if (!SolarControllerEpLs1024B::writesCompiledIn()) {
    logger_.println(
        F("[SOLAR] Writes are disabled in this build. Rebuild with "
          "-D SOLAR_CONTROLLER_WRITES_ENABLED=1."),
        true);
    return;
  }

  logger_.println(
      F("[SOLAR] Writing 0x9003..0x900E as one FC10 block after checking the "
        "battery type and rated voltage; the block is read back before "
        "anything is reported as applied."),
      true);
  const SolarVoltageBlockWriteReport report =
      controller_.applyVoltageBlock(profile_);

  if (report.preconditionsChecked) {
    logger_.print(F("[SOLAR] Preconditions: battery type="), true);
    logger_.print(report.observedBatteryType, true);
    logger_.print(F(" (0=user), rated voltage level="), true);
    logger_.print(report.observedRatedVoltageLevel, true);
    logger_.println(F(" (1=12V)"), true);
  }
  if (report.status == SolarWriteStatus::PreconditionFailed) {
    logger_.println(
        F("[SOLAR] ABORTED before writing: this profile is a user-defined "
          "12 V battery profile and the controller is not configured that way. "
          "Set the battery type and system voltage on the controller first."),
        true);
    return;
  }
  if (report.status == SolarWriteStatus::SetpointsInvalid) {
    logger_.println(
        F("[SOLAR] ABORTED before writing: the configured setpoints are not "
          "mutually ordered. Run 'solar profile' to see the rule's verdict."),
        true);
    return;
  }
  if (!report.writeAttempted) {
    logger_.print(F("[SOLAR] No write was sent. Status: "), true);
    logger_.println(solarWriteStatusName(report.status), true);
    printExceptionHint();
    return;
  }

  for (size_t i = 0; i < protocol::VOLTAGE_BLOCK_COUNT; ++i) {
    const SolarBlockRegisterResult &result = report.results[i];
    const protocol::VoltageBlockField &field = protocol::VOLTAGE_BLOCK_FIELDS[i];
    logger_.print(F("[SOLAR] 0x"), true);
    logger_.print(field.reg, true, "", HEX);
    logger_.print(F(" "), true);
    logger_.print(field.label, true);
    logger_.print(F(": "), true);
    logger_.print(result.verified ? F("OK") : F("MISMATCH"), true);
    logger_.print(F(" requested="), true);
    logger_.print(result.requested, true);
    logger_.print(F(" read_back="), true);
    logger_.print(result.readBack, true);
    if (result.unchanged) logger_.print(F(" (already set)"), true);
    logger_.println(true);
  }

  logger_.print(F("[SOLAR] Block result: "), true);
  logger_.print(solarWriteStatusName(report.status), true);
  logger_.print(F(" ("), true);
  logger_.print(static_cast<unsigned long>(report.verified), true);
  logger_.print(F("/"), true);
  logger_.print(static_cast<unsigned long>(protocol::VOLTAGE_BLOCK_COUNT), true);
  logger_.println(F(" setpoints verified)"), true);
  if (report.status == SolarWriteStatus::VerificationMismatch ||
      report.status == SolarWriteStatus::ReadError) {
    logger_.println(
        F("[SOLAR] The controller did not report back what was sent. Treat the "
          "stored settings as uncertain and check the controller display."),
        true);
  }
  printExceptionHint();
  logger_.println(
      F("[SOLAR] Run 'solar settings' to read the block back independently."),
      true);
}

void SolarTestConsole::printExceptionHint() {
  const uint8_t code = controller_.lastExceptionCode();
  if (code == 0) return;
  logger_.print(F("[SOLAR] Modbus exception: 0x"), true);
  logger_.println(code, true, "", HEX);
}

void SolarTestConsole::printHexFrame(const __FlashStringHelper *label,
                                     const uint8_t *data, size_t length) {
  logger_.print(label, true);
  if (length == 0) {
    logger_.println(F("<no bytes>"), true);
    return;
  }
  for (size_t i = 0; i < length; ++i) {
    logger_.print(data[i], true, "", HEX);
    if (i + 1 < length) logger_.print(' ', true);
  }
  logger_.println(true);
}

void SolarTestConsole::handleFind() {
  const SolarServiceExchange exchange = controller_.sendServiceFindId();
  logger_.println(
      F("[SOLAR] Proprietary service command 0x45, broadcast address 0xF8. "
        "This is not Modbus; the response format is undocumented and is "
        "printed, not parsed."),
      true);
  if (!exchange.frameSent) {
    logger_.println(F("[SOLAR] Find frame not sent."), true);
    return;
  }
  logger_.print(F("[SOLAR] Find frame sent; response: "), true);
  logger_.println(exchange.responseReceived ? F("RECEIVED") : F("NONE"), true);
  printHexFrame(F("[SOLAR] RX: "), exchange.response, exchange.responseLength);
  if (exchange.responseTruncated)
    logger_.println(F("[SOLAR] RX was truncated for display."), true);
  logger_.println(
      F("[SOLAR] Find is informational: use 'solar address <n> confirm' to "
        "change the address."),
      true);
}

void SolarTestConsole::handleAddress(int argc, char **argv) {
  if (argc != 4 || strcmp(argv[3], "confirm") != 0) {
    logger_.println(
        F("[SOLAR] No address changed. Use: solar address <1..247> confirm"),
        true);
    return;
  }
  uint8_t newAddress = 0;
  if (!parseHexAddress(argv[2], newAddress)) {
    logger_.println(
        F("[SOLAR] Address must be 1..247, decimal or 0x-prefixed."), true);
    return;
  }
  if (!SolarControllerEpLs1024B::writesCompiledIn()) {
    logger_.println(
        F("[SOLAR] Writes are disabled in this build. Rebuild with "
          "-D SOLAR_CONTROLLER_WRITES_ENABLED=1."),
        true);
    return;
  }
  if (newAddress == controller_.slaveAddress()) {
    logger_.println(
        F("[SOLAR] The new address equals the configured address."), true);
    return;
  }

  logger_.println(
      F("[SOLAR] The service command is a broadcast: exactly one LS1024B may "
        "be connected to this RS-485 trunk."),
      true);
  const SolarAddressChangeReport report = controller_.changeAddress(newAddress);
  if (!report.requestAccepted) {
    logger_.println(
        F("[SOLAR] Address change not attempted: check the address range and "
          "that the RS-485 transport is up."),
        true);
    return;
  }

  logger_.print(F("[SOLAR] Find frame before the change: "), true);
  logger_.println(report.find.responseReceived ? F("RESPONSE") : F("NO_RESPONSE"),
                  true);
  logger_.print(F("[SOLAR] Set-ID frame sent; response: "), true);
  logger_.println(
      report.setId.responseReceived ? F("RESPONSE") : F("NO_RESPONSE"), true);
  if (report.settled)
    logger_.println(F("[SOLAR] Settle delay completed before verification."),
                    true);

  logger_.print(F("[SOLAR] New address 0x"), true);
  logger_.print(report.newAddress, true, "", HEX);
  logger_.print(F(" probe: "), true);
  logger_.println(readingStatusName(report.newAddressProbe), true);
  logger_.print(F("[SOLAR] Previous address 0x"), true);
  logger_.print(report.previousAddress, true, "", HEX);
  logger_.print(F(" probe: "), true);
  logger_.println(readingStatusName(report.previousAddressProbe), true);

  if (report.confirmed()) {
    logger_.println(
        F("[SOLAR] SUCCESS: the new address answers and the previous address "
          "does not."),
        true);
    logger_.println(
        F("[SOLAR] Update SLAVE_ADDRESS in "
          "examples/PressureControlNode/include/PressureNodeConfig.h and "
          "rebuild before the next session."),
        true);
  } else if (report.ambiguous()) {
    logger_.println(
        F("[SOLAR] DANGEROUS/PARTIAL: both addresses answer. More than one "
          "controller is probably on the trunk; verify the wiring."),
        true);
  } else {
    logger_.println(
        F("[SOLAR] FAILED/PARTIAL: the new address did not answer. Re-run "
          "'solar' against the configured address to see what still responds."),
        true);
  }
}

void SolarTestConsole::handleRegisterRead(int argc, char **argv, bool holding) {
  if (argc != 3) {
    logger_.println(holding ? F("[SOLAR] Use: solar hreg <hex register>")
                            : F("[SOLAR] Use: solar reg <hex register>"),
                    true);
    return;
  }
  uint16_t address = 0;
  if (!parseRegisterAddress(argv[2], address)) {
    logger_.println(F("[SOLAR] Register must be hexadecimal, e.g. 0x3104."),
                    true);
    return;
  }

  uint16_t raw = 0;
  const uint8_t function = holding ? protocol::READ_HOLDING_REGISTERS
                                   : protocol::READ_INPUT_REGISTERS;
  if (!controller_.readRegister(function, address, raw)) {
    logger_.print(F("[SOLAR] Read failed for 0x"), true);
    logger_.println(address, true, "", HEX);
    printExceptionHint();
    return;
  }
  logger_.print(F("[SOLAR] 0x"), true);
  logger_.print(address, true, "", HEX);
  logger_.print(F(" = 0x"), true);
  logger_.print(raw, true, "", HEX);
  logger_.print(F(" ("), true);
  logger_.print(raw, true);
  logger_.println(F(")"), true);
}

}  // namespace irrigation::pressure_node

#endif  // PCV_SOLAR_TEST
