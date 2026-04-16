#include "RainGaugeCounter.h"

RainGaugeCounter* RainGaugeCounter::_activeInterruptCounter = nullptr;

RainGaugeCounter::RainGaugeCounter(const char* sensorId,
                                   uint8_t address,
                                   bool debugEnable,
                                   uint8_t powerLineIndex,
                                   uint8_t interfaceIndex,
                                   uint16_t sampleRateMin,
                                   uint32_t warmUpTimeMs,
                                   uint8_t maxConsecutiveErrors,
                                   uint32_t minUsefulPowerOffMs,
                                   double mmPerPulse,
                                   CountingMode countingMode,
                                   uint8_t pcf8574Address,
                                   int8_t counterResetPin,
                                   int8_t interruptPin,
                                   uint16_t interruptDebounceMs)
    : SensorDriver(sensorId,
                   address,
                   debugEnable,
                   powerLineIndex,
                   interfaceIndex,
                   sampleRateMin,
                   warmUpTimeMs,
                   maxConsecutiveErrors,
                   minUsefulPowerOffMs),
      pulse_count(0),
      rainfall_mm(0.0),
      _bus(nullptr),
      _wire(nullptr),
      _log(nullptr),
      _mmPerPulse(mmPerPulse),
      _countingMode(countingMode),
      _pcf8574Address(pcf8574Address),
      _counterResetPin(counterResetPin),
      _interruptPin(interruptPin),
      _interruptDebounceMs(interruptDebounceMs),
      _mcuPulseCounter(0),
      _lastInterruptMs(0),
      _countingActive(false) {}

void RainGaugeCounter::begin(TwoWire* wire, RS485Bus* bus) {
  _wire = wire;
  _bus = bus;

  if (_counterResetPin >= 0) {
    pinMode(_counterResetPin, OUTPUT);
    digitalWrite(_counterResetPin, LOW);
  }

  if (_countingMode == MCU_INTERRUPT && _interruptPin >= 0) {
    pinMode(_interruptPin, INPUT_PULLUP);
  }
}

void RainGaugeCounter::setFallbackValues() {
  pulse_count = 0;
  rainfall_mm = -99.0;
}

const char* RainGaugeCounter::countingModeToString(CountingMode mode) {
  switch (mode) {
    case EXTERNAL_COUNTER: return "external PCF8574/4040 counter";
    case MCU_INTERRUPT: return "MCU interrupt pulse counter";
    case RS485_SENSOR: return "RS485 sensor internal counter";
    default: return "unknown";
  }
}

void RainGaugeCounter::logStep(const __FlashStringHelper* message) const {
  if (_log && _debugEnable) {
    _log->println(message, true);
  }
}

bool RainGaugeCounter::readData() {
  markReadTime(millis());

  bool ok = false;
  switch (_countingMode) {
    case EXTERNAL_COUNTER:
      ok = readExternalCounter();
      break;

    case MCU_INTERRUPT:
      ok = readMcuCounter();
      break;

    case RS485_SENSOR:
      if (supportsRs485SensorMode()) {
        double value = -99.0;
        ok = readRs485Rainfall(value);
        if (ok) {
          rainfall_mm = value;
          pulse_count = (uint32_t)((value / _mmPerPulse) + 0.5);
        }
      }
      break;

    default:
      ok = false;
      break;
  }

  if (ok) {
    markSuccess();
    return true;
  }

  setFallbackValues();
  markFailure();
  return false;
}

bool RainGaugeCounter::resetCounter() {
  switch (_countingMode) {
    case EXTERNAL_COUNTER:
      return resetExternalCounter();

    case MCU_INTERRUPT:
      return resetMcuCounter();

    case RS485_SENSOR:
      if (supportsRs485SensorMode()) {
        return resetRs485Rainfall();
      }
      return false;

    default:
      return false;
  }
}

bool RainGaugeCounter::readExternalCounter() {
  if (!_wire) return false;

  logStep(F("[RAIN] Reading PCF8574 external counter byte"));

  _wire->beginTransmission(_pcf8574Address);
  _wire->write((uint8_t)0xFF);
  if (_wire->endTransmission() != 0) {
    return false;
  }

  const uint8_t requested = _wire->requestFrom((int)_pcf8574Address, 1);
  if (requested < 1 || !_wire->available()) {
    return false;
  }

  const uint8_t raw = _wire->read();
  pulse_count = raw;
  rainfall_mm = (double)pulse_count * _mmPerPulse;

  if (_log && _debugEnable) {
    _log->print(F("[RAIN] PCF8574 raw count="), true);
    _log->print((unsigned int)raw, true, " | rainfall=");
    _log->print(rainfall_mm, true, " mm", 4);
    _log->println("", true);
  }

  return true;
}

bool RainGaugeCounter::resetExternalCounter() {
  if (_counterResetPin < 0) return false;

  logStep(F("[RAIN] Resetting 4040 counter: reset pin HIGH then LOW"));
  digitalWrite(_counterResetPin, HIGH);
  delay(10);
  digitalWrite(_counterResetPin, LOW);
  return true;
}

bool RainGaugeCounter::readMcuCounter() {
  noInterrupts();
  const uint32_t count = _mcuPulseCounter;
  interrupts();

  pulse_count = count;
  rainfall_mm = (double)pulse_count * _mmPerPulse;
  return true;
}

bool RainGaugeCounter::resetMcuCounter() {
  noInterrupts();
  _mcuPulseCounter = 0;
  _lastInterruptMs = 0;
  interrupts();
  pulse_count = 0;
  rainfall_mm = 0.0;
  return true;
}

bool RainGaugeCounter::activateCounting() {
  if (_countingMode != MCU_INTERRUPT || _interruptPin < 0) {
    return false;
  }

  _activeInterruptCounter = this;
  attachInterrupt(digitalPinToInterrupt(_interruptPin), RainGaugeCounter::handleActiveInterrupt, FALLING);
  _countingActive = true;
  logStep(F("[RAIN] MCU interrupt counting activated"));
  return true;
}

bool RainGaugeCounter::deactivateCounting() {
  if (_countingMode != MCU_INTERRUPT || _interruptPin < 0) {
    return false;
  }

  detachInterrupt(digitalPinToInterrupt(_interruptPin));
  if (_activeInterruptCounter == this) {
    _activeInterruptCounter = nullptr;
  }
  _countingActive = false;
  logStep(F("[RAIN] MCU interrupt counting deactivated"));
  return true;
}

void RainGaugeCounter::handlePulseInterrupt() {
  const unsigned long now = millis();
  if ((now - _lastInterruptMs) > _interruptDebounceMs) {
    ++_mcuPulseCounter;
    _lastInterruptMs = now;
  }
}

void RainGaugeCounter::handleActiveInterrupt() {
  if (_activeInterruptCounter) {
    _activeInterruptCounter->handlePulseInterrupt();
  }
}

bool RainGaugeCounter::readRs485Rainfall(double& rainfallMm) {
  (void)rainfallMm;
  return false;
}

bool RainGaugeCounter::resetRs485Rainfall() {
  return false;
}
