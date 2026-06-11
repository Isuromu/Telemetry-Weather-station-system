#include <Arduino.h>
#include "config.h"
#include "RS485SensorExampleRuntime.h"
#include "RS485AddressChangeExample.h"
#include "JXBS_GasO3CONH3Shield.h"

static JXBS_GasO3CONH3Shield gasShield(
    rs485,
    SENSOR_ID,
    SENSOR_ADDRESS,
    SENSOR_DEBUG,
    CO_MAX_PPM,
    O3_MAX_PPM,
    NH3_MAX_PPM,
    POWERLINE_INDEX_0,
    RS485_PORT_INDEX_0,
    SAMPLE_RATE_15_MIN,
    1000UL,
    SENSOR_DEFAULT_MAX_ERRORS,
    MIN_USEFUL_POWER_OFF_MS);

static bool dumpShieldGasRegisters() {
  static const uint16_t startRegister = 0x0006;
  static const uint8_t registerCount = 3;
  static const uint8_t responseSize = 5 + (2 * registerCount);

  uint8_t request[8] = {
    gasShield.getAddress(),
    0x03,
    (uint8_t)(startRegister >> 8),
    (uint8_t)(startRegister & 0xFF),
    0x00,
    registerCount,
    0x00,
    0x00
  };
  uint8_t response[responseSize] = {0};
  const uint8_t check[3] = {
    gasShield.getAddress(),
    0x03,
    (uint8_t)(2 * registerCount)
  };

  printer.print(F("[APP][REGDUMP] addr=0x"), true);
  printer.print((unsigned int)gasShield.getAddress(), true, "", HEX);
  printer.println(F(" read shield gas registers 0x0006..0x0008"), true);

  const bool ok = rs485.SendRequest(request,
                                    sizeof(request),
                                    response,
                                    sizeof(response),
                                    check,
                                    sizeof(check),
                                    SENSOR_DEFAULT_BUS_RETRIES,
                                    SENSOR_DEFAULT_READ_TIMEOUT_MS,
                                    SENSOR_DEBUG,
                                    SENSOR_DEFAULT_AFTER_REQ_MS);

  if (!ok) {
    printer.print(F("[APP][REGDUMP] addr=0x"), true);
    printer.print((unsigned int)gasShield.getAddress(), true, "", HEX);
    printer.println(F(" FAILED"), true);
    return false;
  }

  printer.print(F("[APP][REGDUMP] addr=0x"), true);
  printer.print((unsigned int)gasShield.getAddress(), true, "", HEX);
  printer.println(F(" OK"), true);

  uint16_t words[registerCount] = {0};
  for (uint8_t i = 0; i < registerCount; ++i) {
    words[i] = ((uint16_t)response[3 + (2 * i)] << 8) |
               response[4 + (2 * i)];
  }

  printer.print(F("[APP][REGDUMP] CO raw="), true);
  printer.print((unsigned int)words[0], true);
  printer.print(F(" /10="), true);
  printer.print((double)words[0] / 10.0, true, " ppm | O3 raw=", 1);
  printer.print((unsigned int)words[1], true);
  printer.print(F(" /100="), true);
  printer.print((double)words[1] / 100.0, true, " ppm | NH3 raw=", 2);
  printer.print((unsigned int)words[2], true);
  printer.print(F(" /10="), true);
  printer.print((double)words[2] / 10.0, true, " ppm", 1);
  printer.println("", true);

  for (uint8_t i = 0; i < registerCount; ++i) {
    char line[80];
    const uint16_t reg = startRegister + i;
    snprintf(line,
             sizeof(line),
             "[APP][REGDUMP] addr=0x%02X reg 0x%04X = %u (0x%04X)",
             (unsigned int)gasShield.getAddress(),
             (unsigned int)reg,
             (unsigned int)words[i],
             (unsigned int)words[i]);
    printer.println(line, true);
  }

  return true;
}

static void printBanner() {
  printer.println(F(""), true);
  printer.println(F("============================================================"), true);
  printer.println(F(" JXBS O3/CO/NH3 Gas Shield Diagnostic Example"), true);
  printer.println(F("============================================================"), true);
  printer.println(F("- Shield label map: CO=0x0006 O3=0x0007 NH3=0x0008"), true);
  printer.println(F("- This shield does not provide RH/temperature registers"), true);
  printer.println(F("- Driver reads one block: start=0x0006 count=3"), true);
  printer.println(F(""), true);
}

static void printResult(bool ok) {
  printer.print(F("[APP] Sensor ID: "), true);
  printer.print(gasShield.getSensorId(), true, " | Address: 0x");
  printer.println((unsigned int)gasShield.getAddress(), true, "", HEX);
  printer.println(ok ? F("[APP] Successfully Read Values:") : F("[APP] Read failed. Current driver attributes:"), true);
  printer.print(F("CO: "), true);
  printer.print(gasShield.co_ppm, true, " ppm | ", 1);
  printer.print(F("O3: "), true);
  printer.print(gasShield.o3_ppm, true, " ppm | ", 2);
  printer.print(F("NH3: "), true);
  printer.print(gasShield.nh3_ppm, true, " ppm", 1);
  printer.println("", true);
}

void setup() {
  beginRS485SensorExample();
  printBanner();
  if (ADDRESS_CHANGE_AT_BOOT) {
    runAddressChangeAtBoot(gasShield, printer, ADDRESS_CHANGE_NEW_ADDRESS, F("Only the target O3/CO/NH3 gas shield should be connected."));
  }
  if (DO_SCAN) {
    const uint8_t found = gasShield.scanForAddress(1, 247, 150, 20);
    printer.print(F("[APP] Scan result: 0x"), true);
    printer.println((unsigned int)found, true, "", HEX);
  }
}

void loop() {
  printer.println(F(""), true);
  printer.println(F("------------------------------------------------------------"), true);
  if (DUMP_SHIELD_GAS_REGISTERS) {
    dumpShieldGasRegisters();
  }
  printResult(gasShield.readData());
  delay(POLL_INTERVAL_MS);
}
