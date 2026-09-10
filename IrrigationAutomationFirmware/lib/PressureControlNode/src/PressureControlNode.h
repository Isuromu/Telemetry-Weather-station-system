#pragma once

#include <BatteryMonitor.h>
#include <FlowMeter.h>
#include <PressureControlValve.h>
#include <PressureNodeTypes.h>
#include <PressureSensorXDB401.h>

namespace irrigation::pressure_node {

struct ValveCommandResult {
  ValveActuationStatus actuation{ValveActuationStatus::NotInitialized};
  ValveStatus valve{};
  PressurePairReading pressures{};

  bool ok() const { return actuation == ValveActuationStatus::Ok; }
};

class PressureControlNode {
 public:
  PressureControlNode(BatteryMonitor &battery,
                      PressureSensorXDB401 &upstreamPressure,
                      PressureSensorXDB401 &downstreamPressure,
                      PressureControlValve &valve, FlowMeter &flowMeter,
                      PowerPolicyConfiguration powerConfiguration);

  bool begin();
  BatteryReading readBattery();
  PressurePairReading readPressures();
  FlowReading readFlow();
  FlowTotalReading readFlowTotals();
  FlowTotalResetResult resetFlowTotal();
  FlowMeterWordOrderProbe probeFlowWordOrder();
  const PressureControlNodeStatus &refreshStatus();
  ValveCommandResult commandValve(PressureControlValveState desiredState);
  bool restoreValveCommandedState(PressureControlValveState state);
  bool restoreFlowTotalBaseline(float positiveCubicMeters);
  bool hasFlowTotalBaseline() const { return hasFlowTotalBaseline_; }
  float flowTotalBaselineCubicMeters() const {
    return flowTotalBaselineCubicMeters_;
  }
  bool prepareForSleep();

  const PressureControlNodeStatus &status() const { return status_; }
  const PowerPolicy &powerPolicy() const { return powerPolicy_; }
  bool markSleepReady();

 private:
  BatteryMonitor &battery_;
  PressureSensorXDB401 &upstreamPressure_;
  PressureSensorXDB401 &downstreamPressure_;
  PressureControlValve &valve_;
  FlowMeter &flowMeter_;
  PowerPolicy powerPolicy_;
  PressureControlNodeStatus status_{};
  float flowTotalBaselineCubicMeters_{0.0F};
  bool hasFlowTotalBaseline_{false};

  void markActive();
  void markIdle();
  void synchronizeValveStatus();
  PressurePairReading readPressuresWhileActive();
  FlowTotalReading readFlowTotalsWhileActive();
  FlowTotalReading applyFlowTotalBaseline(FlowTotalReading reading) const;
};

}  // namespace irrigation::pressure_node
