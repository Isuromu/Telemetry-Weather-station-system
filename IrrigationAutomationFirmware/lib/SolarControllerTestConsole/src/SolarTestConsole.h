#pragma once

#include <Arduino.h>
#include <PressureNodeCommandProcessor.h>
#include <PrintController.h>
#include <SolarControllerEpLs1024B.h>

namespace irrigation::pressure_node {

// Serial console for the LS1024B bench session.
//
// It is the only line reader in the solar test build: `solar ...` lines are
// handled here and everything else is forwarded to the node's own command
// processor, so the existing command set keeps working unchanged.
//
// The battery profile is supplied by the caller rather than chosen here: which
// battery is attached is an installation fact, and this library is shared.
class SolarTestConsole {
 public:
  SolarTestConsole(PressureNodeCommandProcessor &delegate,
                   SolarControllerEpLs1024B &controller,
                   PrintController &logger,
                   epever_ls1024b::protocol::SolarVoltageBlockProfile profile);

  // Returns true when a complete line was received.
  bool poll(Stream &stream);

 private:
  static constexpr size_t BUFFER_SIZE = 96;
  static constexpr size_t MAX_TOKENS = 6;

  PressureNodeCommandProcessor &delegate_;
  SolarControllerEpLs1024B &controller_;
  PrintController &logger_;
  epever_ls1024b::protocol::SolarVoltageBlockProfile profile_{};

  char buffer_[BUFFER_SIZE]{};
  size_t length_{0};
  bool overflow_{false};

  void processLine(char *line);
  void printHelp();
  void printMeasurements();
  void printSettings();
  void printProfile();
  void handleWrite(int argc, char **argv);
  void handleFind();
  void handleAddress(int argc, char **argv);
  void handleRegisterRead(int argc, char **argv, bool holding);
  void printExceptionHint();
  void printHexFrame(const __FlashStringHelper *label, const uint8_t *data,
                     size_t length);
};

}  // namespace irrigation::pressure_node
