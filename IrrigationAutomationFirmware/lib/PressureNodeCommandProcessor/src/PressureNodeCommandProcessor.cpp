#include "PressureNodeCommandProcessor.h"

#include <SerialDebugMode.h>
#include <Tuf2000mProtocol.h>

#include <ctype.h>
#include <string.h>

namespace irrigation::pressure_node {

PressureNodeCommandProcessor::PressureNodeCommandProcessor(
    PressureControlNode &node, PrintController &logger)
    : node_(node), logger_(logger) {}

void PressureNodeCommandProcessor::processLine(const char *line) {
  if (line == nullptr) return;
  char command[MAX_LINE_LENGTH];
  strncpy(command, line, sizeof(command) - 1);
  command[sizeof(command) - 1] = '\0';

  char *start = command;
  while (*start != '\0' && isspace(static_cast<unsigned char>(*start))) {
    ++start;
  }
  char *end = start + strlen(start);
  while (end > start && isspace(static_cast<unsigned char>(end[-1]))) {
    *--end = '\0';
  }
  for (char *cursor = start; *cursor != '\0'; ++cursor) {
    *cursor = static_cast<char>(
        tolower(static_cast<unsigned char>(*cursor)));
  }
  if (*start == '\0') return;

  if (strcmp(start, "help") == 0 || strcmp(start, "?") == 0) {
    printHelp();
  } else if (strcmp(start, "debug simple") == 0) {
    irrigation::serial_debug::setMode(
        irrigation::serial_debug::Mode::Simple);
    logger_.println(F("[DEBUG] Simple serial diagnostics enabled."), true);
  } else if (strcmp(start, "debug full") == 0) {
    irrigation::serial_debug::setMode(irrigation::serial_debug::Mode::Full);
    logger_.println(
        F("[DEBUG] Full serial diagnostics enabled; Modbus frames will print."),
        true);
  } else if (strcmp(start, "debug status") == 0) {
    logger_.print(F("[DEBUG] Mode: "), true);
    logger_.println(irrigation::serial_debug::modeName(), true);
  } else if (strcmp(start, "status") == 0) {
    printStatus(node_.refreshStatus());
  } else if (strcmp(start, "battery") == 0) {
    printBattery(node_.readBattery());
  } else if (strcmp(start, "pressure") == 0) {
    printPressurePair(node_.readPressures());
  } else if (strcmp(start, "flow probe") == 0) {
    printFlowWordOrderProbe(node_.probeFlowWordOrder());
  } else if (strcmp(start, "flow total reset") == 0 ||
             strcmp(start, "flow reset") == 0) {
    handleFlowTotalReset();
  } else if (strcmp(start, "flow total") == 0) {
    printFlowTotals(node_.readFlowTotals());
  } else if (strcmp(start, "flow") == 0) {
    printFlow(node_.readFlow());
  } else if (strcmp(start, "pcv open") == 0) {
    handleValveCommand(PressureControlValveState::Open);
  } else if (strcmp(start, "pcv close") == 0) {
    handleValveCommand(PressureControlValveState::Closed);
  } else if (strcmp(start, "pcv state") == 0) {
    printValveStatus(node_.status().valve);
  } else {
    logger_.println(F("[CMD][ERROR] Unknown command. Type: help"), true);
  }
}

void PressureNodeCommandProcessor::printHelp() {
  logger_.println(F("Commands:"), true);
  logger_.println(F("  help"), true);
  logger_.println(F("  debug simple | full | status"), true);
  logger_.println(F("  status"), true);
  logger_.println(F("  battery"), true);
  logger_.println(F("  pressure"), true);
  logger_.println(F("  flow"), true);
  logger_.println(F("  flow total"), true);
  logger_.println(F("  flow total reset"), true);
  logger_.println(F("  flow probe"), true);
  logger_.println(F("  pcv open"), true);
  logger_.println(F("  pcv close"), true);
  logger_.println(F("  pcv state"), true);
}

void PressureNodeCommandProcessor::printBattery(
    const BatteryReading &reading) {
  logger_.print(F("[BATTERY] Status: "), true);
  logger_.println(readingStatusName(reading.status), true);
  if (reading.hasVoltage()) {
    logger_.print(F("[BATTERY] ADC pin: "), true);
    logger_.print(reading.adcVoltageV, true, "", 3);
    logger_.println(F(" V"), true);
    logger_.print(F("[BATTERY] Voltage: "), true);
    logger_.print(reading.voltageV, true, "", 3);
    logger_.println(F(" V"), true);
  }
  logger_.print(F("[BATTERY] SOC: "), true);
  logger_.println(batterySocStatusName(reading.socStatus), true);
}

void PressureNodeCommandProcessor::printPressure(
    const char *label, const PressureReading &reading) {
  logger_.print(F("[PRESSURE] "), true);
  logger_.print(label, true);
  logger_.print(F(": "), true);
  logger_.println(readingStatusName(reading.status), true);
  if (!reading.hasSample()) return;
  logger_.print(F("[PRESSURE] Value: "), true);
  logger_.print(reading.pressureBar, true, "", 3);
  logger_.print(F(" bar, temperature: "), true);
  logger_.print(reading.temperatureC, true, "", 2);
  logger_.print(F(" C, address: 0x"), true);
  logger_.println(reading.address, true, "", HEX);
}

void PressureNodeCommandProcessor::printPressurePair(
    const PressurePairReading &readings) {
  printPressure("UPSTREAM", readings.upstream);
  printPressure("DOWNSTREAM", readings.downstream);
  if (readings.upstream.hasSample() && readings.downstream.hasSample()) {
    logger_.print(F("[PRESSURE] Drop: "), true);
    logger_.print(readings.upstream.pressureBar -
                      readings.downstream.pressureBar,
                  true, "", 3);
    logger_.println(F(" bar (scale not yet validated)"), true);
  }
}

void PressureNodeCommandProcessor::printFlow(const FlowReading &reading) {
  logger_.print(F("[FLOW] Status: "), true);
  logger_.println(readingStatusName(reading.status), true);
  if (reading.diagnosticsAvailable) {
    printTuf2000mErrorBits(
        F("[FLOW] TUF-2000M: "), reading.deviceErrorBits,
        tuf2000m::protocol::flowSampleValid(reading.deviceErrorBits));
  }
  if (!reading.hasSample()) {
    if (reading.status == ReadingStatus::ConfigurationMissing) {
      logger_.println(
          F("[FLOW] TUF communication configuration is incomplete."),
          true);
      logger_.println(
          F("[FLOW] Check PressureNodeConfig.h and the RS-485 wiring."),
          true);
    }
    return;
  }
  logger_.print(F("[FLOW] Rate: "), true);
  logger_.print(reading.value, true, "", 3);
  logger_.print(' ', true);
  logger_.println(flowUnitName(reading.unit), true);
  if (reading.hasVelocity()) {
    logger_.print(F("[FLOW] Velocity: "), true);
    logger_.print(reading.velocityMetersPerSecond, true, "", 3);
    logger_.println(F(" m/s"), true);
  }
}

void PressureNodeCommandProcessor::printTuf2000mErrorBits(
    const __FlashStringHelper *prefix, uint16_t errorBits,
    bool flowSampleValid) {
  logger_.print(prefix, true);
  if (errorBits == 0) {
    logger_.print(F("system normal"), true);
  } else {
    bool first = true;
    for (uint8_t bit = 0; bit < 16; ++bit) {
      if ((errorBits & (static_cast<uint16_t>(1U) << bit)) == 0) continue;
      if (!first) logger_.print(F("; "), true);
      logger_.print(tuf2000m::protocol::errorBitMeaning(bit), true);
      first = false;
    }
  }
  logger_.print(F(" [0x"), true);
  if (errorBits < 0x1000U) logger_.print('0', true);
  if (errorBits < 0x0100U) logger_.print('0', true);
  if (errorBits < 0x0010U) logger_.print('0', true);
  logger_.print(errorBits, true, "", HEX);
  logger_.println(flowSampleValid ? F("] (flow sample valid)")
                                   : F("] (flow sample invalid)"),
                  true);
}

void PressureNodeCommandProcessor::printFlowTotals(
    const FlowTotalReading &reading) {
  logger_.print(F("[FLOW][TOTAL] Status: "), true);
  logger_.println(readingStatusName(reading.status), true);
  if (!reading.hasMeterTotals()) return;

  if (reading.hasSinceResetVolume()) {
    logger_.print(F("[FLOW][TOTAL] Since local total reset: "), true);
    logger_.print(reading.sinceResetCubicMeters, true, "", 6);
    logger_.print(F(" m3 ("), true);
    logger_.print(reading.sinceResetCubicMeters * 1000.0F, true, "", 3);
    logger_.println(F(" L)"), true);
  } else {
    logger_.println(
        F("[FLOW][TOTAL] Since reset: NOT_SET; run 'flow total reset'."),
        true);
  }

  logger_.print(F("[FLOW][TOTAL] Meter NET: "), true);
  logger_.print(reading.meterNetCubicMeters, true, "", 6);
  logger_.println(F(" m3"), true);
  logger_.print(F("[FLOW][TOTAL] Meter POSITIVE: "), true);
  logger_.print(reading.meterPositiveCubicMeters, true, "", 6);
  logger_.println(F(" m3"), true);
  logger_.print(F("[FLOW][TOTAL] Meter NEGATIVE: "), true);
  logger_.print(reading.meterNegativeCubicMeters, true, "", 6);
  logger_.println(F(" m3"), true);
}

void PressureNodeCommandProcessor::handleFlowTotalReset() {
  const FlowTotalResetResult result = node_.resetFlowTotal();
  if (!result.applied) {
    logger_.print(F("[FLOW][TOTAL][ERROR] Baseline reset failed: "), true);
    logger_.println(readingStatusName(result.totals.status), true);
    return;
  }
  logger_.println(
      F("[FLOW][TOTAL] ESP32 baseline reset to the current TUF positive total."),
      true);
  logger_.println(
      F("[FLOW][TOTAL] The TUF-2000M internal accumulators were not erased."),
      true);
  printFlowTotals(result.totals);
}

void PressureNodeCommandProcessor::printFlowWordOrderProbe(
    const FlowMeterWordOrderProbe &probe) {
  logger_.print(F("[FLOW][PROBE] Status: "), true);
  logger_.println(readingStatusName(probe.status), true);
  if (!probe.hasResponse()) return;

  logger_.print(F("[FLOW][PROBE] Raw REG0001-REG0006: "), true);
  for (size_t index = 0; index < FlowMeterWordOrderProbe::RAW_DATA_LENGTH;
       ++index) {
    logger_.print(probe.rawData[index], true, "", HEX);
    if (index + 1 < FlowMeterWordOrderProbe::RAW_DATA_LENGTH)
      logger_.print(' ', true);
  }
  logger_.println(true);

  logger_.print(F("[FLOW][PROBE] HIGH_WORD_FIRST: "), true);
  if (probe.highWordFirstDecoded) {
    logger_.print(F("flow="), true);
    logger_.print(probe.highWordFirstFlowRateM3PerHour, true, "", 6);
    logger_.print(F(" m3/h, velocity="), true);
    logger_.print(probe.highWordFirstVelocityMetersPerSecond, true, "", 6);
    logger_.println(F(" m/s"), true);
  } else {
    logger_.println(F("INVALID"), true);
  }

  logger_.print(F("[FLOW][PROBE] LOW_WORD_FIRST: "), true);
  if (probe.lowWordFirstDecoded) {
    logger_.print(F("flow="), true);
    logger_.print(probe.lowWordFirstFlowRateM3PerHour, true, "", 6);
    logger_.print(F(" m3/h, velocity="), true);
    logger_.print(probe.lowWordFirstVelocityMetersPerSecond, true, "", 6);
    logger_.println(F(" m/s"), true);
  } else {
    logger_.println(F("INVALID"), true);
  }

  if (probe.diagnosticsAvailable) {
    printTuf2000mErrorBits(F("[FLOW][PROBE] TUF-2000M: "),
                            probe.deviceErrorBits, probe.flowSampleValid);
  } else {
    logger_.println(
        F("[FLOW][PROBE] TUF-2000M error bits unavailable; this probe cannot "
          "confirm signal quality."),
        true);
  }
  if (probe.signalQualityAvailable) {
    logger_.print(F("[FLOW][PROBE] Signal quality Q: "), true);
    logger_.print(probe.signalQuality, true);
    logger_.print(F(" (0-99); transducer strength upstream/downstream: "), true);
    logger_.print(probe.upstreamSignalStrength, true);
    logger_.print(F("/"), true);
    logger_.print(probe.downstreamSignalStrength, true);
    logger_.println(F(" (0-2047)"), true);
  }

  // The verdict covers only the order this build actually decodes with, and
  // only when the decode above succeeded. Printing a fixed LOW_WORD_FIRST claim
  // would contradict an INVALID line and is wrong for any build whose order is
  // not validated yet.
  if (!probe.configuredOrderValidated) {
    logger_.println(
        F("[FLOW][PROBE] No word order is marked hardware-validated in this "
          "build; normal flow telemetry is not decoding REAL4 values."),
        true);
    return;
  }
  const bool configuredOrderDecoded = probe.configuredLowWordFirst
                                          ? probe.lowWordFirstDecoded
                                          : probe.highWordFirstDecoded;
  if (!configuredOrderDecoded) {
    logger_.println(
        F("[FLOW][PROBE] The configured word order did not decode on this "
          "reading; confirm the meter's REAL4 format before trusting "
          "telemetry."),
        true);
    return;
  }
  logger_.print(F("[FLOW][PROBE] The configured "), true);
  logger_.print(
      probe.configuredLowWordFirst ? F("LOW_WORD_FIRST") : F("HIGH_WORD_FIRST"),
      true);
  logger_.println(F(" decoder is hardware-verified for this meter."), true);
}

void PressureNodeCommandProcessor::printValveStatus(
    const ValveStatus &status) {
  logger_.print(F("[PCV] Last commanded: "), true);
  logger_.println(valveStateName(status.lastCommanded), true);
  logger_.print(F("[PCV] Verified/inferred: "), true);
  logger_.println(valveStateName(status.verifiedOrInferred), true);
  logger_.print(F("[PCV] Verification: "), true);
  logger_.println(verificationStateName(status.verification), true);
}

void PressureNodeCommandProcessor::printStatus(
    const PressureControlNodeStatus &status) {
  logger_.println(F("========== PRESSURE NODE STATUS =========="), true);
  printValveStatus(status.valve);
  printBattery(status.battery);
  printPressurePair({status.upstreamPressure, status.downstreamPressure});
  printFlow(status.flow);
  printFlowTotals(status.flowTotal);
  logger_.print(F("[POWER] Mode: "), true);
  logger_.println(powerModeName(status.powerMode), true);
  logger_.println(F("=========================================="), true);
}

void PressureNodeCommandProcessor::handleValveCommand(
    PressureControlValveState desiredState) {
  logger_.print(F("[PCV] Sending "), true);
  logger_.print(valveStateName(desiredState), true);
  logger_.println(F(" latching pulse."), true);
  const ValveCommandResult result = node_.commandValve(desiredState);
  logger_.print(F("[PCV] Actuation: "), true);
  logger_.println(valveActuationStatusName(result.actuation), true);
  printValveStatus(result.valve);
  if (result.ok()) printPressurePair(result.pressures);
}

SerialPressureNodeCommandSource::SerialPressureNodeCommandSource(
    PressureNodeCommandProcessor &processor, PrintController &logger)
    : processor_(processor), logger_(logger) {}

bool SerialPressureNodeCommandSource::poll(Stream &stream) {
  bool receivedInput = false;
  while (stream.available() > 0) {
    const int raw = stream.read();
    if (raw < 0) return receivedInput;
    receivedInput = true;
    const char character = static_cast<char>(raw);
    if (character == '\r' || character == '\n') {
      if (overflow_) {
        logger_.println(F("[CMD][ERROR] Command is too long."), true);
      } else if (length_ > 0) {
        buffer_[length_] = '\0';
        processor_.processLine(buffer_);
      }
      length_ = 0;
      overflow_ = false;
    } else if (!overflow_) {
      if (length_ + 1 < BUFFER_SIZE) {
        buffer_[length_++] = character;
      } else {
        overflow_ = true;
      }
    }
  }
  return receivedInput;
}

}  // namespace irrigation::pressure_node
