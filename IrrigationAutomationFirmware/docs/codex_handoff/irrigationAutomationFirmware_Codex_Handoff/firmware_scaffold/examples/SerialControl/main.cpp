#include <Arduino.h>
#include <Wire.h>

#include "BoardPins.hpp"
#include "SystemConfig.hpp"
#include "BatteryMonitor.hpp"
#include "PressureSensorXDB401.hpp"
#include "PressureControlValve.hpp"

TwoWire upstreamI2c(0);
TwoWire downstreamI2c(1);

BatteryMonitor battery(
    BoardPins::BATTERY_ADC,
    SystemConfig::Battery::DIVIDER_HIGH_OHM,
    SystemConfig::Battery::DIVIDER_LOW_OHM,
    SystemConfig::Battery::CALIBRATION,
    SystemConfig::Battery::SAMPLE_COUNT
);

PressureSensorXDB401 upstreamPressure(
    upstreamI2c,
    SystemConfig::PressureSensor::ADDRESS_PRIMARY,
    SystemConfig::PressureSensor::ADDRESS_ALTERNATE,
    SystemConfig::PressureSensor::FULL_SCALE_BAR
);

PressureSensorXDB401 downstreamPressure(
    downstreamI2c,
    SystemConfig::PressureSensor::ADDRESS_PRIMARY,
    SystemConfig::PressureSensor::ADDRESS_ALTERNATE,
    SystemConfig::PressureSensor::FULL_SCALE_BAR
);

PressureControlValve pressureControlValve(
    BoardPins::PCV_IN1,
    BoardPins::PCV_IN2,
    BoardPins::PCV_POWER_ENABLE,
    SystemConfig::PressureControlValve::POWER_ENABLE_ACTIVE_HIGH,
    SystemConfig::PressureControlValve::POWER_SETTLE_MS,
    SystemConfig::PressureControlValve::SOLENOID_PULSE_MS,
    SystemConfig::PressureControlValve::POST_PULSE_MS,
    SystemConfig::PressureControlValve::OPEN_IN1_HIGH,
    SystemConfig::PressureControlValve::OPEN_IN2_HIGH,
    SystemConfig::PressureControlValve::CLOSE_IN1_HIGH,
    SystemConfig::PressureControlValve::CLOSE_IN2_HIGH
);

String commandBuffer;

void printReading(const char* label,
                  const PressureSensorXDB401& sensor,
                  const XDB401Reading& reading) {
    if (!sensor.isPresent()) {
        Serial.printf("%s: SENSOR NOT FOUND\n", label);
        return;
    }

    if (!reading.valid) {
        Serial.printf("%s: READ ERROR at 0x%02X\n",
                      label,
                      sensor.address());
        return;
    }

    Serial.printf("%s: %.3f bar | %.2f C | address 0x%02X\n",
                  label,
                  reading.pressureBar,
                  reading.temperatureC,
                  sensor.address());
}

void printPressure() {
    const XDB401Reading upstream = upstreamPressure.read();
    const XDB401Reading downstream = downstreamPressure.read();

    printReading("Pressure UPSTREAM  ", upstreamPressure, upstream);
    printReading("Pressure DOWNSTREAM", downstreamPressure, downstream);

    if (upstream.valid && downstream.valid) {
        Serial.printf("Pressure DROP      : %.3f bar\n",
                      upstream.pressureBar - downstream.pressureBar);
    }
}

void printStatus() {
    Serial.println();
    Serial.println("========== IRRIGATION NODE STATUS ==========");
    Serial.printf("PCV last commanded state: %s\n",
                  PressureControlValve::stateName(
                      pressureControlValve.lastCommandedState()));
    Serial.printf("Battery: %.2f V\n", battery.readVoltage());
    printPressure();
    Serial.println("============================================");
}

void printHelp() {
    Serial.println("Commands:");
    Serial.println("  help       - show this command list");
    Serial.println("  status     - battery, pressure and PCV commanded state");
    Serial.println("  battery    - read battery voltage");
    Serial.println("  pressure   - read upstream/downstream pressure");
    Serial.println("  pcv open   - send OPEN latching pulse");
    Serial.println("  pcv close  - send CLOSE latching pulse");
    Serial.println("  pcv state  - show last commanded PCV state");
}

void executeCommand(String input) {
    input.trim();
    input.toLowerCase();

    if (input == "help" || input == "?") {
        printHelp();
    } else if (input == "status") {
        printStatus();
    } else if (input == "battery") {
        Serial.printf("Battery: %.2f V\n", battery.readVoltage());
    } else if (input == "pressure") {
        printPressure();
    } else if (input == "pcv open") {
        Serial.println("PCV: sending OPEN pulse...");
        pressureControlValve.commandOpen();
        delay(SystemConfig::PressureControlValve::HYDRAULIC_SETTLE_MS);
        Serial.println("PCV: OPEN pulse complete.");
        printPressure();
    } else if (input == "pcv close") {
        Serial.println("PCV: sending CLOSE pulse...");
        pressureControlValve.commandClose();
        delay(SystemConfig::PressureControlValve::HYDRAULIC_SETTLE_MS);
        Serial.println("PCV: CLOSE pulse complete.");
        printPressure();
    } else if (input == "pcv state") {
        Serial.printf("PCV last commanded state: %s\n",
                      PressureControlValve::stateName(
                          pressureControlValve.lastCommandedState()));
    } else if (!input.isEmpty()) {
        Serial.printf("Unknown command: %s\n", input.c_str());
        Serial.println("Type 'help' for commands.");
    }
}

void readSerialCommands() {
    while (Serial.available() > 0) {
        const char c = static_cast<char>(Serial.read());

        if (c == '\r' || c == '\n') {
            if (!commandBuffer.isEmpty()) {
                executeCommand(commandBuffer);
                commandBuffer = "";
            }
        } else if (commandBuffer.length() <
                   SystemConfig::SerialConsole::COMMAND_BUFFER_LENGTH) {
            commandBuffer += c;
        }
    }
}

void setup() {
    pressureControlValve.begin();

    Serial.begin(SystemConfig::SerialConsole::BAUD);
    delay(500);

    battery.begin();

    upstreamI2c.begin(
        BoardPins::I2C_UPSTREAM_SDA,
        BoardPins::I2C_UPSTREAM_SCL,
        SystemConfig::PressureSensor::I2C_FREQUENCY_HZ
    );

    downstreamI2c.begin(
        BoardPins::I2C_DOWNSTREAM_SDA,
        BoardPins::I2C_DOWNSTREAM_SCL,
        SystemConfig::PressureSensor::I2C_FREQUENCY_HZ
    );

    const bool upstreamFound = upstreamPressure.begin();
    const bool downstreamFound = downstreamPressure.begin();

    commandBuffer.reserve(
        SystemConfig::SerialConsole::COMMAND_BUFFER_LENGTH
    );

    Serial.println();
    Serial.println("Irrigation pressure-control node ready.");
    Serial.printf("Upstream sensor: %s",
                  upstreamFound ? "FOUND" : "NOT FOUND");
    if (upstreamFound) {
        Serial.printf(" at 0x%02X", upstreamPressure.address());
    }
    Serial.println();

    Serial.printf("Downstream sensor: %s",
                  downstreamFound ? "FOUND" : "NOT FOUND");
    if (downstreamFound) {
        Serial.printf(" at 0x%02X", downstreamPressure.address());
    }
    Serial.println();

    printHelp();
    printStatus();
}

void loop() {
    readSerialCommands();
}
