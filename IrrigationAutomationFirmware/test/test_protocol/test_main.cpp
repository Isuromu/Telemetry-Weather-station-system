#include <unity.h>

#include <DelixiProtocol.h>
#include <EpeverLs1024bProtocol.h>
#include <ModbusCrc.h>
#include <Tuf2000mProtocol.h>

using irrigation::pressure_node::tuf2000m::FloatWordOrder;
namespace tuf = irrigation::pressure_node::tuf2000m::protocol;
namespace solar = irrigation::pressure_node::epever_ls1024b::protocol;

constexpr uint8_t kManualForwardRequest[] = {0x01, 0x06, 0xA0,
                                              0x00, 0x00, 0x01};
static_assert(modbus::crc16(kManualForwardRequest,
                            sizeof(kManualForwardRequest)) == 0x0A6A,
              "The manual forward command CRC must remain 0x0A6A.");
static_assert(delixi::protocol::parameterAddress(2, 1, 12, false) == 0x210C,
              "Persistent CDI-E parameter addressing changed.");
static_assert(delixi::protocol::parameterAddress(2, 1, 12, true) == 0x250C,
              "RAM-only CDI-E parameter addressing changed.");
static_assert(delixi::protocol::frequencyHzToRawPercent(25.0f, 50.0f) == 5000,
              "CDI-E communication frequency scaling changed.");
constexpr auto kTufManualReadRequest =
    tuf::makeReadHoldingRegistersRequest(1, tuf::modbusAddress(1), 10);
static_assert(kTufManualReadRequest.bytes[0] == 0x01 &&
                  kTufManualReadRequest.bytes[1] == 0x03 &&
                  kTufManualReadRequest.bytes[2] == 0x00 &&
                  kTufManualReadRequest.bytes[3] == 0x00 &&
                  kTufManualReadRequest.bytes[4] == 0x00 &&
                  kTufManualReadRequest.bytes[5] == 0x0A,
              "TUF-2000M manual request encoding changed.");
static_assert(modbus::crc16(kTufManualReadRequest.bytes,
                            sizeof(kTufManualReadRequest.bytes)) == 0xCDC5,
              "TUF-2000M manual request CRC must remain C5 CD on the wire.");
static_assert(tuf::FLOW_RATE_REGISTER == 0x0000,
              "REG0001 must map to Modbus address zero.");
static_assert(tuf::VELOCITY_REGISTER == 0x0004,
              "REG0005 must map to Modbus address four.");
static_assert(tuf::ERROR_CODE_REGISTER == 0x0047,
              "REG0072 must map to Modbus address 0x0047.");
static_assert(tuf::FLOW_TOTALS_REGISTER == 0x0070,
              "REG0113 must map to Modbus address 0x0070.");

void test_manual_forward_command_crc() {
  const uint8_t request[] = {0x01, 0x06, 0xA0, 0x00, 0x00, 0x01};
  TEST_ASSERT_EQUAL_HEX16(0x0A6A, modbus::crc16(request, sizeof(request)));
}

void test_parameter_address_encoding() {
  TEST_ASSERT_EQUAL_HEX16(
      0x210C, delixi::protocol::parameterAddress(2, 1, 12, false));
  TEST_ASSERT_EQUAL_HEX16(
      0x250C, delixi::protocol::parameterAddress(2, 1, 12, true));
  TEST_ASSERT_EQUAL_HEX16(
      0x4102, delixi::protocol::parameterAddress(4, 1, 2, false));
  TEST_ASSERT_EQUAL_HEX16(
      0x0011, delixi::protocol::parameterAddress(0, 0, 17, false));
}

void test_frequency_scaling() {
  TEST_ASSERT_EQUAL_UINT16(10000,
                           delixi::protocol::frequencyPercentToRaw(100.0f));
  TEST_ASSERT_EQUAL_UINT16(
      5000, delixi::protocol::frequencyHzToRawPercent(25.0f, 50.0f));
  TEST_ASSERT_EQUAL_UINT16(
      10000, delixi::protocol::frequencyHzToRawPercent(50.0f, 50.0f));
}

