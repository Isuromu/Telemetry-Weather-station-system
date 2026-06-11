#include <Arduino.h>
#include "config.h"
#include "RS485SensorExampleRuntime.h"
#include "RS485AddressChangeExample.h"
#include "JXBS_GasSO2NO2PressureShield.h"

static JXBS_GasSO2NO2PressureShield gasPressureShield(
    rs485,
    SENSOR_ID,
    SENSOR_ADDRESS,
    SENSOR_DEBUG,
    NO2_SCALE_DIVISOR,
    SO2_SCALE_DIVISOR,
    NO2_MAX_PPM,
    SO2_MAX_PPM,
    POWERLINE_INDEX_0,
    RS485_PORT_INDEX_0,
    SAMPLE_RATE_15_MIN,
    1000UL,
    SENSOR_DEFAULT_MAX_ERRORS,
    MIN_USEFUL_POWER_OFF_MS);

static bool rangeContains(uint16_t startRegister, uint8_t registerCount, uint16_t targetRegister) {
  return targetRegister >= startRegister &&
         targetRegister < (uint16_t)(startRegister + registerCount);
}

static bool dumpRegisterRange(uint16_t startRegister, uint16_t endRegister, const __FlashStringHelper* label) {
  static const uint8_t maxRegisterCount = 125;
  if (endRegister < startRegister) {
    printer.println(F("[APP][REGDUMP] invalid range"), true);
    return false;
  }
  const uint16_t requestedCount = endRegister - startRegister + 1;
  if (requestedCount > maxRegisterCount) {
    printer.println(F("[APP][REGDUMP] range too large for one Modbus request"), true);
    return false;
  }
  const uint8_t registerCount = (uint8_t)requestedCount;
  const size_t responseSize = 5 + (2 * registerCount);

  uint8_t request[8] = {
    gasPressureShield.getAddress(),
    0x03,
    (uint8_t)(startRegister >> 8),
    (uint8_t)(startRegister & 0xFF),
    0x00,
    registerCount,
    0x00,
    0x00
  };
  uint8_t response[5 + (2 * maxRegisterCount)] = {0};
  const uint8_t check[3] = {
    gasPressureShield.getAddress(),
    0x03,
    (uint8_t)(2 * registerCount)
  };

  printer.print(F("[APP][REGDUMP] "), true);
  printer.print(label, true);
  printer.print(F(" start=0x"), true);
  printer.print((unsigned int)startRegister, true, "", HEX);
  printer.print(F(" count="), true);
  printer.println((unsigned int)registerCount, true);

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

  printer.println(ok ? F("[APP][REGDUMP] OK") : F("[APP][REGDUMP] FAILED"), true);
  if (!ok) {
    return false;
  }

  uint16_t words[maxRegisterCount] = {0};
  for (uint8_t i = 0; i < registerCount; ++i) {
    words[i] = ((uint16_t)response[3 + (2 * i)] << 8) |
               response[4 + (2 * i)];
  }

  for (uint8_t i = 0; i < registerCount; ++i) {
    char line[88];
    const uint16_t reg = startRegister + i;
    snprintf(line,
             sizeof(line),
             "[APP][REGDUMP] reg 0x%04X = %u (0x%04X)",
             (unsigned int)reg,
             (unsigned int)words[i],
             (unsigned int)words[i]);
    printer.println(line, true);
  }

  if (rangeContains(startRegister, registerCount, 0x0006) &&
      rangeContains(startRegister, registerCount, 0x0007)) {
    const uint8_t no2Index = (uint8_t)(0x0006 - startRegister);
    const uint8_t so2Index = (uint8_t)(0x0007 - startRegister);
    printer.print(F("[APP][REGDUMP] Current gas parser: NO2 raw="), true);
    printer.print((unsigned int)words[no2Index], true);
    printer.print(F(" cfg="), true);
    printer.print((double)words[no2Index] / NO2_SCALE_DIVISOR, true, " ppm", 2);
    printer.print(F(" /10="), true);
    printer.print((double)words[no2Index] / 10.0, true, " /100=", 2);
    printer.print((double)words[no2Index] / 100.0, true, " | SO2 raw=", 2);
    printer.print((unsigned int)words[so2Index], true);
    printer.print(F(" cfg="), true);
    printer.print((double)words[so2Index] / SO2_SCALE_DIVISOR, true, " ppm", 2);
    printer.print(F(" /10="), true);
    printer.print((double)words[so2Index] / 10.0, true, " /100=", 2);
    printer.print((double)words[so2Index] / 100.0, true, "", 2);
    printer.println("", true);
  }

  if (rangeContains(startRegister, registerCount, 0x0012) &&
      rangeContains(startRegister, registerCount, 0x0013)) {
    const uint8_t pressureHighIndex = (uint8_t)(0x0012 - startRegister);
    const uint32_t pressureRaw = ((uint32_t)words[pressureHighIndex] << 16) |
                                 (uint32_t)words[pressureHighIndex + 1];
    printer.print(F("[APP][REGDUMP] Current pressure parser raw="), true);
    printer.print((unsigned long)pressureRaw, true);
    printer.print(F(" /100="), true);
    printer.print((double)pressureRaw / 100.0, true, " mbar", 2);
    printer.println("", true);
  }

  return true;
}

