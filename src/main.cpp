#include "LeafSensor.h"
#include "PrintController.h"
#include "RS485Modbus.h"
#include "RikaLeafSensor.h"
#include "SoilSensor7in1.h"
#include <Arduino.h>
#include <stdint.h>

// -------------------- Compile-time settings from platformio.ini
// --------------------
#ifndef SERIAL_BAUD
#define SERIAL_BAUD 115200
#endif

#ifndef RS485_BAUD
#define RS485_BAUD 9600
#endif

#ifndef RS485_DE_PIN
#define RS485_DE_PIN 21
#endif

#ifndef RS485_RX_PIN
#define RS485_RX_PIN 16
#endif

#ifndef RS485_TX_PIN
#define RS485_TX_PIN 17
#endif

#ifndef PRINT_DEFAULT_MODE
#define PRINT_DEFAULT_MODE 1
#endif

#ifndef MODE_BUTTON_PIN
#define MODE_BUTTON_PIN 0
#endif

#ifndef MODE_BUTTON_ACTIVE_LOW
#define MODE_BUTTON_ACTIVE_LOW 1
#endif

// Touch pin for Modbus Scanner (ESP32 specific)
#ifndef TOUCH_SCANNER_PIN
#define TOUCH_SCANNER_PIN 4 // GPIO4 is Touch0 on many ESP32s
#endif

#ifndef TOUCH_THRESHOLD
#define TOUCH_THRESHOLD 30 // Adjust based on your specific ESP32 board
#endif
// ----------------------------------------------------------------------------------

static PrintController printer(Serial, true,
                               (PrintController::Verbosity)PRINT_DEFAULT_MODE);
static RS485Bus bus;

// ESP32: use UART1 (custom pins). Mega2560: use Serial2 by default.
#if defined(ARDUINO_ARCH_ESP32)
static HardwareSerial RS485Serial(1);
#endif

// Soil & Leaf sensors
static SoilSensor7in1 soil(bus, 0x01); // Default to 0x01
static LeafSensor leaf(bus, 0x02); // Defaulting Leaf to 0x02, adjust if known
static RikaLeafSensor rikaLeaf(bus, 0x03); // Defaulting Rika Leaf to 0x03
static bool buttonPressed() {
  pinMode(MODE_BUTTON_PIN, INPUT_PULLUP);

  int v = digitalRead(MODE_BUTTON_PIN);
  if (MODE_BUTTON_ACTIVE_LOW)
    return (v == LOW);
  return (v == HIGH);
}

// ----------------- Scanner Logic -----------------
static bool isTouchScannerTriggered() {
#if defined(ARDUINO_ARCH_ESP32)
  uint16_t touchValue = touchRead(TOUCH_SCANNER_PIN);
  // Lower value usually means touched on default ESP32 touch pins
  if (touchValue < TOUCH_THRESHOLD) {
    return true;
  }
#endif
  return false;
}

static void runModbusScanner() {
  printer.logln(PrintController::SUMMARY,
                F("\n=================================="));
  printer.logln(PrintController::SUMMARY,
                F("    MODBUS ADDRESS SCANNER START  "));
  printer.logln(PrintController::SUMMARY,
                F("==================================\n"));

  printer.logln(PrintController::SUMMARY, F("Scanning addresses 1 to 252..."));

  int foundCount = 0;

  for (uint8_t addr = 1; addr <= 252; addr++) {
    // Attempt a basic read of 1 register at 0x0000.
    // Most sensors respond to 0x03 if they exist, even if register 0 is
    // undocumented.
    uint8_t tx[8] = {addr, 0x03, 0x00, 0x00, 0x00, 0x01, 0, 0};
    uint16_t crcReq = RS485Bus::crc16Modbus(tx, 6);
    tx[6] = crcReq & 0xFF;
    tx[7] = (crcReq >> 8) & 0xFF;

    uint8_t prefix[3] = {addr, 0x03, 0x02}; // expecting 2 bytes payload
    uint8_t rxFrame[7];                     // 3 prefix + 2 data + 2 crc

    // Fast timeout for scanning to save time (50ms wait per address)
    bool ok = bus.transferAndExtractFixedFrame(tx, 8, prefix, 3, 7, rxFrame,
                                               sizeof(rxFrame), 50, 10);

    if (ok) {
      printer.log(PrintController::SUMMARY,
                  F(">>> FOUND DEVICE AT ADDRESS: 0x"));
      if (addr < 16)
        printer.print("0");
      printer.println(addr, HEX);
      foundCount++;
    }
  }

  printer.logln(PrintController::SUMMARY,
                F("\n=================================="));
  printer.log(PrintController::SUMMARY, F("SCAN COMPLETE. Found "));
  printer.print(foundCount);
  printer.println(F(" devices."));
  printer.logln(PrintController::SUMMARY,
                F("==================================\n"));

  // Anti-bounce delay after scan
  delay(2000);
}
// -------------------------------------------------