void test_tuf_manual_request_crc() {
  TEST_ASSERT_EQUAL_HEX16(
      0xCDC5,
      modbus::crc16(kTufManualReadRequest.bytes,
                    sizeof(kTufManualReadRequest.bytes)));
}

void test_tuf_real4_word_orders() {
  const uint8_t highWordFirst[] = {0x3F, 0xC0, 0x00, 0x00};
  const uint8_t lowWordFirst[] = {0x00, 0x00, 0x3F, 0xC0};
  float decoded = 0.0F;

  TEST_ASSERT_TRUE(
      tuf::decodeReal4(highWordFirst, FloatWordOrder::HighWordFirst, decoded));
  TEST_ASSERT_FLOAT_WITHIN(0.0001F, 1.5F, decoded);
  TEST_ASSERT_TRUE(
      tuf::decodeReal4(lowWordFirst, FloatWordOrder::LowWordFirst, decoded));
  TEST_ASSERT_FLOAT_WITHIN(0.0001F, 1.5F, decoded);
  TEST_ASSERT_FALSE(
      tuf::decodeReal4(highWordFirst, FloatWordOrder::Unspecified, decoded));
}

void test_tuf_flow_and_velocity_decode() {
  const uint8_t registerData[] = {
      0x40, 0x49, 0x0F, 0xDB,  // Flow rate: approximately 3.1415927 m3/h.
      0x00, 0x00, 0x00, 0x00,  // Energy flow rate: ignored.
      0x3F, 0x00, 0x00, 0x00,  // Velocity: 0.5 m/s.
  };
  float flowRate = 0.0F;
  float velocity = 0.0F;
  TEST_ASSERT_TRUE(tuf::decodeFlowAndVelocity(
      registerData, sizeof(registerData), FloatWordOrder::HighWordFirst,
      flowRate, velocity));
  TEST_ASSERT_FLOAT_WITHIN(0.0001F, 3.1415927F, flowRate);
  TEST_ASSERT_FLOAT_WITHIN(0.0001F, 0.5F, velocity);
}

void test_tuf_low_word_first_flow_totals_decode() {
  const uint8_t registerData[] = {
      0x00, 0x00, 0x41, 0x20,  // Net: 10.0 m3.
      0x00, 0x00, 0x41, 0x30,  // Positive: 11.0 m3.
      0x00, 0x00, 0x3F, 0x80,  // Negative: 1.0 m3.
  };
  float net = 0.0F;
  float positive = 0.0F;
  float negative = 0.0F;
  TEST_ASSERT_TRUE(tuf::decodeFlowTotals(
      registerData, sizeof(registerData), FloatWordOrder::LowWordFirst, net,
      positive, negative));
  TEST_ASSERT_FLOAT_WITHIN(0.0001F, 10.0F, net);
  TEST_ASSERT_FLOAT_WITHIN(0.0001F, 11.0F, positive);
  TEST_ASSERT_FLOAT_WITHIN(0.0001F, 1.0F, negative);
}

// ---------------------------------------------------------------------------
// EPEVER LandStar LS1024B
// ---------------------------------------------------------------------------

// The two service frames are byte-for-byte copies of what the official EPEVER PC
// tool puts on the wire, which is how the proprietary 0x45 address change was
// captured. If either CRC stops matching, the firmware would silently stop being
// able to change a controller address.
constexpr auto kSolarServiceFindId = solar::makeServiceFindIdRequest();
static_assert(modbus::crc16(kSolarServiceFindId.bytes,
                            sizeof(kSolarServiceFindId.bytes)) == 0xBE89,
              "The captured find-ID frame F8 45 00 01 01 F8 89 BE changed.");
constexpr auto kSolarServiceSetId60 = solar::makeServiceSetIdRequest(0x60);
static_assert(modbus::crc16(kSolarServiceSetId60.bytes,
                            sizeof(kSolarServiceSetId60.bytes)) == 0x1488,
              "The captured set-ID frame F8 45 00 01 01 60 88 14 changed.");
static_assert(kSolarServiceFindId.bytes[0] == 0xF8 &&
                  kSolarServiceFindId.bytes[1] == 0x45 &&
                  kSolarServiceFindId.bytes[5] == 0xF8,
              "The service command must stay a broadcast frame on function 0x45.");
