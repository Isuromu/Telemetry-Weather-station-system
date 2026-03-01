#include "PrintController.h"
#include "RS485Modbus.h"
#include "TVOCSensor.h"
#include "config.h"
#include <Arduino.h>


#if defined(ARDUINO_ARCH_ESP32)
HardwareSerial RS485Serial(1);
#endif

static PrintController
    printer(Serial, static_cast<bool>(PRINT_ENABLED),
            static_cast<PrintController::Verbosity>(PRINT_VERBOSITY));
static RS485Bus bus;
static uint8_t targetAddress = SENSOR_DEFAULT_ADDRESS;
static TVOCSensor sensor(bus, targetAddress);

void scanAddresses() {
  printer.logln(PrintController::TRACE,
                F("\n--- Modbus Address Scan (1-247) ---"));
  uint8_t found = 0;
  for (uint8_t addr = 1; addr <= 247; addr++) {
    uint8_t req[8];
    req[0] = addr;
    req[1] = 0x03;
    req[2] = 0x00;
    req[3] = 0x00;
    req[4] = 0x00;
    req[5] = 0x01;
    uint16_t crc = RS485Bus::crc16Modbus(req, 6);
    req[6] = crc & 0xFF;
    req[7] = crc >> 8;
    if (bus.transferRaw(req, 8, SCAN_TIMEOUT_MS)) {
      printer.log(PrintController::TRACE, F("  Found at 0x"));
      if (addr < 16)
        printer.print(F("0"));
      printer.println(addr, HEX);
      found++;
    }
  }
  if (found == 0)
    printer.logln(PrintController::TRACE, F("  No devices found."));
  printer.logln(PrintController::TRACE, F("--- Scan Complete ---\n"));
  bus.flushInput();
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(500);
  printer.println(F(""));
  printer.println(F("================================"));
  printer.println(F("  FIRMWARE: TVOC Sensor Example"));
  printer.println(F("  Program starts here (reset point)."));
  printer.println(F("================================"));
  pinMode(SCAN_BUTTON_PIN, INPUT_PULLUP);
  bus.setLogger(&printer);
#if defined(ARDUINO_ARCH_ESP32)
  bus.begin(RS485Serial, RS485_BAUD, RS485_RX_PIN, RS485_TX_PIN);
#else
  bus.begin(Serial2, RS485_BAUD);
#endif
  bus.setDirectionControl(RS485_DE_PIN, true);
  printer.logln(PrintController::TRACE,
                F("Hold SCAN button (GPIO14) within 3 s for address scan..."));
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
    printer.logln(PrintController::TRACE,
                  F("No button press — normal operation.\n"));
  }
}

void loop() {
  static uint32_t lastPoll = 0;
  if (millis() - lastPoll >= static_cast<uint32_t>(POLL_INTERVAL_MS) ||
      lastPoll == 0) {
    lastPoll = millis();
    TVOCSensor_Data data{};
    printer.logln(PrintController::TRACE, F("Polling TVOC Sensor..."));
    if (sensor.readAll(data)) {
      printer.logln(PrintController::TRACE,
                    F("\n===== TVOC AIR QUALITY SENSOR ====="));
      printer.log(PrintController::TRACE, F("TVOC        : "));
      printer.print(data.tvoc_ppb);
      printer.println(F(" ppb"));
      printer.log(PrintController::TRACE, F("CO2         : "));
      printer.print(data.co2_ppm);
      printer.println(F(" ppm"));
      printer.log(PrintController::TRACE, F("PM2.5       : "));
      printer.print(data.pm25_ug_m3);
      printer.println(F(" ug/m3"));
      printer.log(PrintController::TRACE, F("PM10        : "));
      printer.print(data.pm10_ug_m3);
      printer.println(F(" ug/m3"));
      printer.log(PrintController::TRACE, F("Temperature : "));
      printer.print(data.temperature_c, 1);
      printer.println(F(" C"));
      printer.log(PrintController::TRACE, F("Humidity    : "));
      printer.print(data.humidity_pct, 1);
      printer.println(F(" %RH"));
      printer.logln(PrintController::TRACE,
                    F("===================================\n"));
    } else {
      printer.logln(PrintController::TRACE, F("ERROR: Read failed."));
    }
  }
}
