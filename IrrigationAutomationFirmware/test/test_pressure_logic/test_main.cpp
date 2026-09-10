#include <unity.h>

#include <BatteryMonitor.h>
#include <FlowMeter.h>
#include <PressureNodeConfig.h>
#include <PressureNodeLoRaProtocol.h>
#include <PressureNodeRuntimeMode.h>
#include <PressureNodeTypes.h>

using namespace irrigation::pressure_node;
namespace config = irrigation::pressure_node::rev_a;
namespace lora_protocol = irrigation::pressure_node::lorawan_protocol;

static_assert(areOppositePolarities(
                  {config::pcv::OPEN_IN1_HIGH, config::pcv::OPEN_IN2_HIGH},
                  {config::pcv::CLOSE_IN1_HIGH,
                   config::pcv::CLOSE_IN2_HIGH}),
              "Valve polarities must remain opposite.");
static_assert(!BatteryReading{}.hasSoc(),
              "Default battery status must not claim state of charge.");
static_assert(!readingHasSample(ReadingStatus::ReadError),
              "Sensor errors must not be exposed as samples.");
constexpr BatteryMonitorConfiguration kMeasuredBatteryConfiguration{
    35, 100000.0F, 20000.0F, 0.9883F, 32, 10};
static_assert(batteryVoltageFromAdc(2.097F,
                                    kMeasuredBatteryConfiguration) > 12.43F &&
                  batteryVoltageFromAdc(2.097F,
                                        kMeasuredBatteryConfiguration) <
                      12.44F,
              "Measured battery-divider calibration changed.");
static_assert(config::pins::PCV_IN1 == 2 && config::pins::PCV_IN2 == 15,
              "The valve bridge pin migration changed.");
static_assert(config::flow_meter::UART_RX == 16 &&
                  config::flow_meter::UART_TX == 17,
              "The RS-485 UART allocation changed.");
static_assert(lora_protocol::STATUS_PAYLOAD_SIZE == 32,
              "LoRaWAN status payload size changed.");
static_assert(runtime::ACTIVE_MODE == runtime::Mode::SerialOnly,
              "Compile tests default to the safe Serial-only runtime.");

void test_valve_state_is_unknown_after_reset() {
  PressureControlValveStateModel model;
  TEST_ASSERT_EQUAL_STRING("UNKNOWN",
                           valveStateName(model.status().lastCommanded));
  TEST_ASSERT_EQUAL_STRING(
      "NOT_AVAILABLE", verificationStateName(model.status().verification));

  model.recordSuccessfulCommand(PressureControlValveState::Open);
  TEST_ASSERT_EQUAL_STRING("OPEN",
                           valveStateName(model.status().lastCommanded));
  model.reset();
  TEST_ASSERT_EQUAL_STRING("UNKNOWN",
                           valveStateName(model.status().lastCommanded));
}

void test_open_and_close_polarities_are_opposite() {
  const BridgePolarity open{config::pcv::OPEN_IN1_HIGH,
                            config::pcv::OPEN_IN2_HIGH};
  const BridgePolarity close{config::pcv::CLOSE_IN1_HIGH,
                             config::pcv::CLOSE_IN2_HIGH};
  TEST_ASSERT_TRUE(areOppositePolarities(open, close));
  TEST_ASSERT_FALSE(areOppositePolarities(open, open));
}

void test_pressure_error_has_no_sample() {
  PressureReading reading{};
  reading.status = ReadingStatus::ReadError;
  TEST_ASSERT_FALSE(reading.hasSample());
  reading.status = ReadingStatus::ValidUncalibrated;
  TEST_ASSERT_TRUE(reading.hasSample());
  TEST_ASSERT_FALSE(reading.engineeringUnitsValidated());
}

void test_default_status_does_not_claim_battery_soc() {
  PressureControlNodeStatus status{};
  TEST_ASSERT_FALSE(status.battery.hasSoc());
  TEST_ASSERT_EQUAL_STRING("UNKNOWN",
                           batterySocStatusName(status.battery.socStatus));
  TEST_ASSERT_EQUAL_STRING("UNKNOWN",
                           valveStateName(status.valve.lastCommanded));
}

void test_battery_calibration_matches_measured_reference() {
  TEST_ASSERT_FLOAT_WITHIN(
      0.005F, 12.435F,
      batteryVoltageFromAdc(2.097F, kMeasuredBatteryConfiguration));
}

void test_unavailable_flow_meter_is_explicit() {
  UnavailableFlowMeter flowMeter;
  TEST_ASSERT_FALSE(flowMeter.begin());
  TEST_ASSERT_FALSE(flowMeter.available());
  const FlowReading reading = flowMeter.read();
  TEST_ASSERT_EQUAL_STRING("CONFIGURATION_MISSING",
                           readingStatusName(reading.status));
  TEST_ASSERT_FALSE(reading.hasSample());
  TEST_ASSERT_EQUAL_STRING("UNKNOWN", flowUnitName(reading.unit));
}