static_assert(kSolarServiceSetId60.bytes[5] == 0x60,
              "The set-ID frame carries the new address in its last data byte.");

// Documented examples from the EPEVER protocol table: battery voltage at 0x3108
// is `01 04 31 08 00 01 BE F4`, and a three-register block read from 0x9000 is
// `01 03 90 00 00 03 28 CB`.
constexpr auto kSolarDocumentedInputRead =
    solar::makeReadRegistersRequest(0x01, solar::READ_INPUT_REGISTERS, 0x3108, 1);
static_assert(modbus::crc16(kSolarDocumentedInputRead.bytes,
                            sizeof(kSolarDocumentedInputRead.bytes)) == 0xF4BE,
              "The documented FC04 read frame 01 04 31 08 00 01 BE F4 changed.");
constexpr auto kSolarDocumentedHoldingRead =
    solar::makeReadRegistersRequest(0x01, solar::READ_HOLDING_REGISTERS, 0x9000,
                                    3);
static_assert(modbus::crc16(kSolarDocumentedHoldingRead.bytes,
                            sizeof(kSolarDocumentedHoldingRead.bytes)) == 0xCB28,
              "The documented FC03 read frame 01 03 90 00 00 03 28 CB changed.");

// Commissioned-address frames used by the node. Their CRCs are regression locks
// computed from the encoding above, not quoted from a manual.
constexpr auto kSolarBatteryVoltageRead = solar::makeReadRegistersRequest(
    0x60, solar::READ_INPUT_REGISTERS, solar::REG_BATTERY_VOLTAGE, 1);
static_assert(kSolarBatteryVoltageRead.bytes[0] == 0x60 &&
                  kSolarBatteryVoltageRead.bytes[1] == 0x04 &&
                  kSolarBatteryVoltageRead.bytes[2] == 0x31 &&
                  kSolarBatteryVoltageRead.bytes[3] == 0x04 &&
                  kSolarBatteryVoltageRead.bytes[4] == 0x00 &&
                  kSolarBatteryVoltageRead.bytes[5] == 0x01,
              "The LS1024B battery-voltage request must be 60 04 31 04 00 01.");
static_assert(modbus::crc16(kSolarBatteryVoltageRead.bytes,
                            sizeof(kSolarBatteryVoltageRead.bytes)) == 0x8676,
              "The LS1024B battery-voltage request CRC changed.");
constexpr auto kSolarSettingsBlockRead = solar::makeReadRegistersRequest(
    0x60, solar::READ_HOLDING_REGISTERS, solar::SETTINGS_BLOCK_START,
    solar::SETTINGS_BLOCK_COUNT);
static_assert(modbus::crc16(kSolarSettingsBlockRead.bytes,
                            sizeof(kSolarSettingsBlockRead.bytes)) == 0xBE60,
              "The 0x9000 settings-block request CRC changed.");
constexpr auto kSolarSettingsTailRead = solar::makeReadRegistersRequest(
    0x60, solar::READ_HOLDING_REGISTERS, solar::SETTINGS_TAIL_START,
    solar::SETTINGS_TAIL_COUNT);
static_assert(modbus::crc16(kSolarSettingsTailRead.bytes,
                            sizeof(kSolarSettingsTailRead.bytes)) == 0xB9E0,
              "The 0x900C settings-tail request CRC changed.");
constexpr auto kSolarPvBatteryBlockRead = solar::makeReadRegistersRequest(
    0x60, solar::READ_INPUT_REGISTERS, solar::PV_BATTERY_BLOCK_START,
    solar::PV_BATTERY_BLOCK_COUNT);
static_assert(modbus::crc16(kSolarPvBatteryBlockRead.bytes,
                            sizeof(kSolarPvBatteryBlockRead.bytes)) == 0x82F6,
              "The 0x3100 PV/battery block request CRC changed.");
constexpr auto kSolarLoadTemperatureBlockRead = solar::makeReadRegistersRequest(
    0x60, solar::READ_INPUT_REGISTERS, solar::LOAD_TEMPERATURE_BLOCK_START,
    solar::LOAD_TEMPERATURE_BLOCK_COUNT);
