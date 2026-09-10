#include "PressureNodeCommandProcessor.h"

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
    logger_.print(F("[FLOW] TUF-2000M error bits: 0x"), true);
    logger_.println(reading.deviceErrorBits, true, "", HEX);
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
  logger_.println(
      F("[FLOW][PROBE] LOW_WORD_FIRST is hardware-verified for this meter."),
      true);
  logger_.println(
      F("[FLOW][PROBE] Normal telemetry uses the configured LOW_WORD_FIRST decoder."),
      true);
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
