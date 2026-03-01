#include "LeafSensor.h"
#include "PrintController.h"
#include "RS485Modbus.h"
#include "RikaLeafSensor.h"
#include "SoilSensor7in1.h"
#include <Arduino.h>

// -------------------- Configuration --------------------
#define SERIAL_BAUD 115200
#define RS485_BAUD 9600
#define PRINT_VERBOSITY 2 // 0=SUMMARY  1=IO  2=TRACE
#define PRINT_ENABLED 1   // 0=silent

// Pins for ESP32-S3
#define RS485_RX_PIN 16
#define RS485_TX_PIN 17
#define RS485_DE_PIN 21

// Scanner button (held at boot)
#define SCAN_BUTTON_PIN 14
#define SCAN_WINDOW_MS 3000
#define SCAN_TIMEOUT_MS 100
// -------------------------------------------------------

static PrintController
    printer(Serial, static_cast<bool>(PRINT_ENABLED),
            static_cast<PrintController::Verbosity>(PRINT_VERBOSITY));
static RS485Bus bus;

#if defined(ARDUINO_ARCH_ESP32)
HardwareSerial RS485Serial(1);
#endif

// Define the sensors present on your bus
static SoilSensor7in1 soil(bus, 0x01);
static LeafSensor leaf(bus, 0x02);
static RikaLeafSensor rikaLeaf(bus, 0x03);

void scanAddresses() {
  printer.logln(PrintController::TRACE,
                F("\n=================================================="));
  printer.logln(PrintController::TRACE,
                F("  MODBUS ADDRESS SCANNER (Discovery Mode)"));
  printer.logln(PrintController::TRACE,
                F("  Iterating addresses 1 to 247 with FC03..."));
  printer.logln(PrintController::TRACE,
                F("==================================================\n"));

  uint8_t found = 0;
  for (uint16_t addr = 1; addr <= 247; addr++) {
    // 1. Prepare Request
    uint8_t req[8];
    req[0] = static_cast<uint8_t>(addr);
    req[1] = 0x03;
    req[2] = 0x00;
    req[3] = 0x00;
    req[4] = 0x00;
    req[5] = 0x01;
    uint16_t crc = RS485Bus::crc16Modbus(req, 6);
    req[6] = crc & 0xFF;
    req[7] = crc >> 8;

    // 2. Log Progress with Hex Request
    printer.log(PrintController::TRACE, F("[Scanner] Trying 0x"));
    if (addr < 16)
      printer.print(F("0"));
    printer.print(addr, HEX);
    printer.print(F(" ("));
    for (int i = 0; i < 8; i++) {
      if (req[i] < 16)
        printer.print(F("0"));
      printer.print(req[i], HEX);
      if (i < 7)
        printer.print(F(" "));
    }
    printer.print(F(") -> "));

    // 3. Transfer
    if (bus.transferRaw(req, 8, SCAN_TIMEOUT_MS)) {
      printer.println(F("FOUND!"));
      found++;
    } else {
      printer.println(F("no response"));
    }

    // 4. Yield for ESP32 background tasks
    delay(1);
    yield();
  }

  printer.logln(PrintController::TRACE,
                F("\n--------------------------------------------------"));
  printer.log(PrintController::TRACE, F("Scan Complete. Found "));
  printer.print(found);
  printer.println(F(" devices."));
  printer.logln(PrintController::TRACE,
                F("--------------------------------------------------\n"));
  bus.flushInput();
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(500);

  // Firmware identification banner
  printer.println(F(""));
  printer.println(F("==========================================="));
  printer.println(F("  PROJECT: Telemetry Weather Station"));
  printer.println(F("  FIRMWARE: Main Sensor Loop"));
  printer.println(F("  Program starts here (reset point)."));
  printer.println(F("==========================================="));

  pinMode(SCAN_BUTTON_PIN, INPUT_PULLUP);
  bus.setLogger(&printer);

#if defined(ARDUINO_ARCH_ESP32)
  bus.begin(RS485Serial, RS485_BAUD, RS485_RX_PIN, RS485_TX_PIN);
#else
  bus.begin(Serial2, RS485_BAUD);
#endif
  bus.setDirectionControl(RS485_DE_PIN, true);

  printer.logln(PrintController::TRACE,
                F("--> WAITING FOR SCAN BUTTON (GPIO14 TO GND) <--"));
  printer.logln(PrintController::TRACE,
                F("Hold button NOW for 5 seconds to enter DISCOVERY MODE..."));

  uint32_t deadline = millis() + 5000; // 5 second window
  bool doScan = false;
  while (millis() < deadline) {
    if (digitalRead(SCAN_BUTTON_PIN) == LOW) {
      delay(50); // debounce
      if (digitalRead(SCAN_BUTTON_PIN) == LOW) {
        doScan = true;
        break;
      }
    }
    delay(10);
  }

  if (doScan) {
    scanAddresses();
  } else {
    printer.logln(
        PrintController::TRACE,
        F("No button press detected. Entering normal POLLING LOOP.\n"));
  }
}

void loop() {
  static uint32_t lastPoll = 0;
  if (millis() - lastPoll >= 3000 || lastPoll == 0) {
    lastPoll = millis();

    printer.logln(PrintController::TRACE, F("--- Polling Cycle Start ---"));

    // 1. Soil Sensor
    SoilSensor7in1_Data soilData{};
    if (soil.readAll(soilData)) {
      printer.log(PrintController::TRACE, F("SOIL (addr 1)   : Temp="));
      printer.print(soilData.temperatureC, 1);
      printer.print(F("C, Hum="));
      printer.print(soilData.humidityPct, 1);
      printer.println(F("%"));
    } else {
      printer.logln(PrintController::TRACE, F("SOIL (addr 1)   : FAILED"));
    }

    // 2. Leaf Sensor
    LeafSensor_Data leafData{};
    if (leaf.readAll(leafData)) {
      printer.log(PrintController::TRACE, F("LEAF (addr 2)   : Temp="));
      printer.print(leafData.temperatureC, 1);
      printer.print(F("C, Hum="));
      printer.print(leafData.humidityPct, 1);
      printer.println(F("%"));
    } else {
      printer.logln(PrintController::TRACE, F("LEAF (addr 2)   : FAILED"));
    }

    // 3. Rika Leaf Sensor
    RikaLeafSensor_Data rikaData{};
    if (rikaLeaf.readAll(rikaData)) {
      printer.log(PrintController::TRACE, F("RIKA LEAF (addr 3): Temp="));
      printer.print(rikaData.rika_leaf_temp, 1);
      printer.print(F("C, Hum="));
      printer.print(rikaData.rika_leaf_humid, 1);
      printer.println(F("%"));
    } else {
      printer.logln(PrintController::TRACE, F("RIKA LEAF (addr 3): FAILED"));
    }

    printer.logln(PrintController::TRACE, F("--- Polling Cycle End ---\n"));
  }
}
