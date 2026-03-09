#include "PrintController.h"
#include "RS485Modbus.h"
#include "SoilSensor7in1.h"
#include "config.h"
#include <Arduino.h>

// All settings are in config.h — do NOT hard-code values here.

#if defined(ARDUINO_ARCH_ESP32)
HardwareSerial RS485Serial(1);
#endif

static PrintController printer(Serial, static_cast<bool>(DEBUG_ENABLED));
static RS485Bus bus;
static uint8_t targetAddress = SENSOR_DEFAULT_ADDRESS;
static SoilSensor7in1 sensor(bus, targetAddress);

void scanAddresses() {
  if (DEBUG_ENABLED && DEBUG_LEVEL >= 2) {
    printer.println(F("\n--- Modbus Address Scan (1-247) ---"));
  }
  uint8_t found = 0;
  for (uint16_t addr = 1; addr <= 247; addr++) {
    if (DEBUG_ENABLED && DEBUG_LEVEL >= 2) {
    printer.print(F("[Scanner] Trying address 0x"));
  }
    if (addr < 16)
      printer.print(F("0"));
    printer.print(addr, HEX);
    printer.print(F("... "));

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
    if (bus.transferRaw(req, 8, SCAN_TIMEOUT_MS)) {
      printer.println(F("FOUND!"));
      found++;
    } else {
      printer.println(F("no response"));
    }
  }
  if (found == 0)
    if (DEBUG_ENABLED && DEBUG_LEVEL >= 2) {
    printer.println(F("  No devices found."));
  }
  if (DEBUG_ENABLED && DEBUG_LEVEL >= 2) {
    printer.println(F("--- Scan Complete ---\n"));
  }
  bus.flushInput();
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(500);

  // ===== START: Firmware identification banner (shown on every reset) =====
  printer.println(F(""));
  printer.println(F("==========================================="));
  printer.println(F("  FIRMWARE: Soil Sensor 7-in-1 Example"));
  printer.println(F("  Program starts here (reset point)."));
  printer.println(F("==========================================="));
  // ===== END: Banner =====

  pinMode(SCAN_BUTTON_PIN, INPUT_PULLUP);
  bus.setDebug(&printer, static_cast<bool>(DEBUG_ENABLED), static_cast<uint8_t>(DEBUG_LEVEL));

#if defined(ARDUINO_ARCH_ESP32)
  bus.begin(RS485Serial, RS485_BAUD, RS485_RX_PIN, RS485_TX_PIN);
#else
  bus.begin(Serial2, RS485_BAUD);
#endif
  bus.setDirectionControl(RS485_DE_PIN, true);

  if (DEBUG_ENABLED && DEBUG_LEVEL >= 2) {
    printer.println(F("Hold SCAN button (GPIO14) within 3 s for address scan..."));
  }
  uint32_t deadline = millis() + static_cast<uint32_t>(SCAN_WINDOW_MS);
  bool scan = false;
  while (millis() < deadline) {
    if (digitalRead(SCAN_BUTTON_PIN) == LOW) {
      delay(50);
      if (digitalRead(SCAN_BUTTON_PIN) == LOW) {
        scan = true;
        break;
      }
    }
    delay(10);
  }
  if (scan) {
    scanAddresses();
  } else {
    if (DEBUG_ENABLED && DEBUG_LEVEL >= 2) {
    printer.println(F("No button press — normal operation.\n"));
  }
  }
}

void loop() {
  static uint32_t lastPoll = 0;
  if (millis() - lastPoll >= static_cast<uint32_t>(POLL_INTERVAL_MS) ||
      lastPoll == 0) {
    lastPoll = millis();

    SoilSensor7in1_Data data{};
    if (DEBUG_ENABLED && DEBUG_LEVEL >= 2) {
    printer.println(F("Polling Soil Sensor 7-in-1..."));
  }

    if (sensor.readAll(data)) {
      if (DEBUG_ENABLED && DEBUG_LEVEL >= 2) {
    printer.println(F("\n===== SOIL SENSOR 7-IN-1 DATA ====="));
  }
      if (DEBUG_ENABLED && DEBUG_LEVEL >= 2) {
    printer.print(F("Temperature  : "));
  }
      printer.print(data.temperatureC, 1);
      printer.println(F(" C"));
      if (DEBUG_ENABLED && DEBUG_LEVEL >= 2) {
    printer.print(F("Humidity     : "));
  }
      printer.print(data.humidityPct, 1);
      printer.println(F(" %"));
      if (DEBUG_ENABLED && DEBUG_LEVEL >= 2) {
    printer.print(F("Conductivity : "));
  }
      printer.print(data.conductivity_uScm);
      printer.println(F(" uS/cm"));
      if (DEBUG_ENABLED && DEBUG_LEVEL >= 2) {
    printer.print(F("pH           : "));
  }
      printer.println(data.pH, 2);
      if (DEBUG_ENABLED && DEBUG_LEVEL >= 2) {
    printer.print(F("Nitrogen     : "));
  }
      printer.print(data.nitrogen_mgkg);
      printer.println(F(" mg/kg"));
      if (DEBUG_ENABLED && DEBUG_LEVEL >= 2) {
    printer.print(F("Phosphorus   : "));
  }
      printer.print(data.phosphorus_mgkg);
      printer.println(F(" mg/kg"));
      if (DEBUG_ENABLED && DEBUG_LEVEL >= 2) {
    printer.print(F("Potassium    : "));
  }
      printer.print(data.potassium_mgkg);
      printer.println(F(" mg/kg"));
      if (DEBUG_ENABLED && DEBUG_LEVEL >= 2) {
    printer.println(F("===================================\n"));
  }
    } else {
      if (DEBUG_ENABLED && DEBUG_LEVEL >= 2) {
    printer.println(F("ERROR: Read failed."));
  }
    }
  }
}
