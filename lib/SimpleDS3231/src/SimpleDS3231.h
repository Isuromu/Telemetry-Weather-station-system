#pragma once

#include <Arduino.h>
#include <Wire.h>

/*
  SimpleDS3231

  A tiny DS3231 helper for this firmware:
  - reads UTC time from the external RTC
  - writes UTC time after a network/NTP sync
  - converts between DS3231 calendar fields and Unix epoch seconds

  It does not use alarms. ESP32 wakes itself with its own deep-sleep timer;
  DS3231 is only the trusted wall clock.
*/

struct SimpleDS3231DateTime {
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
  uint32_t epochSeconds;
};

class SimpleDS3231 {
public:
  explicit SimpleDS3231(uint8_t address = 0x68);

  void begin(TwoWire& wire);

  bool read(SimpleDS3231DateTime& out);
  bool setFromEpoch(uint32_t epochSeconds);

  static bool isValid(const SimpleDS3231DateTime& value);
  static bool formatUtc(uint32_t epochSeconds, char* out, size_t outSize);
  static uint32_t toEpochSeconds(uint16_t year,
                                 uint8_t month,
                                 uint8_t day,
                                 uint8_t hour,
                                 uint8_t minute,
                                 uint8_t second);
  static bool fromEpochSeconds(uint32_t epochSeconds, SimpleDS3231DateTime& out);

private:
  TwoWire* _wire;
  uint8_t _address;

  bool readRegisters(uint8_t firstRegister, uint8_t* data, uint8_t count);
  bool writeRegisters(uint8_t firstRegister, const uint8_t* data, uint8_t count);
  bool readStatus(uint8_t& status);
  bool clearOscillatorStopFlag();

  static uint8_t bcdToDec(uint8_t value);
  static uint8_t decToBcd(uint8_t value);
  static int32_t daysFromCivil(int32_t year, uint8_t month, uint8_t day);
  static void civilFromDays(int32_t days, uint16_t& year, uint8_t& month, uint8_t& day);
  static uint8_t dayOfWeek(uint32_t epochSeconds);
};
