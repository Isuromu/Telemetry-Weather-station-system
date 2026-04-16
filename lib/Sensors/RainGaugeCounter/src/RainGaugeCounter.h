#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "PrintController.h"
#include "RS485Modbus.h"
#include "SensorDriver.h"
#include "Configuration_System.h"

#define RAIN_GAUGE_COUNTING_MODE_EXTERNAL_COUNTER 0
#define RAIN_GAUGE_COUNTING_MODE_MCU_INTERRUPT    1
#define RAIN_GAUGE_COUNTING_MODE_RS485_SENSOR     2

class RainGaugeCounter : public SensorDriver {
public:
  enum CountingMode : uint8_t {
    EXTERNAL_COUNTER = RAIN_GAUGE_COUNTING_MODE_EXTERNAL_COUNTER,
    MCU_INTERRUPT    = RAIN_GAUGE_COUNTING_MODE_MCU_INTERRUPT,
    RS485_SENSOR     = RAIN_GAUGE_COUNTING_MODE_RS485_SENSOR
  };

  uint32_t pulse_count;
  double rainfall_mm;

  RainGaugeCounter(const char* sensorId,
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
                   uint16_t interruptDebounceMs);

  void begin(TwoWire* wire = &Wire, RS485Bus* bus = nullptr);
  void setLogger(PrintController* logger) { _log = logger; }

  bool readData() override;
  void setFallbackValues() override;

  bool resetCounter();

  bool activateCounting();
  bool deactivateCounting();
  bool isCountingActive() const { return _countingActive; }

  CountingMode getCountingMode() const { return _countingMode; }
  double getMmPerPulse() const { return _mmPerPulse; }

  static const char* countingModeToString(CountingMode mode);

protected:
  virtual bool supportsRs485SensorMode() const { return false; }
  virtual bool readRs485Rainfall(double& rainfallMm);
  virtual bool resetRs485Rainfall();

  RS485Bus* _bus;

private:
  TwoWire* _wire;
  PrintController* _log;

  double _mmPerPulse;
  CountingMode _countingMode;
  uint8_t _pcf8574Address;
  int8_t _counterResetPin;
  int8_t _interruptPin;
  uint16_t _interruptDebounceMs;

  volatile uint32_t _mcuPulseCounter;
  volatile unsigned long _lastInterruptMs;
  bool _countingActive;

  bool readExternalCounter();
  bool resetExternalCounter();
  bool readMcuCounter();
  bool resetMcuCounter();
  void handlePulseInterrupt();
  void logStep(const __FlashStringHelper* message) const;

  static RainGaugeCounter* _activeInterruptCounter;
  static void handleActiveInterrupt();
};
