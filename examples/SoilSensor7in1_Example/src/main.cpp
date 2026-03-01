#include "PrintController.h"
#include "RS485Modbus.h"
#include "SoilSensor7in1.h"
#include <Arduino.h>


// -------------------- Compile-time settings --------------------
#define TOUCH_PIN T0 // Change to your ESP32 touch pin (T0 = GPIO 4)
// ----------------------------------------------------------------

PrintController printer(Serial, true, PrintController::TRACE);
RS485Bus bus;

uint8_t targetAddress = 0x01; // Default sensor address
SoilSensor7in1 sensor(bus, targetAddress);

#if defined(ARDUINO_ARCH_ESP32)
HardwareSerial RS485Serial(1);
#endif

void setup() {
  Serial.begin(115200);
  delay(1000);

  printer.println(F("\n=== Soil Sensor 7-in-1 Example ==="));

  bus.setLogger(&printer);

#if defined(ARDUINO_ARCH_ESP32)
  // ESP32: custom pins rx=16, tx=17
  bus.begin(RS485Serial, 9600, 16, 17);
  bus.setDirectionControl(21, true);
#else
  // AVR: Serial2
  bus.begin(Serial2, 9600);
  bus.setDirectionControl(21, true);
#endif

  printer.logln(PrintController::SUMMARY, F("Initialization complete."));
}

void scanAddresses() {
  printer.logln(PrintController::SUMMARY,
                F("\n--- Starting Modbus Address Scan (0-247) ---"));
  for (uint8_t addr = 0; addr <= 247; addr++) {
    uint8_t req[8];
    req[0] = addr;
    req[1] = 0x03; // Read Holding Registers
    req[2] = 0x00;
    req[3] = 0x00; // Start at 0x0000
    req[4] = 0x00;
    req[5] = 0x01; // Read 1 register
    uint16_t crc = RS485Bus::crc16Modbus(req, 6);
    req[6] = crc & 0xFF;
    req[7] = crc >> 8;

    if (bus.transferRaw(req, 8, 100)) { // 100ms timeout
      printer.log(PrintController::SUMMARY, F("Found device at address: 0x"));
      if (addr < 16)
        printer.print(F("0"));
      printer.println(addr, HEX);
    }
  }
  printer.logln(PrintController::SUMMARY, F("--- Scan Complete ---\n"));
  bus.flushInput();
}

void loop() {
#if defined(ARDUINO_ARCH_ESP32)
  // Check if capacitance falls below threshold (touched)
  // Threshold may need adjustment based on your board (some are <40, S3 might
  // be different)
  if (touchRead(TOUCH_PIN) < 40) {
    delay(50); // debounce
    if (touchRead(TOUCH_PIN) < 40) {
      scanAddresses();
      delay(1000); // Prevent multiple scans
    }
  }
#endif

  static uint32_t lastPoll = 0;
  if (millis() - lastPoll >= 3000 || lastPoll == 0) {
    lastPoll = millis();

    printer.logln(PrintController::SUMMARY, F("\nReading Soil Sensor..."));

    SoilSensor7in1_Data data;
    if (sensor.readAll(data)) {
      printer.log(PrintController::SUMMARY, F("Temperature: "));
      printer.print(data.temperatureC);
      printer.println(F(" C"));

      printer.log(PrintController::SUMMARY, F("Humidity: "));
      printer.print(data.humidityPct);
      printer.println(F(" %"));

      printer.log(PrintController::SUMMARY, F("Conductivity: "));
      printer.print(data.conductivity_uScm);
      printer.println(F(" uS/cm"));

      printer.log(PrintController::SUMMARY, F("pH: "));
      printer.print(data.pH);
      printer.println();

      printer.log(PrintController::SUMMARY, F("Nitrogen: "));
      printer.print(data.nitrogen_mgkg);
      printer.println(F(" mg/kg"));

      printer.log(PrintController::SUMMARY, F("Phosphorus: "));
      printer.print(data.phosphorus_mgkg);
      printer.println(F(" mg/kg"));

      printer.log(PrintController::SUMMARY, F("Potassium: "));
      printer.print(data.potassium_mgkg);
      printer.println(F(" mg/kg"));
    } else {
      printer.logln(PrintController::SUMMARY, F("Failed to read sensor."));
    }
  }
}
