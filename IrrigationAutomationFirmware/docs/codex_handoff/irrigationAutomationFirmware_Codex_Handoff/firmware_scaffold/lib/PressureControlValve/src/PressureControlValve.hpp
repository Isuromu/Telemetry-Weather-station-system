#pragma once

#include <Arduino.h>

enum class PressureControlValveState : uint8_t {
    Unknown,
    Open,
    Closed
};

enum class PressureControlValveCommand : uint8_t {
    Open,
    Close
};

class PressureControlValve {
public:
    PressureControlValve(uint8_t in1Pin,
                         uint8_t in2Pin,
                         uint8_t powerEnablePin,
                         bool powerEnableActiveHigh,
                         uint16_t powerSettleMs,
                         uint16_t pulseMs,
                         uint16_t postPulseMs,
                         bool openIn1High,
                         bool openIn2High,
                         bool closeIn1High,
                         bool closeIn2High);

    void begin();

    bool commandOpen();
    bool commandClose();
    bool command(PressureControlValveCommand command);

    PressureControlValveState lastCommandedState() const;
    static const char* stateName(PressureControlValveState state);

private:
    uint8_t _in1Pin;
    uint8_t _in2Pin;
    uint8_t _powerEnablePin;

    bool _powerEnableActiveHigh;

    uint16_t _powerSettleMs;
    uint16_t _pulseMs;
    uint16_t _postPulseMs;

    bool _openIn1High;
    bool _openIn2High;
    bool _closeIn1High;
    bool _closeIn2High;

    PressureControlValveState _lastCommandedState;

    void setPower(bool enabled);
    void setBridgeIdle();
    void setBridge(bool in1High, bool in2High);
};