static_assert(modbus::crc16(kSolarLoadTemperatureBlockRead.bytes,
                            sizeof(kSolarLoadTemperatureBlockRead.bytes)) ==
                  0x86B6,
              "The 0x310C load/temperature block request CRC changed.");
// The reference implementation never issues a read longer than twelve registers.
static_assert(solar::SETTINGS_BLOCK_COUNT + solar::SETTINGS_TAIL_COUNT ==
                  solar::REG_DISCHARGING_LIMIT - solar::REG_BATTERY_TYPE + 1,
              "The settings reads must cover 0x9000..0x900E without a gap.");

static_assert(solar::REG_BATTERY_VOLTAGE == 0x3104 &&
                  solar::REG_BATTERY_TEMPERATURE == 0x3110 &&
                  solar::REG_BATTERY_SOC == 0x311A,
              "The LS1024B real-time register map changed.");
static_assert(solar::REG_BATTERY_TYPE == 0x9000 &&
                  solar::REG_DISCHARGING_LIMIT == 0x900E &&
                  solar::REG_RATED_VOLTAGE_LEVEL == 0x9067 &&
                  solar::REG_MAX_CHARGING_CURRENT == 0x90BF,
              "The 0x9000 settings register map changed.");
static_assert(solar::PV_BATTERY_BLOCK_COUNT == 12 &&
                  solar::PV_BATTERY_BLOCK_BYTE_COUNT == 24 &&
                  solar::LOAD_TEMPERATURE_BLOCK_COUNT == 6 &&
                  solar::SETTINGS_BLOCK_COUNT == 12,
              "The block read sizes changed; the driver decodes by offset.");
static_assert(solar::PV_BATTERY_BLOCK_COUNT <= solar::MAX_READ_REGISTER_COUNT &&
                  solar::LOAD_TEMPERATURE_BLOCK_COUNT <=
                      solar::MAX_READ_REGISTER_COUNT &&
                  solar::SETTINGS_BLOCK_COUNT <= solar::MAX_READ_REGISTER_COUNT &&
                  solar::SETTINGS_TAIL_COUNT <= solar::MAX_READ_REGISTER_COUNT,
              "No read may exceed the twelve-register envelope.");
static_assert(solar::VOLTAGE_BLOCK_COUNT == 12 &&
                  solar::VOLTAGE_BLOCK_START == 0x9003 &&
                  solar::VOLTAGE_BLOCK_BYTE_COUNT == 24,
              "The written setpoint block changed.");
static_assert(solar::VOLTAGE_BLOCK_FIELDS[solar::voltage_block::FLOAT].reg ==
                      solar::REG_FLOAT_CHARGING_VOLTAGE &&
                  solar::VOLTAGE_BLOCK_FIELDS[solar::voltage_block::DISCHARGING_LIMIT]
                          .reg == solar::REG_DISCHARGING_LIMIT,
              "The setpoint block field order changed.");
static_assert(solar::voltsToRaw(13.70F) == 1370 &&
                  solar::voltsToRaw(14.40F) == 1440 &&
                  solar::voltsToRaw(11.80F) == 1180,
              "Coefficient-100 encoding changed.");

// The commissioned 12 V 9 Ah VRLA setpoints, in 0x9003..0x900E order.
constexpr uint16_t kSolarSetpoints[solar::VOLTAGE_BLOCK_COUNT] = {
    1480, 1440, 1460, 1440, 1440, 1370, 1320, 1250, 1220, 1200, 1180, 1060};
static_assert(solar::voltageBlockOrdered(kSolarSetpoints),
              "The commissioned setpoints must satisfy the ordering rule.");

// The ordering rule is the controller's, so it must reject a block the
// controller would reject: here the low-voltage disconnect sits above the boost
// reconnect voltage, which the rule requires to be strictly greater.
constexpr uint16_t kSolarUnorderedSetpoints[solar::VOLTAGE_BLOCK_COUNT] = {
    1480, 1440, 1460, 1440, 1440, 1370, 1320, 1250, 1220, 1200, 1400, 1060};
static_assert(!solar::voltageBlockOrdered(kSolarUnorderedSetpoints),
              "The ordering rule must reject an out-of-order block.");
