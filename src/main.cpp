#include <Arduino.h>
#include <stdint.h>
#include "PrintController.h"
#include "RS485Modbus.h"
#include "SoilTR.h"

// -------------------- Compile-time settings from platformio.ini --------------------
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
// ----------------------------------------------------------------------------------

static PrintController printer;
static RS485Modbus bus;

// ESP32: use UART1 (custom pins). Mega2560: use Serial2 by default.
#if defined(ARDUINO_ARCH_ESP32)
static HardwareSerial RS485Serial(1);
#endif

// Soil sensor default address (your manual examples often show 0x01)
static SoilTR soil(bus, 0x01);

static bool buttonPressed() {
  pinMode(MODE_BUTTON_PIN, INPUT_PULLUP);

  int v = digitalRead(MODE_BUTTON_PIN);
  if (MODE_BUTTON_ACTIVE_LOW) return (v == LOW);
  return (v == HIGH);
}

static void printSoilData(const SoilTR_Data& d) {
  if (!printer.isNormalOrHigher()) return;

  Serial.println();
  Serial.println("===== SOIL SENSOR =====");
  Serial.print("Temp (C): "); Serial.println(d.temperatureC, 1);
  Serial.print("Hum  (%): "); Serial.println(d.humidityPct, 1);
  Serial.print("EC (uS/cm): "); Serial.println(d.conductivity_uScm);
  Serial.print("pH: "); Serial.println(d.pH, 2);
  Serial.print("N (mg/kg): "); Serial.println(d.nitrogen_mgkg);
  Serial.print("P (mg/kg): "); Serial.println(d.phosphorus_mgkg);
  Serial.print("K (mg/kg): "); Serial.println(d.potassium_mgkg);
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(300);

  // Setup printer
  printer.begin(&Serial, (PrintController::Mode)PRINT_DEFAULT_MODE);
  printer.info("BOOT", "Starting project (Soil sensor only, no MQTT/Wi-Fi)");

  // Setup mode button
  pinMode(MODE_BUTTON_PIN, INPUT_PULLUP);

  // Setup RS485
#if defined(ARDUINO_ARCH_ESP32)
  bus.setPrinter(&printer);
  bus.begin(RS485Serial, RS485_BAUD, RS485_RX_PIN, RS485_TX_PIN, RS485_DE_PIN, true);
#else
  // Mega2560: Serial2 is available on RX2=17 TX2=16
  bus.setPrinter(&printer);
  bus.begin(Serial2, RS485_BAUD, RS485_DE_PIN, true);
#endif

  printer.info("BOOT", "Hold the mode button to cycle debug mode at runtime.");
}

void loop() {
  // Cycle debug mode on button press (simple debounce)
  static uint32_t lastBtnMs = 0;
  if (buttonPressed() && (millis() - lastBtnMs) > 350) {
    lastBtnMs = millis();
    printer.cycleMode();
  }

  SoilTR_Data data{};
  bool ok = soil.readAll(data);

  if (!ok) {
    printer.error("SOIL", "Read failed");
    delay(800);
    return;
  }

  printSoilData(data);

  delay(2000);
}
