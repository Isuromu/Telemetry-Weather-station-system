#include <Arduino.h>
#include "config.h"
#include "PrintController.h"
#include "RS485Modbus.h"
#include "JXBS_SoilComp7in1.h"

#if defined(ARDUINO_ARCH_ESP32)
HardwareSerial& DebugPort = Serial0;
HardwareSerial RS485Port(1);
#else
#define DebugPort Serial
#endif

static PrintController printer(DebugPort, false);
static RS485Bus rs485;

static JXBS_SoilComp7in1 soil(
    rs485,
    SENSOR_ID,
    SENSOR_ADDRESS,
    SENSOR_DEBUG,
    POWERLINE_INDEX_0,
    RS485_PORT_INDEX_0,
    SAMPLE_RATE_15_MIN,
    1000UL,
    SENSOR_DEFAULT_MAX_ERRORS,
    MIN_USEFUL_POWER_OFF_MS);

static void printBanner() {
  printer.println(F(""), true);
  printer.println(F("============================================================"), true);
  printer.println(F(" JXBS Soil Comprehensive 7-in-1 Diagnostic Example"), true);
  printer.println(F("============================================================"), true);
  printer.println(F("- Optional scan in setup()"), true);
  printer.println(F("- Optional address change at boot"), true);
  printer.println(F("- Loop reads 7 values"), true);
  printer.println(F(""), true);
}

static void printReadResult(bool ok) {
  if (ok) {
    printer.print(F("[APP] Sensor ID: "), true);
    printer.print(soil.getSensorId(), true, " | ");

    printer.print(F("Address: 0x"), true);
    printer.print((unsigned int)soil.getAddress(), true, " | ", HEX);
    printer.println("", true);

    printer.println(F("[APP] Successfully Read Values:"), true);

    printer.print(F("Moisture: "), true);
    printer.print(soil.soil_moisture, true, " % | ", 1);

    printer.print(F("Temp: "), true);
    printer.print(soil.soil_temp, true, " C", 1);
    printer.println("", true);

    printer.print(F("EC: "), true);
    printer.print(soil.soil_ec, true, " us/cm | ", 0);

    printer.print(F("pH: "), true);
    printer.print(soil.soil_ph, true, "", 2);
    printer.println("", true);

    printer.print(F("N: "), true);
    printer.print((unsigned int)soil.soil_nitrogen, true, " mg/kg | ");

    printer.print(F("P: "), true);
    printer.print((unsigned int)soil.soil_phosphorus, true, " mg/kg | ");

    printer.print(F("K: "), true);
    printer.print((unsigned int)soil.soil_potassium, true, " mg/kg");
    printer.println("", true);
  } else {
    printer.print(F("[APP] Read failed for sensor "), true);
    printer.print(soil.getSensorId(), true, " | ");
    printer.print(F("Error count: "), true);
    printer.println((unsigned int)soil.getConsecutiveErrors(), true);
  }
}

void setup() {
  DebugPort.begin(PCB_DEBUG_SERIAL_BAUD);
  delay(300);
  printBanner();

  pinMode(PCB_SERVICE_BUTTON_PIN, INPUT_PULLUP);

  rs485.setDebug(&printer);

#if defined(ARDUINO_ARCH_ESP32)
  rs485.begin(RS485Port,
              RS485_DEFAULT_BAUD,
              PCB_RS485_RX_PINS[RS485_PORT_INDEX_0],
              PCB_RS485_TX_PINS[RS485_PORT_INDEX_0],
              RS485_DEFAULT_SERIAL_CONFIG);
#else
  rs485.begin(Serial2, RS485_DEFAULT_BAUD, -1, -1, RS485_DEFAULT_SERIAL_CONFIG);
#endif

  rs485.setDirectionControl(PCB_RS485_DE_PINS[RS485_PORT_INDEX_0],
                            PCB_RS485_DE_ACTIVE_HIGH[RS485_PORT_INDEX_0]);

  if (DO_SCAN) {
    printer.println(F("[APP] Scan mode enabled. Searching sensor address..."), true);
    const uint8_t found = soil.scanForAddress(1, 247, 150, SENSOR_DEFAULT_AFTER_REQ_MS);
    if (found != 0) {
      printer.print(F("[APP] Sensor found at address 0x"), true);
      printer.println((unsigned int)found, true, "", HEX);
    } else {
      printer.println(F("[APP] No sensor responded during scan."), true);
    }
    printer.println(F(""), true);
  }

  if (TRY_ADDRESS_CHANGE_AT_BOOT && digitalRead(PCB_SERVICE_BUTTON_PIN) == LOW) {
    delay(50);
    if (digitalRead(PCB_SERVICE_BUTTON_PIN) == LOW) {
      printer.println(F("[APP] Address change requested at boot"), true);

      if (soil.changeAddress(NEW_ADDRESS, SENSOR_DEFAULT_BUS_RETRIES, 500, SENSOR_DEFAULT_AFTER_REQ_MS)) {
        printer.print(F("[APP] Address changed successfully to 0x"), true);
        printer.println((unsigned int)soil.getAddress(), true, "", HEX);
      } else {
        printer.println(F("[APP] Address change failed."), true);
      }
      printer.println(F(""), true);
    }
  }
}

void loop() {
  printer.println(F(""), true);
  printer.println(F("------------------------------------------------------------"), true);
  printer.println(F("[APP] New polling cycle"), true);
  printer.println(F("------------------------------------------------------------"), true);

  const bool ok = soil.readData();
  printReadResult(ok);

  printer.println(F(""), true);
  delay(POLL_INTERVAL_MS);
}