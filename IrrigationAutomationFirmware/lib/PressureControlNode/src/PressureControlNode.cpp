#include "PressureControlNode.h"

#include <Arduino.h>
#include <math.h>

namespace irrigation::pressure_node {

PressureControlNode::PressureControlNode(
    BatteryMonitor &battery, PressureSensorXDB401 &upstreamPressure,
    PressureSensorXDB401 &downstreamPressure, PressureControlValve &valve,
    FlowMeter &flowMeter, PowerPolicyConfiguration powerConfiguration)
    : battery_(battery),
      upstreamPressure_(upstreamPressure),
      downstreamPressure_(downstreamPressure),
      valve_(valve),
      flowMeter_(flowMeter),
      powerPolicy_(powerConfiguration) {}

bool PressureControlNode::begin() {
  markActive();
  const bool valveReady = valve_.begin();
  const bool batteryReady = battery_.begin();
  status_.upstreamPressure.status = upstreamPressure_.begin();
  status_.upstreamPressure.address = upstreamPressure_.address();
  status_.downstreamPressure.status = downstreamPressure_.begin();
  status_.downstreamPressure.address = downstreamPressure_.address();
  const bool flowReady = flowMeter_.begin();
  status_.flow.status =
      flowReady ? ReadingStatus::NotSampled
                : ReadingStatus::ConfigurationMissing;
  status_.flowTotal.status =
      flowReady ? ReadingStatus::NotSampled
                : ReadingStatus::ConfigurationMissing;
  status_.battery.status =
      batteryReady ? ReadingStatus::NotSampled
                   : ReadingStatus::ConfigurationMissing;
  synchronizeValveStatus();
  markIdle();
  return valveReady && batteryReady;
}

void PressureControlNode::markActive() {
  powerPolicy_.markActive();
  status_.powerMode = powerPolicy_.mode();
}

void PressureControlNode::markIdle() {
  powerPolicy_.markIdle();
  status_.powerMode = powerPolicy_.mode();
}

void PressureControlNode::synchronizeValveStatus() {
  status_.valve = valve_.status();
}

BatteryReading PressureControlNode::readBattery() {
  markActive();
  status_.battery = battery_.read();
  markIdle();
  return status_.battery;
}

PressurePairReading PressureControlNode::readPressuresWhileActive() {
  PressurePairReading pressures{upstreamPressure_.read(),
                                downstreamPressure_.read()};
  status_.upstreamPressure = pressures.upstream;
  status_.downstreamPressure = pressures.downstream;
  return pressures;
}

PressurePairReading PressureControlNode::readPressures() {
  markActive();
  const PressurePairReading pressures = readPressuresWhileActive();
  markIdle();
  return pressures;
}

FlowReading PressureControlNode::readFlow() {
  markActive();
  status_.flow = flowMeter_.read();
  markIdle();
  return status_.flow;
}

FlowTotalReading PressureControlNode::applyFlowTotalBaseline(
    FlowTotalReading reading) const {
  if (!reading.hasMeterTotals() || !hasFlowTotalBaseline_) return reading;

  // A smaller current positive total means the TUF counter was reset or
  // replaced outside this firmware. Do not silently report a negative water
  // consumption value; keep the raw meter totals visible and require a new
  // explicit local baseline reset.
  if (reading.meterPositiveCubicMeters < flowTotalBaselineCubicMeters_)
    return reading;

  reading.sinceResetCubicMeters =
      reading.meterPositiveCubicMeters - flowTotalBaselineCubicMeters_;
  reading.sinceResetAvailable = true;
  return reading;
}

FlowTotalReading PressureControlNode::readFlowTotalsWhileActive() {
  status_.flowTotal = applyFlowTotalBaseline(flowMeter_.readTotals());
  return status_.flowTotal;
}

FlowTotalReading PressureControlNode::readFlowTotals() {
  markActive();
  const FlowTotalReading reading = readFlowTotalsWhileActive();
  markIdle();
  return reading;
}

FlowTotalResetResult PressureControlNode::resetFlowTotal() {
  markActive();
  FlowTotalResetResult result{};
  result.totals = flowMeter_.readTotals();
  if (result.totals.hasMeterTotals() &&
      isfinite(result.totals.meterPositiveCubicMeters)) {
    flowTotalBaselineCubicMeters_ =
        result.totals.meterPositiveCubicMeters;
    hasFlowTotalBaseline_ = true;
    result.totals = applyFlowTotalBaseline(result.totals);
    result.applied = true;
  }
  status_.flowTotal = result.totals;
  markIdle();
  return result;
}

FlowMeterWordOrderProbe PressureControlNode::probeFlowWordOrder() {
  markActive();
  const FlowMeterWordOrderProbe probe = flowMeter_.probeWordOrder();
  markIdle();
  return probe;
}

const PressureControlNodeStatus &PressureControlNode::refreshStatus() {
  markActive();
  status_.battery = battery_.read();
  readPressuresWhileActive();
  status_.flow = flowMeter_.read();
  readFlowTotalsWhileActive();
  synchronizeValveStatus();
  markIdle();
  return status_;
}

ValveCommandResult PressureControlNode::commandValve(
    PressureControlValveState desiredState) {
  markActive();
  ValveCommandResult result{};
  result.actuation = valve_.command(desiredState);
  if (result.actuation == ValveActuationStatus::Ok) {
    delay(valve_.hydraulicSettleMs());
    result.pressures = readPressuresWhileActive();
  } else {
    result.pressures = {status_.upstreamPressure,
                        status_.downstreamPressure};
  }
  synchronizeValveStatus();
  result.valve = status_.valve;
  markIdle();
  return result;
}

bool PressureControlNode::restoreValveCommandedState(
    PressureControlValveState state) {
  const bool restored = valve_.restoreLastCommandedState(state);
  synchronizeValveStatus();
  return restored;
}

bool PressureControlNode::restoreFlowTotalBaseline(
    float positiveCubicMeters) {
  if (!isfinite(positiveCubicMeters) || positiveCubicMeters < 0.0F)
    return false;
  flowTotalBaselineCubicMeters_ = positiveCubicMeters;
  hasFlowTotalBaseline_ = true;
  return true;
}

bool PressureControlNode::prepareForSleep() {
  valve_.forceSafeIdle();
  return markSleepReady();
}

bool PressureControlNode::markSleepReady() {
  const bool accepted = powerPolicy_.markSleepReady();
  status_.powerMode = powerPolicy_.mode();
  return accepted;
}

}  // namespace irrigation::pressure_node
