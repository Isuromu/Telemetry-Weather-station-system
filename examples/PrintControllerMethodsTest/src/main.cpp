#include "PrintController.h"
#include <Arduino.h>

// Global instance of PrintController
PrintController printer(Serial, true); // Initially enabled

// Helper function to print a separator
void printSeparator() {
    printer.println(F("--------------------------------------------------"), true);
}

// Test function for various data types
void testDataTypes() {
    printer.println(F("Test: Data Types"), true);
    printer.print(F("Integer: "), true, "\t");
    printer.println(42, true);
    printer.print(F("Unsigned Integer: "), true, "\t");
    printer.println(42u, true);
    printer.print(F("Long: "), true, "\t");
    printer.println(42L, true);
    printer.print(F("Unsigned Long: "), true, "\t");
    printer.println(42UL, true);
    printer.print(F("Float (2 decimal places): "), true, "\t");
    printer.println(3.14159, true, "", 2);
    printer.print(F("Double (3 decimal places): "), true, "\t");
    printer.println(2.718281828, true, "", 3);
    printer.print(F("Character: "), true, "\t");
    printer.println('A', true);
    printer.print(F("String: "), true, "\t");
    printer.println("Hello, World!", true);
    printer.print(F("Flash String: "), true, "\t");
    printer.println(F("Stored in Flash"), true);
    printSeparator();
}

// Test function for number bases
void testNumberBases() {
    printer.println(F("Test: Number Bases"), true);
    int number = 255;
    printer.print(F("Decimal: "), true, "\t");
    printer.println(number, true, "", DEC);
    printer.print(F("Hexadecimal: "), true, "\t");
    printer.println(number, true, "", HEX);
    printer.print(F("Octal: "), true, "\t");
    printer.println(number, true, "", OCT);
    printer.print(F("Binary: "), true, "\t");
    printer.println(number, true, "", BIN);
    printSeparator();
}

// Test function for end characters
void testEndCharacters() {
    printer.println(F("Test: End Characters"), true);
    printer.print(F("No endChar"), true);
    printer.print(F(" - Space endChar"), true, " ");
    printer.print(F("- Tab endChar"), true, "\t");
    printer.println(F("- Newline endChar"), true, "\n");
    printSeparator();
}

// Test function for enable/disable functionality
void testEnableDisable() {
    printer.println(F("Test: Enable/Disable Functionality"), true);
    printer.setEnable(false);
    printer.println(F("This should NOT print.")); // Should not print
    printer.println(F("This should print (override)."), true); // Should print
    printer.setEnable(true);
    printer.println(F("Printing re-enabled."), true);
    printSeparator();
}

// Sample class to test integration with PrintController
class Sensor {
private:
    PrintController& printer;

public:
    Sensor(PrintController& p) : printer(p) {}

    void displayReadings() {
        printer.println(F("Sensor Readings:"), true);
        printer.print(F("Temperature: "), true, "\t");
        printer.println(25.6, true, " C\n", 1);
        printer.print(F("Humidity: "), true, "\t");
        printer.println(60, true, " %\n");
    }
};

// Test function for class integration
void testClassIntegration() {
    printer.println(F("Test: Class Integration"), true);
    Sensor sensor(printer);
    sensor.displayReadings();
    printSeparator();
}

void setup() {
    Serial.begin(9600);
    while (!Serial) {
        ; // Wait for serial port to connect. Needed for native USB port only
    }

    // Run all test functions
    testDataTypes();
    testNumberBases();
    testEndCharacters();
    testEnableDisable();
    testClassIntegration();
}

void loop() {
    // No repeated actions needed
}
