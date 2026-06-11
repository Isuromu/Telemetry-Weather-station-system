#include <Arduino.h>
#include "config.h"
#include "RS485SensorExampleRuntime.h"
#include "RS485AddressChangeExample.h"
#include "JXBS_OpticalRainGauge.h"

static JXBS_OpticalRainGauge rainGauge(
    rs485,
    SENSOR_ID,
    SENSOR_ADDRESS,
    SENSOR_DEBUG,
    RAIN_MAX_MM,
    POWERLINE_INDEX_0,
    RS485_PORT_INDEX_0,
    SAMPLE_RATE_15_MIN,
    1000UL,
    SENSOR_DEFAULT_MAX_ERRORS,
    MIN_USEFUL_POWER_OFF_MS);

static void printBanner() {
  printer.println(F(""), true);
  printer.println(F("============================================================"), true);
  printer.println(F(" JXBS Optical Rain Gauge Diagnostic Example"), true);
  printer.println(F("============================================================"), true);
  printer.println(F("- Register 0x0003 = accumulated rainfall raw / 10 mm"), true);
  printer.println(F("- Separate red/white lead is optional 0-5V pulse output"), true);
  printer.println(F("- RS485 use: leave pulse lead insulated unless you count pulses"), true);
  printer.println(F("- Optional clear command: write 0 to 0x0105 or 0x0101"), true);
  printer.println(F(""), true);
}

static void printResult(bool ok) {
  printer.print(F("[APP] Sensor ID: "), true);
  printer.print(rainGauge.getSensorId(), true, " | Address: 0x");
  printer.println((unsigned int)rainGauge.getAddress(), true, "", HEX);
  printer.println(ok ? F("[APP] Successfully Read Values:") : F("[APP] Read failed. Current driver attributes:"), true);
  printer.print(F("Rain raw: "), true);
  printer.print((unsigned int)rainGauge.rainfall_raw, true, " | ", DEC);
  printer.print(F("Rain: "), true);
  printer.print(rainGauge.rainfall_mm, true, " mm", 1);
  printer.println("", true);
}

void setup() {
  beginRS485SensorExample();
  printBanner();

  if (ADDRESS_CHANGE_AT_BOOT) {
    runAddressChangeAtBoot(rainGauge,
                           printer,
                           ADDRESS_CHANGE_NEW_ADDRESS,
                           F("Only the target optical rain gauge should be connected."));
  }

  if (CLEAR_RAIN_AT_BOOT) {
    const bool cleared = rainGauge.clearAccumulatedRainfall(CLEAR_RAIN_REGISTER);
    printer.println(cleared ? F("[APP] Rain accumulator clear command OK")
                            : F("[APP] Rain accumulator clear command failed"), true);
  }

  if (DO_SCAN) {
    const uint8_t found = rainGauge.scanForAddress(1, 247, 150, 20);
    printer.print(F("[APP] Scan result: 0x"), true);
    printer.println((unsigned int)found, true, "", HEX);
  }
}

void loop() {
  printer.println(F(""), true);
  printer.println(F("------------------------------------------------------------"), true);
  printResult(rainGauge.readData());
  delay(POLL_INTERVAL_MS);
}
