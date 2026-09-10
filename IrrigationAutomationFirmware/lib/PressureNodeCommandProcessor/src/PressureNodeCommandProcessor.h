#pragma once

#include <Arduino.h>
#include <PressureControlNode.h>
#include <PrintController.h>

namespace irrigation::pressure_node {

class PressureNodeCommandProcessor {
 public:
  PressureNodeCommandProcessor(PressureControlNode &node,
                               PrintController &logger);

  void processLine(const char *line);
  void printHelp();

 private:
  static constexpr size_t MAX_LINE_LENGTH = 96;

  PressureControlNode &node_;
  PrintController &logger_;

  void printStatus(const PressureControlNodeStatus &status);
  void printBattery(const BatteryReading &reading);
  void printPressure(const char *label, const PressureReading &reading);
  void printPressurePair(const PressurePairReading &readings);
  void printFlow(const FlowReading &reading);
  void printFlowTotals(const FlowTotalReading &reading);
  void handleFlowTotalReset();
  void printFlowWordOrderProbe(const FlowMeterWordOrderProbe &probe);
  void printValveStatus(const ValveStatus &status);
  void handleValveCommand(PressureControlValveState desiredState);
};

class SerialPressureNodeCommandSource {
 public:
  SerialPressureNodeCommandSource(PressureNodeCommandProcessor &processor,
                                  PrintController &logger);
  bool poll(Stream &stream);

 private:
  static constexpr size_t BUFFER_SIZE = 96;
  PressureNodeCommandProcessor &processor_;
  PrintController &logger_;
  char buffer_[BUFFER_SIZE]{};
  size_t length_{0};
  bool overflow_{false};
};

}  // namespace irrigation::pressure_node