static void printBanner() {
  printer.println(F(""), true);
  printer.println(F("============================================================"), true);
  printer.println(F(" JXBS SO2/NO2/Pressure Shield Diagnostic Example"), true);
  printer.println(F("============================================================"), true);
  printer.println(F("- Shield registers: NO2=0x0006 SO2=0x0007 Pressure=0x0012/0x0013"), true);
  printer.println(F("- Optional raw dump: one request covering 0x0000..0x0013"), true);
  printer.println(F("- RUN_DRIVER_READ_AFTER_DUMP=false keeps this example to one request per loop"), true);
  printer.println(F("- SO2/NO2 PDFs are analog manuals: 20ppm=>/100, 2000ppm=>/10 hypothesis"), true);
  printer.println(F("- Raw dump prints configured ppm plus /10 and /100 comparison"), true);
  printer.println(F(""), true);
}

static void printResult(bool ok) {
  printer.print(F("[APP] Sensor ID: "), true);
  printer.print(gasPressureShield.getSensorId(), true, " | Address: 0x");
  printer.println((unsigned int)gasPressureShield.getAddress(), true, "", HEX);
  printer.println(ok ? F("[APP] Successfully Read Values:") : F("[APP] Read failed. Current driver attributes:"), true);
  printer.print(F("NO2: "), true);
  printer.print(gasPressureShield.no2_ppm, true, " ppm | ", 2);
  printer.print(F("SO2: "), true);
  printer.print(gasPressureShield.so2_ppm, true, " ppm | ", 2);
  printer.print(F("Pressure: "), true);
  printer.print(gasPressureShield.pressure_mbar, true, " mbar", 2);
  printer.println("", true);
}

void setup() {
  beginRS485SensorExample();
  printBanner();
  if (ADDRESS_CHANGE_AT_BOOT) {
    runAddressChangeAtBoot(gasPressureShield, printer, ADDRESS_CHANGE_NEW_ADDRESS, F("Only the target SO2/NO2/pressure shield should be connected."));
  }
  if (DO_SCAN) {
    const uint8_t found = gasPressureShield.scanForAddress(1, 247, 150, 20);
    printer.print(F("[APP] Scan result: 0x"), true);
    printer.println((unsigned int)found, true, "", HEX);
  }
}

void loop() {
  printer.println(F(""), true);
  printer.println(F("------------------------------------------------------------"), true);
  if (READ_SENSOR_REGISTER_RANGE) {
    dumpRegisterRange(REGISTER_DUMP_START, REGISTER_DUMP_END, F("sensor register range"));
  }
#if RUN_DRIVER_READ_AFTER_DUMP
  printResult(gasPressureShield.readData());
#else
  if (!READ_SENSOR_REGISTER_RANGE) {
    printResult(gasPressureShield.readData());
  }
#endif
  delay(POLL_INTERVAL_MS);
}
