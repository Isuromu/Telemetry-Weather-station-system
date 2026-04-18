#include <Arduino.h>
#include "config.h"
#include "RS485SensorExampleRuntime.h"
#include "RS485AddressChangeExample.h"
#include "JXCT_WeatherStationSensors.h"

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

static void printBanner() {
  printer.println(F(""), true);
  printer.println(F("============================================================"), true);
  printer.println(F(" JXBS SO2/NO2/Pressure Shield Diagnostic Example"), true);
  printer.println(F("============================================================"), true);
  printer.println(F("- Shield registers: NO2=0x0006 SO2=0x0007 Pressure=0x0012/0x0013"), true);
  printer.println(F("- Confirm SO2/NO2 scale on hardware; local PDFs are analog-output manuals"), true);
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
  printResult(gasPressureShield.readData());
  delay(POLL_INTERVAL_MS);
}
