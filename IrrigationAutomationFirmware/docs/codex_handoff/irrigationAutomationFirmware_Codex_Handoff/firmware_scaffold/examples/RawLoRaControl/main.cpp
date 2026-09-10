#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <RadioLib.h>

#include "BoardPins.hpp"
#include "SystemConfig.hpp"
#include "BatteryMonitor.hpp"
#include "PressureSensorXDB401.hpp"
#include "PressureControlValve.hpp"

TwoWire upstreamI2c(0);
TwoWire downstreamI2c(1);

SPIClass loraSpi(VSPI);

SX1262 radio = new Module(
    BoardPins::LORA_NSS,
    BoardPins::LORA_DIO1,
    BoardPins::LORA_NRST,
    BoardPins::LORA_BUSY,
    loraSpi
);

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

String buildStatusText() {
    const XDB401Reading upstream = upstreamPressure.read();
    const XDB401Reading downstream = downstreamPressure.read();

    String response;
    response.reserve(160);

    response += "STATUS;PCV=";
    response += PressureControlValve::stateName(
        pressureControlValve.lastCommandedState()
    );

    response += ";BAT=";
    response += String(battery.readVoltage(), 2);

    response += ";P_UP=";
    response += upstream.valid ? String(upstream.pressureBar, 3) : "ERR";

    response += ";P_DN=";
    response += downstream.valid ? String(downstream.pressureBar, 3) : "ERR";

    return response;
}

String executeRadioCommand(String command) {
    command.trim();
    command.toUpperCase();

    if (command == "OPEN") {
        pressureControlValve.commandOpen();
        delay(SystemConfig::PressureControlValve::HYDRAULIC_SETTLE_MS);
        return String("ACK;OPEN;") + buildStatusText();
    }

    if (command == "CLOSE") {
        pressureControlValve.commandClose();
        delay(SystemConfig::PressureControlValve::HYDRAULIC_SETTLE_MS);
        return String("ACK;CLOSE;") + buildStatusText();
    }

    if (command == "STATUS") {
        return buildStatusText();
    }

    return String("NACK;UNKNOWN_COMMAND");
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

    upstreamPressure.begin();
    downstreamPressure.begin();

    loraSpi.begin(
        BoardPins::LORA_SCK,
        BoardPins::LORA_MISO,
        BoardPins::LORA_MOSI,
        BoardPins::LORA_NSS
    );

    int state = radio.begin(
        SystemConfig::RawLoRaBench::FREQUENCY_MHZ,
        SystemConfig::RawLoRaBench::BANDWIDTH_KHZ,
        SystemConfig::RawLoRaBench::SPREADING_FACTOR,
        SystemConfig::RawLoRaBench::CODING_RATE,
        SystemConfig::RawLoRaBench::SYNC_WORD,
        SystemConfig::RawLoRaBench::OUTPUT_POWER_DBM,
        SystemConfig::RawLoRaBench::PREAMBLE_LENGTH
    );

    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("LoRa init failed: %d\n", state);
        while (true) {
            delay(1000);
        }
    }

    radio.setRfSwitchPins(
        BoardPins::LORA_RXEN,
        BoardPins::LORA_TXEN
    );

    Serial.println("Raw LoRa PCV control example ready.");
    Serial.println("Commands over radio: OPEN, CLOSE, STATUS");
}

void loop() {
    String incoming;

    const int state = radio.receive(incoming);

    if (state == RADIOLIB_ERR_NONE) {
        Serial.printf("LoRa RX: %s\n", incoming.c_str());

        const String response = executeRadioCommand(incoming);

        const int txState = radio.transmit(response);
        if (txState == RADIOLIB_ERR_NONE) {
            Serial.printf("LoRa TX: %s\n", response.c_str());
        } else {
            Serial.printf("LoRa TX failed: %d\n", txState);
        }
    } else if (state != RADIOLIB_ERR_RX_TIMEOUT) {
        Serial.printf("LoRa RX error: %d\n", state);
    }
}