void test_power_policy_does_not_enable_sleep_implicitly() {
  PowerPolicy policy({false, 60000, 300000, false});
  TEST_ASSERT_EQUAL_STRING("IDLE", powerModeName(policy.mode()));
  TEST_ASSERT_FALSE(policy.markSleepReady());
  TEST_ASSERT_EQUAL_STRING("IDLE", powerModeName(policy.mode()));
  policy.markActive();
  TEST_ASSERT_EQUAL_STRING("ACTIVE", powerModeName(policy.mode()));
}

void test_lorawan_open_and_report_interval_command_decodes() {
  const uint8_t payload[lora_protocol::COMMAND_PAYLOAD_SIZE] = {
      2, 0x03, 1, 0, 0, 0, 2, 88, 0x12, 0x34};
  lora_protocol::DownlinkCommand command{};
  const auto status = lora_protocol::decodeDownlink(
      payload, sizeof(payload), config::lorawan::COMMAND_FPORT,
      config::lorawan::COMMAND_FPORT,
      config::lorawan::MIN_REPORT_INTERVAL_SECONDS,
      config::lorawan::MAX_REPORT_INTERVAL_SECONDS, command);
  TEST_ASSERT_EQUAL_STRING("OK",
                           lora_protocol::commandDecodeStatusName(status));
  TEST_ASSERT_TRUE(command.hasValveAction);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(lora_protocol::ValveAction::Open),
      static_cast<uint8_t>(command.valveAction));
  TEST_ASSERT_TRUE(command.hasReportInterval);
  TEST_ASSERT_EQUAL_UINT32(600, command.reportIntervalSeconds);
  TEST_ASSERT_EQUAL_HEX16(0x1234, command.commandId);
}

void test_lorawan_rejects_sub_minimum_report_interval() {
  const uint8_t payload[lora_protocol::COMMAND_PAYLOAD_SIZE] = {
      2, 0x02, 0, 0, 0, 0, 0, 59, 0, 1};
  lora_protocol::DownlinkCommand command{};
  const auto status = lora_protocol::decodeDownlink(
      payload, sizeof(payload), config::lorawan::COMMAND_FPORT,
      config::lorawan::COMMAND_FPORT,
      config::lorawan::MIN_REPORT_INTERVAL_SECONDS,
      config::lorawan::MAX_REPORT_INTERVAL_SECONDS, command);
  TEST_ASSERT_EQUAL_STRING(
      "INVALID_REPORT_INTERVAL",
      lora_protocol::commandDecodeStatusName(status));
}

void test_lorawan_accepts_ten_second_commissioning_interval() {
  const uint8_t payload[lora_protocol::COMMAND_PAYLOAD_SIZE] = {
      2, 0x02, 0, 0, 0, 0, 0, 10, 0, 2};
  lora_protocol::DownlinkCommand command{};
  const auto status = lora_protocol::decodeDownlink(
      payload, sizeof(payload), config::lorawan::COMMAND_FPORT,
      config::lorawan::COMMAND_FPORT, 10,
      config::lorawan::MAX_REPORT_INTERVAL_SECONDS, command);
  TEST_ASSERT_EQUAL_STRING("OK",
                           lora_protocol::commandDecodeStatusName(status));
  TEST_ASSERT_TRUE(command.hasReportInterval);
  TEST_ASSERT_EQUAL_UINT32(10, command.reportIntervalSeconds);
}

void test_lorawan_explicit_no_op_decodes_without_valve_pulse() {
  const uint8_t payload[lora_protocol::COMMAND_PAYLOAD_SIZE] = {
      2, 0x01, 0, 0, 0, 0, 0, 0, 0, 7};
  lora_protocol::DownlinkCommand command{};
  const auto status = lora_protocol::decodeDownlink(
      payload, sizeof(payload), config::lorawan::COMMAND_FPORT,
      config::lorawan::COMMAND_FPORT,
      config::lorawan::MIN_REPORT_INTERVAL_SECONDS,
      config::lorawan::MAX_REPORT_INTERVAL_SECONDS, command);
  TEST_ASSERT_EQUAL_STRING("OK",
                           lora_protocol::commandDecodeStatusName(status));
  TEST_ASSERT_TRUE(command.hasValveAction);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(lora_protocol::ValveAction::None),
      static_cast<uint8_t>(command.valveAction));
  TEST_ASSERT_FALSE(command.hasReportInterval);
  TEST_ASSERT_EQUAL_UINT16(7, command.commandId);
}

