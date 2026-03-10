#include <Arduino.h>
#include "config.h"
#include "PrintController.h"
#include "RS485Modbus.h"
#include "RikaLeafSensor.h"

static PrintController printer(Serial, false);
static RS485Bus rs485;

#if defined(ARDUINO_ARCH_ESP32)
HardwareSerial RS485Port(1);
#endif

static RikaLeafSensor leaf(rs485, SENSOR_ID, SENSOR_ADDRESS, SENSOR_DEBUG);

static void printBanner() {
  printer.println(F(""), true);
  printer.println(F("=============================================="), true);
  printer.println(F(" Rika Leaf Sensor Simple Diagnostic Example"), true);
  printer.println(F("=============================================="), true);
  printer.println(F("This example is intentionally simple."), true);
  printer.println(F("- Optional scan in setup()"), true);
  printer.println(F("- Optional address change with button at boot"), true);
  printer.println(F("- Then plain polling every 2000 ms"), true);
  printer.println(F(""), true);
}

static void printReadResult(bool ok) {
  if (ok) {
    printer.print(F("[APP] Sensor ID: "), true);
    printer.print(leaf.getSensorId(), true, " | ");
    printer.print(F("Address: 0x"), true);
    printer.print((unsigned int)leaf.getAddress(), true, " | ", HEX);
    printer.print(F("Temperature: "), true);
    printer.print(leaf.leaf_temp, true, " C | ", 1);
    printer.print(F("Humidity: "), true);
    printer.print(leaf.leaf_humid, true, " %", 1);
    printer.println("", true);
  } else {
    printer.print(F("[APP] Read failed for sensor "), true);
    printer.print(leaf.getSensorId(), true, " | ");
    printer.print(F("Address: 0x"), true);
    printer.print((unsigned int)leaf.getAddress(), true, " | ", HEX);
    printer.print(F("Error count: "), true);
    printer.println((unsigned int)leaf.getConsecutiveErrors(), true);
  }
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(300);
  printBanner();

  pinMode(CHANGE_BUTTON_PIN, INPUT_PULLUP);

  rs485.setDebug(&printer);
#if defined(ARDUINO_ARCH_ESP32)
  rs485.begin(RS485Port, RS485_BAUD, RS485_RX_PIN, RS485_TX_PIN, RS485_CONFIG);
#else
  rs485.begin(Serial2, RS485_BAUD, -1, -1, RS485_CONFIG);
#endif
  rs485.setDirectionControl(RS485_DE_PIN, true);

  if (DO_SCAN) {
    printer.println(F("[APP] Scan mode is enabled. Searching address..."), true);
    const uint8_t found = leaf.scanForAddress(1, 247, 120, 20);
    if (found != 0) {
      printer.print(F("[APP] Sensor found at address 0x"), true);
      printer.println((unsigned int)found, true, "", HEX);
    } else {
      printer.println(F("[APP] No sensor responded during scan."), true);
    }
  }

  if (digitalRead(CHANGE_BUTTON_PIN) == LOW) {
    delay(50);
    if (digitalRead(CHANGE_BUTTON_PIN) == LOW) {
      printer.println(F("[APP] Address change button detected at boot."), true);
      printer.println(F("[APP] Make sure white wire is on V+ and only one sensor is on the bus."), true);
      if (leaf.changeAddress(NEW_ADDRESS, 3, 500, 20)) {
        printer.print(F("[APP] Address changed successfully. New address = 0x"), true);
        printer.println((unsigned int)leaf.getAddress(), true, "", HEX);
      } else {
        printer.println(F("[APP] Address change failed."), true);
      }
    }
  }
}

void loop() {
  const bool ok = leaf.readData();
  printReadResult(ok);
  delay(POLL_INTERVAL_MS);
}