constexpr uint16_t kSolarBoostReconnectAboveFloat[solar::VOLTAGE_BLOCK_COUNT] = {
    1480, 1440, 1460, 1440, 1440, 1370, 1600, 1250, 1220, 1200, 1180, 1060};
static_assert(!solar::voltageBlockOrdered(kSolarBoostReconnectAboveFloat),
              "The ordering rule must reject a boost reconnect above float.");

constexpr auto kSolarVoltageBlockWrite =
    solar::makeWriteVoltageBlockRequest(0x60, kSolarSetpoints);
static_assert(kSolarVoltageBlockWrite.bytes[0] == 0x60 &&
                  kSolarVoltageBlockWrite.bytes[1] == 0x10 &&
                  kSolarVoltageBlockWrite.bytes[2] == 0x90 &&
                  kSolarVoltageBlockWrite.bytes[3] == 0x03 &&
                  kSolarVoltageBlockWrite.bytes[4] == 0x00 &&
                  kSolarVoltageBlockWrite.bytes[5] == 12 &&
                  kSolarVoltageBlockWrite.bytes[6] == 24 &&
                  kSolarVoltageBlockWrite.bytes[7] == 0x05 &&
                  kSolarVoltageBlockWrite.bytes[8] == 0xC8,
              "The FC10 setpoint block must write 0x9003..0x900E, beginning "
              "with the over-voltage disconnect value.");
static_assert(sizeof(kSolarVoltageBlockWrite.bytes) == 31,
              "The FC10 setpoint frame must be 31 bytes without the CRC.");
// Regression lock computed from the encoding above, not quoted from a manual.
static_assert(modbus::crc16(kSolarVoltageBlockWrite.bytes,
                            sizeof(kSolarVoltageBlockWrite.bytes)) == 0x6EAA,
              "The FC10 setpoint block CRC changed.");

void test_solar_service_id_frames() {
  TEST_ASSERT_EQUAL_HEX16(
      0xBE89,
      modbus::crc16(kSolarServiceFindId.bytes, sizeof(kSolarServiceFindId.bytes)));
  TEST_ASSERT_EQUAL_HEX16(
      0x1488,
      modbus::crc16(kSolarServiceSetId60.bytes,
                    sizeof(kSolarServiceSetId60.bytes)));

  // Whole frames, as captured from the official PC tool.
  const uint8_t capturedFind[] = {0xF8, 0x45, 0x00, 0x01, 0x01, 0xF8, 0x89, 0xBE};
  const uint8_t capturedSet[] = {0xF8, 0x45, 0x00, 0x01, 0x01, 0x60, 0x88, 0x14};
  TEST_ASSERT_EQUAL_UINT8_ARRAY(capturedFind, kSolarServiceFindId.bytes, 6);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(capturedSet, kSolarServiceSetId60.bytes, 6);
  TEST_ASSERT_TRUE(modbus::verifyFrame(capturedFind, sizeof(capturedFind)));
  TEST_ASSERT_TRUE(modbus::verifyFrame(capturedSet, sizeof(capturedSet)));
}

void test_solar_read_request_encoding() {
  TEST_ASSERT_EQUAL_HEX16(
      0xF4BE, modbus::crc16(kSolarDocumentedInputRead.bytes,
                            sizeof(kSolarDocumentedInputRead.bytes)));
  TEST_ASSERT_EQUAL_HEX16(
      0xCB28, modbus::crc16(kSolarDocumentedHoldingRead.bytes,
                            sizeof(kSolarDocumentedHoldingRead.bytes)));
  TEST_ASSERT_EQUAL_HEX16(
      0x8676, modbus::crc16(kSolarBatteryVoltageRead.bytes,
                            sizeof(kSolarBatteryVoltageRead.bytes)));
  TEST_ASSERT_EQUAL_HEX16(
      0xBF20, modbus::crc16(kSolarSettingsBlockRead.bytes,
                            sizeof(kSolarSettingsBlockRead.bytes)));
}

