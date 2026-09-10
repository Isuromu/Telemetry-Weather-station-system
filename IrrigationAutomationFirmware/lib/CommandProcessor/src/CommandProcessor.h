#pragma once

#include <Arduino.h>
#include <DelixiCDIE100.h>
#include <PrintController.h>
#include <PumpController.h>

class CommandProcessor {
 public:
  CommandProcessor(PumpController &pump, DelixiCDIE100 &vfd,
                   PrintController &logger);

  void processLine(const char *line);
  void printHelp();

 private:
  static constexpr size_t MAX_ARGUMENTS = 24;
  static constexpr size_t MAX_LINE_LENGTH = 192;

  PumpController &pump_;
  DelixiCDIE100 &vfd_;
  PrintController &logger_;

  void handlePump(int argc, char *argv[]);
  void handleVfd(int argc, char *argv[]);
  void handleModbus(int argc, char *argv[]);
  void printPumpStatus(bool telemetry);
  void printRawResponse(const Rs485Result &result);
  static bool parseUInt16(const char *text, uint16_t &value);
  static bool parseHexByte(const char *text, uint8_t &value);
};

class SerialCommandSource {
 public:
  SerialCommandSource(CommandProcessor &processor, PrintController &logger);
  void poll(Stream &stream);

 private:
  static constexpr size_t BUFFER_SIZE = 192;
  CommandProcessor &processor_;
  PrintController &logger_;
  char buffer_[BUFFER_SIZE];
  size_t length_;
  bool overflow_;
};

