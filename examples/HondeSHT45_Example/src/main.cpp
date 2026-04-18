#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "PrintController.h"
#include "HondeSHT45.h"

#if defined(ARDUINO_ARCH_ESP32)
HardwareSerial& DebugPort = Serial0;
#else
#define DebugPort Serial
#endif

static PrintController printer(DebugPort, false);

static HondeSHT45 sht45(
    SENSOR_ID,
    SENSOR_I2C_ADDRESS,
    SENSOR_DEBUG,
    POWERLINE_INDEX_0,
    0,
    SAMPLE_RATE_15_MIN,
    10UL,
    SENSOR_DEFAULT_MAX_ERRORS,
    MIN_USEFUL_POWER_OFF_MS);

static void beginI2C() {
#if defined(ARDUINO_ARCH_ESP32)
  Wire.begin(PCB_I2C_SDA_PIN, PCB_I2C_SCL_PIN);
#else
  Wire.begin();
#endif
}

static void printBanner() {
  printer.println(F(""), true);
  printer.println(F("============================================================"), true);
  printer.println(F(" Honde SHT45 I2C Diagnostic Example"), true);
  printer.println(F("============================================================"), true);
  printer.print(F("PCB: "), true);
  printer.println(PCB_NAME, true);
  printer.println(F("- I2C air temperature and humidity sensor"), true);
  printer.println(F("- Power sensor from 3.3 V"), true);
  printer.println(F("- Loop reads temperature + relative humidity"), true);
  printer.println(F(""), true);

  printer.print(F("[I2C] Address: 0x"), true);
  printer.println((unsigned int)SENSOR_I2C_ADDRESS, true, "", HEX);
#if defined(ARDUINO_ARCH_ESP32)
  printer.print(F("[I2C] SDA GPIO: "), true);
  printer.print((int)PCB_I2C_SDA_PIN, true, " | SCL GPIO: ");
  printer.println((int)PCB_I2C_SCL_PIN, true);
#else
  printer.println(F("[I2C] Using board hardware SDA/SCL pins."), true);
#endif
  printer.println(F(""), true);
}

static void printReadResult(bool ok) {
  printer.print(F("[APP] Sensor ID: "), true);
  printer.print(sht45.getSensorId(), true, " | ");
  printer.print(F("I2C address: 0x"), true);
  printer.print((unsigned int)sht45.getAddress(), true, " | ", HEX);
  printer.println("", true);

  if (ok) {
    printer.println(F("[APP] Successfully Read Values:"), true);
  } else {
    printer.println(F("[APP] Read failed. Current driver attributes:"), true);
  }

  printer.print(F("Air temperature: "), true);
  printer.print(sht45.air_temperature, true, " C | ", 2);
  printer.print(F("Relative humidity: "), true);
  printer.print(sht45.air_humidity, true, " %RH", 2);
  printer.println("", true);

  if (!ok) {
    printer.print(F("[APP] Error count: "), true);
    printer.println((unsigned int)sht45.getConsecutiveErrors(), true);
  }
}

void setup() {
  DebugPort.begin(PCB_DEBUG_SERIAL_BAUD);
  delay(300);
  printBanner();

  beginI2C();
  sht45.setLogger(&printer);
  sht45.begin(&Wire);

  if (DO_SOFT_RESET_ON_BOOT) {
    printer.println(F("[APP] Sending SHT45 soft reset."), true);
    if (sht45.softReset()) {
      printer.println(F("[APP] Soft reset command accepted."), true);
    } else {
      printer.println(F("[APP] Soft reset command failed."), true);
    }
  }
}

void loop() {
  printer.println(F(""), true);
  printer.println(F("------------------------------------------------------------"), true);
  printer.println(F("[APP] New polling cycle"), true);
  printer.println(F("------------------------------------------------------------"), true);

  const bool ok = sht45.readData();
  printReadResult(ok);

  printer.println(F(""), true);
  delay(POLL_INTERVAL_MS);
}
