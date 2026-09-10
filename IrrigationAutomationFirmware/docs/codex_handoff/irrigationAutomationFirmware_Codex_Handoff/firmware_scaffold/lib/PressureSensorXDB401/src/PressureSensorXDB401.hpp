#pragma once

#include <Arduino.h>
#include <Wire.h>

struct XDB401Reading {
    bool valid;
    float pressureBar;
    float temperatureC;
};

class PressureSensorXDB401 {
public:
    PressureSensorXDB401(TwoWire& bus,
                         uint8_t primaryAddress,
                         uint8_t alternateAddress,
                         float fullScaleBar);

    bool begin();
    bool isPresent() const;
    uint8_t address() const;
    XDB401Reading read();

private:
    static constexpr uint8_t REG_PRESSURE = 0x06;
    static constexpr uint8_t REG_TEMPERATURE = 0x09;
    static constexpr uint8_t REG_MEASURE = 0x30;
    static constexpr uint8_t CMD_MEASURE = 0x0A;

    TwoWire& _bus;
    uint8_t _primaryAddress;
    uint8_t _alternateAddress;
    uint8_t _address;
    float _fullScaleBar;

    bool devicePresent(uint8_t address);
    bool writeRegister(uint8_t reg, uint8_t value);
    bool readRegister(uint8_t reg, uint8_t* data, size_t length);
};
