#include <unity.h>

#include <DelixiProtocol.h>
#include <ModbusCrc.h>
#include <Tuf2000mProtocol.h>

using irrigation::pressure_node::tuf2000m::FloatWordOrder;
namespace tuf = irrigation::pressure_node::tuf2000m::protocol;

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

void setup() {
  UNITY_BEGIN();
  RUN_TEST(test_manual_forward_command_crc);
  RUN_TEST(test_parameter_address_encoding);
  RUN_TEST(test_frequency_scaling);
  RUN_TEST(test_tuf_manual_request_crc);
  RUN_TEST(test_tuf_real4_word_orders);
  RUN_TEST(test_tuf_flow_and_velocity_decode);
  RUN_TEST(test_tuf_low_word_first_flow_totals_decode);
  UNITY_END();
}

void loop() {}
