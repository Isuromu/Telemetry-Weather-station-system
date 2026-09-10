#include "PressureControlValve.hpp"

PressureControlValve::PressureControlValve(
    uint8_t in1Pin,
    uint8_t in2Pin,
    uint8_t powerEnablePin,
    bool powerEnableActiveHigh,
    uint16_t powerSettleMs,
    uint16_t pulseMs,
    uint16_t postPulseMs,
    bool openIn1High,
    bool openIn2High,
    bool closeIn1High,
    bool closeIn2High)
    : _in1Pin(in1Pin),
      _in2Pin(in2Pin),
      _powerEnablePin(powerEnablePin),
      _powerEnableActiveHigh(powerEnableActiveHigh),
      _powerSettleMs(powerSettleMs),
      _pulseMs(pulseMs),
      _postPulseMs(postPulseMs),
      _openIn1High(openIn1High),
      _openIn2High(openIn2High),
      _closeIn1High(closeIn1High),
      _closeIn2High(closeIn2High),
      _lastCommandedState(PressureControlValveState::Unknown) {}

void PressureControlValve::begin() {
    pinMode(_in1Pin, OUTPUT);
    pinMode(_in2Pin, OUTPUT);
    pinMode(_powerEnablePin, OUTPUT);

    setBridgeIdle();
    setPower(false);
    _lastCommandedState = PressureControlValveState::Unknown;
}

bool PressureControlValve::commandOpen() {
    return command(PressureControlValveCommand::Open);
}

bool PressureControlValve::commandClose() {
    return command(PressureControlValveCommand::Close);
}

bool PressureControlValve::command(PressureControlValveCommand command) {
    setBridgeIdle();
    setPower(true);

    delay(_powerSettleMs);

    if (command == PressureControlValveCommand::Open) {
        setBridge(_openIn1High, _openIn2High);
    } else {
        setBridge(_closeIn1High, _closeIn2High);
    }

    delay(_pulseMs);

    setBridgeIdle();
    delay(_postPulseMs);

    setPower(false);

    _lastCommandedState =
        (command == PressureControlValveCommand::Open)
            ? PressureControlValveState::Open
            : PressureControlValveState::Closed;

    return true;
}

PressureControlValveState PressureControlValve::lastCommandedState() const {
    return _lastCommandedState;
}

const char* PressureControlValve::stateName(
    PressureControlValveState state) {
    switch (state) {
        case PressureControlValveState::Open:
            return "OPEN";
        case PressureControlValveState::Closed:
            return "CLOSED";
        case PressureControlValveState::Unknown:
        default:
            return "UNKNOWN";
    }
}

void PressureControlValve::setPower(bool enabled) {
    const bool outputHigh =
        enabled ? _powerEnableActiveHigh : !_powerEnableActiveHigh;

    digitalWrite(_powerEnablePin, outputHigh ? HIGH : LOW);
}

void PressureControlValve::setBridgeIdle() {
    digitalWrite(_in1Pin, LOW);
    digitalWrite(_in2Pin, LOW);
}

void PressureControlValve::setBridge(bool in1High, bool in2High) {
    digitalWrite(_in1Pin, in1High ? HIGH : LOW);
    digitalWrite(_in2Pin, in2High ? HIGH : LOW);
}
