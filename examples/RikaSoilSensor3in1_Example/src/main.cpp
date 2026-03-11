#include <Arduino.h>
#include "config.h"
#include "PrintController.h"
#include "RS485Modbus.h"
#include "RikaSoilSensor3in1.h"

#if defined(ARDUINO_ARCH_ESP32)
HardwareSerial& DebugPort = Serial0;
HardwareSerial RS485Port(1);
#else
#define DebugPort Serial
#endif

static PrintController printer(DebugPort, false);
static RS485Bus rs485;

static RikaSoilSensor3in1 soil(
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
  printer.println(F(" Rika Soil Sensor 3-in-1 Diagnostic Example"), true);
  printer.println(F("============================================================"), true);
  printer.println(F("- Optional scan in setup()"), true);
  printer.println(F("- Loop reads temp, VWC, EC"), true);
  printer.println(F("- Optional loop reads soil type and epsilon"), true);
  printer.println(F(""), true);
}

static void printMainReadResult(bool ok) {
  if (ok) {
    printer.print(F("[APP] Sensor ID: "), true);
    printer.print(soil.getSensorId(), true, " | ");

    printer.print(F("Address: 0x"), true);
    printer.print((unsigned int)soil.getAddress(), true, " | ", HEX);

    printer.println("", true);
    printer.print(F("[APP] Successfully Read Values: "), true, "| ");

    printer.print(F("Temp: "), true);
    printer.print(soil.soil_temp, true, " C | ", 1);

    printer.print(F("VWC: "), true);
    printer.print(soil.soil_vwc, true, " % | ", 1);

    printer.print(F("EC: "), true);
    printer.print(soil.soil_ec, true, " mS/cm", 4);
    printer.println("", true);
  } else {
    printer.print(F("[APP] Read failed for sensor "), true);
    printer.print(soil.getSensorId(), true, " | ");
    printer.print(F("Error count: "), true);
    printer.println((unsigned int)soil.getConsecutiveErrors(), true);
  }
}

static void printSoilTypeResult(bool ok, RikaSoilSensor3in1::SoilType type) {
  if (ok) {
    printer.print(F("[APP] Soil type: "), true);
    printer.println(RikaSoilSensor3in1::soilTypeToString(type), true);
  } else {
    printer.println(F("[APP] Failed to read soil type"), true);
  }
}

static void printEpsilonResult(bool ok, double eps) {
  if (ok) {
    printer.print(F("[APP] Epsilon: "), true);
    printer.println(eps, true, "", 2);
  } else {
    printer.println(F("[APP] Failed to read epsilon"), true);
  }
}

static void printCoeffResult(const __FlashStringHelper* label, bool ok, double value) {
  printer.print(label, true);
  if (ok) {
    printer.println(value, true, "", 2);
  } else {
    printer.println(F("FAILED"), true);
  }
}

void setup() {
  DebugPort.begin(PCB_DEBUG_SERIAL_BAUD);
  delay(300);
  printBanner();

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
    const uint8_t found = soil.scanForAddress(1, 247, 120, 20);
    if (found != 0) {
      printer.print(F("[APP] Sensor found at address 0x"), true);
      printer.println((unsigned int)found, true, "", HEX);
    } else {
      printer.println(F("[APP] No sensor responded during scan."), true);
    }
    printer.println(F(""), true);
  }

  if (SET_SOIL_TYPE_ON_BOOT) {
    printer.print(F("[APP] Setting soil type at boot to value "), true);
    printer.println((unsigned int)BOOT_SOIL_TYPE, true);

    if (soil.setSoilType((RikaSoilSensor3in1::SoilType)BOOT_SOIL_TYPE, 3, 500, 20)) {
      printer.println(F("[APP] Soil type set successfully."), true);
    } else {
      printer.println(F("[APP] Failed to set soil type."), true);
    }
    printer.println(F(""), true);
  }
}

void loop() {
  printer.println(F(""), true);
  printer.println(F("------------------------------------------------------------"), true);
  printer.println(F("[APP] New polling cycle"), true);
  printer.println(F("------------------------------------------------------------"), true);

  const bool ok = soil.readData();
  printMainReadResult(ok);

  if (READ_SOIL_TYPE_IN_LOOP) {
    RikaSoilSensor3in1::SoilType type = RikaSoilSensor3in1::SOIL_UNKNOWN;
    const bool typeOk = soil.readSoilType(type, 3, 500, 20);
    printSoilTypeResult(typeOk, type);
  }

  if (READ_EPSILON_IN_LOOP) {
    double eps = -99.0;
    const bool epsOk = soil.readEpsilon(eps, 3, 500, 20);
    printEpsilonResult(epsOk, eps);
  }

  if (READ_COMP_COEFFS_IN_LOOP) {
    double coeff = -99.0;

    bool okCoeff = soil.readCompensationCoeff(REG_EC_TEMP_COEFF, coeff, 3, 500, 20);
    printCoeffResult(F("[APP] EC temp coeff: "), okCoeff, coeff);

    okCoeff = soil.readCompensationCoeff(REG_SALINITY_COEFF, coeff, 3, 500, 20);
    printCoeffResult(F("[APP] Salinity coeff: "), okCoeff, coeff);

    okCoeff = soil.readCompensationCoeff(REG_TDS_COEFF, coeff, 3, 500, 20);
    printCoeffResult(F("[APP] TDS coeff: "), okCoeff, coeff);
  }

  printer.println(F(""), true);
  delay(POLL_INTERVAL_MS);
}