void test_solar_write_request_encoding() {
  TEST_ASSERT_EQUAL_HEX16(
      0x6EAA, modbus::crc16(kSolarVoltageBlockWrite.bytes,
                            sizeof(kSolarVoltageBlockWrite.bytes)));
  TEST_ASSERT_EQUAL_UINT16(1370, solar::voltsToRaw(13.70F));
  // Twelve setpoints, big-endian, in 0x9003..0x900E order.
  TEST_ASSERT_EQUAL_UINT8(0x05, kSolarVoltageBlockWrite.bytes[7]);
  TEST_ASSERT_EQUAL_UINT8(0x5A, kSolarVoltageBlockWrite.bytes[16]);
  TEST_ASSERT_EQUAL_UINT8(0x04, kSolarVoltageBlockWrite.bytes[29]);
  TEST_ASSERT_EQUAL_UINT8(0x24, kSolarVoltageBlockWrite.bytes[30]);
}

void test_solar_setpoint_ordering_rule() {
  TEST_ASSERT_TRUE(solar::voltageBlockOrdered(kSolarSetpoints));
  TEST_ASSERT_FALSE(solar::voltageBlockOrdered(kSolarUnorderedSetpoints));
  TEST_ASSERT_FALSE(solar::voltageBlockOrdered(kSolarBoostReconnectAboveFloat));

  // Equalize may equal boost, and boost may equal float, but not exceed it.
  uint16_t equalSetpoints[solar::VOLTAGE_BLOCK_COUNT] = {1480, 1440, 1460, 1440,
                                                         1440, 1370, 1320, 1250,
                                                         1220, 1200, 1180, 1060};
  TEST_ASSERT_TRUE(solar::voltageBlockOrdered(equalSetpoints));
  equalSetpoints[solar::voltage_block::EQUALIZE] = 1450;
  TEST_ASSERT_FALSE(solar::voltageBlockOrdered(equalSetpoints));
}

void test_solar_register_decoding() {
  // 1231 counts at 0.01 V is the sketch's own example: 12.31 V.
  const uint8_t voltage[] = {0x04, 0xCF};
  TEST_ASSERT_EQUAL_UINT16(1231, solar::decodeUint16(voltage));
  TEST_ASSERT_FLOAT_WITHIN(0.0001F, 12.31F,
                           solar::decodeScaledUint16(voltage,
                                                     solar::VOLTAGE_SCALE));

  // Battery temperature is signed: a negative reading must not decode as a huge
  // positive one.
  const uint8_t negativeTemperature[] = {0xFF, 0x9C};
  TEST_ASSERT_EQUAL_INT16(-100, solar::decodeInt16(negativeTemperature));
  TEST_ASSERT_FLOAT_WITHIN(0.0001F, -1.00F,
                           solar::decodeScaledInt16(negativeTemperature,
                                                    solar::TEMPERATURE_SCALE));
}

void test_solar_profile_scaling() {
  TEST_ASSERT_FLOAT_WITHIN(
      0.0001F, 13.70F,
      solar::scaleBlockValue(solar::voltage_block::FLOAT, 1370));
  TEST_ASSERT_FLOAT_WITHIN(
      0.0001F, 11.80F,
      solar::scaleBlockValue(solar::voltage_block::LOW_VOLTAGE_DISCONNECT, 1180));
  TEST_ASSERT_FLOAT_WITHIN(0.0001F, 12.50F,
                           solar::scaleBlockValue(
                               solar::voltage_block::LOW_VOLTAGE_RECONNECT,
                               1250));
}

void setup() {
  UNITY_BEGIN();
  RUN_TEST(test_manual_forward_command_crc);
  RUN_TEST(test_parameter_address_encoding);
  RUN_TEST(test_frequency_scaling);
  RUN_TEST(test_tuf_manual_request_crc);
  RUN_TEST(test_tuf_real4_word_orders);
  RUN_TEST(test_tuf_flow_and_velocity_decode);
  RUN_TEST(test_tuf_low_word_first_flow_totals_decode);
  RUN_TEST(test_solar_service_id_frames);
  RUN_TEST(test_solar_read_request_encoding);
  RUN_TEST(test_solar_write_request_encoding);
  RUN_TEST(test_solar_setpoint_ordering_rule);
  RUN_TEST(test_solar_register_decoding);
  RUN_TEST(test_solar_profile_scaling);
  UNITY_END();
}

void loop() {}
