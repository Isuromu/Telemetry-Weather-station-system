#include "PressureSensorXDB401.hpp"

PressureSensorXDB401::PressureSensorXDB401(TwoWire& bus,
                                           uint8_t primaryAddress,
                                           uint8_t alternateAddress,
                                           float fullScaleBar)
    : _bus(bus),
      _primaryAddress(primaryAddress),
      _alternateAddress(alternateAddress),
      _address(0),
      _fullScaleBar(fullScaleBar) {}

bool PressureSensorXDB401::begin() {
    if (devicePresent(_primaryAddress)) {
        _address = _primaryAddress;
        return true;
    }

    if (devicePresent(_alternateAddress)) {
        _address = _alternateAddress;
        return true;
    }

    _address = 0;
    return false;
}

bool PressureSensorXDB401::isPresent() const {
    return _address != 0;
}

uint8_t PressureSensorXDB401::address() const {
    return _address;
}

bool PressureSensorXDB401::devicePresent(uint8_t address) {
    _bus.beginTransmission(address);
    return _bus.endTransmission() == 0;
}

bool PressureSensorXDB401::writeRegister(uint8_t reg, uint8_t value) {
    if (_address == 0) {
        return false;
    }

    _bus.beginTransmission(_address);
    _bus.write(reg);
    _bus.write(value);
    return _bus.endTransmission() == 0;
}

bool PressureSensorXDB401::readRegister(uint8_t reg,
                                        uint8_t* data,
                                        size_t length) {
    if (_address == 0 || data == nullptr || length == 0 || length > 255) {
        return false;
    }

    _bus.beginTransmission(_address);
    _bus.write(reg);

    if (_bus.endTransmission(false) != 0) {
        return false;
    }

    const size_t received =
        _bus.requestFrom(_address, static_cast<uint8_t>(length));

    if (received != length) {
        return false;
    }

    for (size_t i = 0; i < length; ++i) {
        data[i] = _bus.read();
    }

    return true;
}

XDB401Reading PressureSensorXDB401::read() {
    XDB401Reading result{false, 0.0F, 0.0F};

    if (_address == 0) {
        return result;
    }

    if (!writeRegister(REG_MEASURE, CMD_MEASURE)) {
        return result;
    }

    bool measurementReady = false;

    for (uint8_t attempt = 0; attempt < 10; ++attempt) {
        delay(5);

        uint8_t status = 0;
        if (!readRegister(REG_MEASURE, &status, 1)) {
            return result;
        }

        // Current working interpretation from the trainee code:
        // bit 3 clear means measurement ready.
        if ((status & 0x08U) == 0U) {
            measurementReady = true;
            break;
        }
    }

    if (!measurementReady) {
        return result;
    }

    uint8_t pressureData[3] = {0};
    uint8_t temperatureData[2] = {0};

    if (!readRegister(REG_PRESSURE, pressureData, sizeof(pressureData))) {
        return result;
    }

    if (!readRegister(REG_TEMPERATURE,
                      temperatureData,
                      sizeof(temperatureData))) {
        return result;
    }

    const uint32_t raw24 =
        (static_cast<uint32_t>(pressureData[0]) << 16) |
        (static_cast<uint32_t>(pressureData[1]) << 8) |
        static_cast<uint32_t>(pressureData[2]);

    int32_t rawPressure = 0;
    if ((raw24 & 0x800000UL) != 0UL) {
        rawPressure = static_cast<int32_t>(raw24 | 0xFF000000UL);
    } else {
        rawPressure = static_cast<int32_t>(raw24);
    }

    const int16_t rawTemperature = static_cast<int16_t>(
        (static_cast<uint16_t>(temperatureData[0]) << 8) |
        static_cast<uint16_t>(temperatureData[1])
    );

    // These conversions reproduce the trainee firmware's current interpretation.
    // Validate them against the exact XDB401/S1204 datasheet before production.
    result.pressureBar =
        static_cast<float>(rawPressure) /
        8388608.0F *
        _fullScaleBar;

    result.temperatureC =
        static_cast<float>(rawTemperature) / 256.0F;

    result.valid = true;
    return result;
}