void test_lorawan_flow_total_reset_command_decodes() {
  const uint8_t payload[lora_protocol::COMMAND_PAYLOAD_SIZE] = {
      2, 0x04, 0, 0, 0, 0, 0, 0, 0, 8};
  lora_protocol::DownlinkCommand command{};
  const auto status = lora_protocol::decodeDownlink(
      payload, sizeof(payload), config::lorawan::COMMAND_FPORT,
      config::lorawan::COMMAND_FPORT,
      config::lorawan::MIN_REPORT_INTERVAL_SECONDS,
      config::lorawan::MAX_REPORT_INTERVAL_SECONDS, command);
  TEST_ASSERT_EQUAL_STRING("OK",
                           lora_protocol::commandDecodeStatusName(status));
  TEST_ASSERT_FALSE(command.hasValveAction);
  TEST_ASSERT_FALSE(command.hasReportInterval);
  TEST_ASSERT_TRUE(command.hasFlowTotalReset);
  TEST_ASSERT_EQUAL_UINT16(8, command.commandId);
}

void test_lorawan_status_payload_encodes_all_measurements() {
  PressureControlNodeStatus status{};
  status.upstreamPressure =
      {ReadingStatus::ValidUncalibrated, -10.0F, -10.14F, 0x7F};
  status.downstreamPressure =
      {ReadingStatus::ValidUncalibrated, 0.02F, 29.33F, 0x7F};
  status.battery.status = ReadingStatus::Valid;
  status.battery.voltageV = 12.435F;
  status.flow.status = ReadingStatus::Valid;
  status.flow.value = 1.234F;
  status.flow.unit = FlowUnit::CubicMetersPerHour;
  status.flow.velocityMetersPerSecond = 0.456F;
  status.flow.velocityAvailable = true;
  status.flow.deviceErrorBits = 0x0102;
  status.flow.diagnosticsAvailable = true;
  status.flowTotal.status = ReadingStatus::Valid;
  status.flowTotal.meterPositiveCubicMeters = 127.579F;
  status.flowTotal.sinceResetCubicMeters = 123.456F;
  status.flowTotal.sinceResetAvailable = true;
  status.valve.lastCommanded = PressureControlValveState::Closed;

  uint8_t payload[lora_protocol::STATUS_PAYLOAD_SIZE] = {};
  lora_protocol::buildStatusPayload(
      status, lora_protocol::StatusReason::PeriodicReport, 600, 0x1234,
      payload);
  TEST_ASSERT_EQUAL_UINT8(lora_protocol::PROTOCOL_VERSION, payload[0]);
  TEST_ASSERT_EQUAL_HEX8(0xCF, payload[1]);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(PressureControlValveState::Closed), payload[2]);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(lora_protocol::StatusReason::PeriodicReport),
      payload[3]);
  TEST_ASSERT_EQUAL_HEX8(0xFC, payload[4]);
  TEST_ASSERT_EQUAL_HEX8(0x18, payload[5]);  // -10.00 bar x100.
  TEST_ASSERT_EQUAL_HEX8(0x30, payload[12]);
  TEST_ASSERT_EQUAL_HEX8(0x93, payload[13]);  // 12,435 mV.
  TEST_ASSERT_EQUAL_HEX8(0x00, payload[22]);
  TEST_ASSERT_EQUAL_HEX8(0x00, payload[23]);
  TEST_ASSERT_EQUAL_HEX8(0x02, payload[24]);
  TEST_ASSERT_EQUAL_HEX8(0x58, payload[25]);  // 600 seconds.
  TEST_ASSERT_EQUAL_HEX8(0x12, payload[26]);
  TEST_ASSERT_EQUAL_HEX8(0x34, payload[27]);
  TEST_ASSERT_EQUAL_HEX8(0x00, payload[28]);
  TEST_ASSERT_EQUAL_HEX8(0x01, payload[29]);
  TEST_ASSERT_EQUAL_HEX8(0xE2, payload[30]);
  TEST_ASSERT_EQUAL_HEX8(0x40, payload[31]);  // 123,456 litres.
}

void runTests() {
  UNITY_BEGIN();
  RUN_TEST(test_valve_state_is_unknown_after_reset);
  RUN_TEST(test_open_and_close_polarities_are_opposite);
  RUN_TEST(test_pressure_error_has_no_sample);
  RUN_TEST(test_default_status_does_not_claim_battery_soc);
  RUN_TEST(test_battery_calibration_matches_measured_reference);
  RUN_TEST(test_unavailable_flow_meter_is_explicit);
  RUN_TEST(test_power_policy_does_not_enable_sleep_implicitly);
  RUN_TEST(test_lorawan_open_and_report_interval_command_decodes);
  RUN_TEST(test_lorawan_rejects_sub_minimum_report_interval);
  RUN_TEST(test_lorawan_accepts_ten_second_commissioning_interval);
  RUN_TEST(test_lorawan_explicit_no_op_decodes_without_valve_pulse);
  RUN_TEST(test_lorawan_flow_total_reset_command_decodes);
  RUN_TEST(test_lorawan_status_payload_encodes_all_measurements);
  UNITY_END();
}

void setup() { runTests(); }

void loop() {}
