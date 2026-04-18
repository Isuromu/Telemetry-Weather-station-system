#include <Arduino.h>
#include "config.h"
#include "RS485SensorExampleRuntime.h"
#include "RS485AddressChangeExample.h"
#include "JXCT_WeatherStationSensors.h"

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

static void printBanner() {
  printer.println(F(""), true);
  printer.println(F("============================================================"), true);
  printer.println(F(" JXBS O3/CO/NH3 Gas Shield Diagnostic Example"), true);
  printer.println(F("============================================================"), true);
  printer.println(F("- Shield registers: CO=0x0006 O3=0x0007 NH3=0x0008"), true);
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
  printResult(gasShield.readData());
  delay(POLL_INTERVAL_MS);
}
