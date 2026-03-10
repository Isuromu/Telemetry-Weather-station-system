#pragma once
#include <Arduino.h>
#include "SensorDriver.h"
#include "PrintController.h"

// ============================================================================
// SlotManager — Enforces Sample Rate Slot Limits
// ============================================================================
// Some sample rates have limited sensor slots due to battery/bus constraints:
//   1-minute rate: max 2 sensors
//   5-minute rate: max 4 sensors
//   15+ minute rates: no limit
//
// The server UI controls which sensors occupy limited slots.
// SlotManager validates during setup() and rejects over-limit assignments.
//
// Usage:
//   SlotManager slots;
//   slots.registerSensor(&leaf_s1);   // 1-min sensor
//   slots.registerSensor(&leaf_s2);   // 1-min sensor
//   slots.registerSensor(&leaf_s3);   // REJECTED — slot full!
//   if (!slots.validate()) { /* error, too many sensors */ }
// ============================================================================

// Slot limits — changeable by server
#define DEFAULT_MAX_1MIN_SENSORS  2
#define DEFAULT_MAX_5MIN_SENSORS  4

// Maximum total sensors the station can track
#define MAX_TOTAL_SENSORS 20

class SlotManager {
public:
  SlotManager();

  // Register a sensor into the manager
  // Returns true if accepted, false if slot limit exceeded
  bool registerSensor(SensorDriver* sensor);

  // Remove a sensor from tracking (by pointer)
  void removeSensor(SensorDriver* sensor);

  // Validate all registered sensors against slot limits
  // Returns true if all limits OK
  bool validate();

  // Get count of sensors at a specific rate
  uint8_t countAtRate(uint8_t rateMinutes);

  // Get total registered sensors
  uint8_t totalSensors() const { return _sensorCount; }

  // Get a sensor by index (for looping in main code)
  SensorDriver* getSensor(uint8_t index);

  // --- Slot Limit Configuration (server can change these) ---
  void setMax1MinSensors(uint8_t max) { _max1Min = max; }
  void setMax5MinSensors(uint8_t max) { _max5Min = max; }
  uint8_t getMax1MinSensors() const { return _max1Min; }
  uint8_t getMax5MinSensors() const { return _max5Min; }

  // Change a sensor's sample rate (with slot validation)
  // Returns true if the new rate was accepted
  bool changeSensorRate(SensorDriver* sensor, uint8_t newRateMin);

  // Optional debug
  void setDebug(PrintController* printer, bool enable);

  // Print registered sensors and slot usage
  void printStatus();

private:
  SensorDriver* _sensors[MAX_TOTAL_SENSORS];
  uint8_t _sensorCount;

  uint8_t _max1Min;
  uint8_t _max5Min;

  PrintController* _log;
  bool _debugEnable;
};