static void printSoilData(const SoilSensor7in1_Data &d) {
  if (!printer.isEnabled())
    return;

  Serial.println();
  Serial.println("===== SOIL SENSOR =====");
  Serial.print("Temp (C): ");
  Serial.println(d.temperatureC, 1);
  Serial.print("Hum  (%): ");
  Serial.println(d.humidityPct, 1);
  Serial.print("EC (uS/cm): ");
  Serial.println(d.conductivity_uScm);
  Serial.print("pH: ");
  Serial.println(d.pH, 2);
  Serial.print("N (mg/kg): ");
  Serial.println(d.nitrogen_mgkg);
  Serial.print("P (mg/kg): ");
  Serial.println(d.phosphorus_mgkg);
  Serial.print("K (mg/kg): ");
  Serial.println(d.potassium_mgkg);
}

static void printLeafData(const LeafSensor_Data &d) {
  if (!printer.isEnabled())
    return;

  Serial.println();
  Serial.println("===== LEAF SENSOR =====");
  Serial.print("Temp (C): ");
  Serial.println(d.temperatureC, 1);
  Serial.print("Hum  (%): ");
  Serial.println(d.humidityPct, 1);
}

static void printRikaLeafData(const RikaLeafSensor_Data &d) {
  if (!printer.isEnabled())
    return;

  Serial.println();
  Serial.println("===== RIKA LEAF SENSOR =====");
  Serial.print("Temp (C): ");
  Serial.println(d.rika_leaf_temp, 1);
  Serial.print("Hum  (%): ");
  Serial.println(d.rika_leaf_humid, 1);
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(1000);

  printer.logln(PrintController::SUMMARY,
                F("BOOT: Starting project (Soil & Leaf Sensors)"));

  // Setup mode button
  pinMode(MODE_BUTTON_PIN, INPUT_PULLUP);

  // Setup RS485
  bus.setLogger(&printer);
#if defined(ARDUINO_ARCH_ESP32)
  bus.begin(RS485Serial, RS485_BAUD, RS485_RX_PIN, RS485_TX_PIN);
  bus.setDirectionControl(RS485_DE_PIN, true);
#else
  // Mega2560: Serial2 is available
  bus.begin(Serial2, RS485_BAUD);
  bus.setDirectionControl(RS485_DE_PIN, true);
#endif

  printer.logln(
      PrintController::SUMMARY,
      F("BOOT: Hold the mode button to toggle debug mode at runtime."));
#if defined(ARDUINO_ARCH_ESP32)
  printer.logln(PrintController::SUMMARY,
                F("BOOT: Touch PIN 4 to run Modbus Address Scanner."));
#endif
}

void loop() {
  // Check Address Scanner (Touch Pin)
  if (isTouchScannerTriggered()) {
    runModbusScanner();
    return;
  }

  // Cycle debug mode on button press (simple debounce)
  static uint32_t lastBtnMs = 0;
  if (buttonPressed() && (millis() - lastBtnMs) > 350) {
    lastBtnMs = millis();
    // Toggle 0 (SUMMARY) -> 1 (IO) -> 2 (TRACE) -> 0
    uint8_t curr = (uint8_t)printer.verbosity();
    curr = (curr + 1) % 3;
    printer.setVerbosity((PrintController::Verbosity)curr);
    printer.log(PrintController::SUMMARY, F("Verbosity changed to: "));
    printer.println(curr);
  }

  // --- Read Soil Sensor ---
  SoilSensor7in1_Data soilData{};
  bool soilOk = soil.readAll(soilData);

  if (soilOk) {
    printSoilData(soilData);
  } else {
    printer.logln(PrintController::SUMMARY,
                  F("SOIL: Read failed or out-of-bounds. Check address, "
                    "wiring, or ranges."));
  }

  delay(500); // Small separation between sensor reads

  // --- Read Leaf Sensor ---
  LeafSensor_Data leafData{};
  bool leafOk = leaf.readAll(leafData);

  if (leafOk) {
    printLeafData(leafData);
  } else {
    printer.logln(PrintController::SUMMARY,
                  F("LEAF: Read failed or out-of-bounds. Check address, "
                    "wiring, or ranges."));
  }

  delay(500);

  // --- Read Rika Leaf Sensor ---
  RikaLeafSensor_Data rikaData{};
  bool rikaOk = rikaLeaf.readAll(rikaData);

  if (rikaOk) {
    printRikaLeafData(rikaData);
  } else {
    printer.logln(PrintController::SUMMARY,
                  F("RIKA LEAF: Read failed or out-of-bounds. Check address, "
                    "wiring, or ranges."));
  }

  delay(2000);
}
