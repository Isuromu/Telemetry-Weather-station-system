#pragma once

#include <stddef.h>
#include <stdint.h>

// EPEVER LandStar LS1024B protocol facts.
//
// Provenance of every address in this file is recorded in
// docs/EPEVER_LS1024B.md. Nothing here is inferred from the device name:
// - the real-time block and the settings addresses come from the LS1024B
//   configuration sketches, which read battery voltage (0x3104), battery
//   temperature (0x3110), load current (0x310D), and battery status (0x3200) on
//   the installed controller;
// - the write method is the one those sketches use: a single FC10 block write of
//   0x9003..0x900E, which the profile requires because the twelve setpoints are
//   mutually constrained and must change together;
// - the 0x45 service command comes from frames captured from the official PC
//   tool.
//
// The 0x9000 settings base has NOT been confirmed against an LS1024B document.
// Treat every settings write as a bench experiment that must be checked by
// read-back and against the controller's own display.

#ifndef SOLAR_CONTROLLER_WRITES_ENABLED
#define SOLAR_CONTROLLER_WRITES_ENABLED 0
#endif

namespace irrigation::pressure_node::epever_ls1024b {

namespace protocol {

// ---------------------------------------------------------------- function codes

inline constexpr uint8_t READ_HOLDING_REGISTERS = 0x03;
inline constexpr uint8_t READ_INPUT_REGISTERS = 0x04;
inline constexpr uint8_t WRITE_MULTIPLE_REGISTERS = 0x10;

// Function 0x06 (write single register) is deliberately absent. The charge
// profile requires the complete 0x9003..0x900E block to be written together, and
// this firmware has no other single-register write.

// Proprietary EPEVER service command. Not Modbus: no register, no standard
// response, and the frame is broadcast rather than addressed to the slave.
inline constexpr uint8_t SERVICE_COMMAND = 0x45;
inline constexpr uint8_t SERVICE_BROADCAST_ADDRESS = 0xF8;
// "Find/read the current ID" marker in the last payload byte. The official PC
// tool sends the same frame shape with the new address in that byte to set it.
inline constexpr uint8_t SERVICE_FIND_ID_MARKER = 0xF8;

inline constexpr size_t READ_REQUEST_LENGTH_WITHOUT_CRC = 6;
inline constexpr size_t SERVICE_REQUEST_LENGTH_WITHOUT_CRC = 6;

// The only working reference implementation for this device never issues a read
// longer than twelve registers, so the driver stays inside that envelope.
inline constexpr uint16_t MAX_READ_REGISTER_COUNT = 12;

// ------------------------------------------------------- real-time block (FC04)

// The installed controller answers FC04 reads for these input registers.
inline constexpr uint16_t REG_PV_VOLTAGE = 0x3100;
inline constexpr uint16_t REG_PV_CURRENT = 0x3101;
inline constexpr uint16_t REG_BATTERY_VOLTAGE = 0x3104;
inline constexpr uint16_t REG_CHARGING_CURRENT = 0x3105;
inline constexpr uint16_t REG_LOAD_VOLTAGE = 0x310C;
inline constexpr uint16_t REG_LOAD_CURRENT = 0x310D;
inline constexpr uint16_t REG_BATTERY_TEMPERATURE = 0x3110;
inline constexpr uint16_t REG_DEVICE_TEMPERATURE = 0x3111;
inline constexpr uint16_t REG_BATTERY_SOC = 0x311A;
inline constexpr uint16_t REG_BATTERY_STATUS = 0x3200;
inline constexpr uint16_t REG_CHARGING_STATUS = 0x3201;

// Two transfers cover the real-time values and both stay within the twelve
// register envelope: 0x3100..0x310B (PV and battery) and 0x310C..0x3111 (load
// and temperatures).
inline constexpr uint16_t PV_BATTERY_BLOCK_START = REG_PV_VOLTAGE;
inline constexpr uint16_t PV_BATTERY_BLOCK_COUNT = 12;
inline constexpr uint8_t PV_BATTERY_BLOCK_BYTE_COUNT =
    static_cast<uint8_t>(PV_BATTERY_BLOCK_COUNT * 2);

inline constexpr uint16_t LOAD_TEMPERATURE_BLOCK_START = REG_LOAD_VOLTAGE;
inline constexpr uint16_t LOAD_TEMPERATURE_BLOCK_COUNT = 6;
inline constexpr uint8_t LOAD_TEMPERATURE_BLOCK_BYTE_COUNT =
    static_cast<uint8_t>(LOAD_TEMPERATURE_BLOCK_COUNT * 2);

inline constexpr uint16_t STATUS_BLOCK_START = REG_BATTERY_STATUS;
inline constexpr uint16_t STATUS_BLOCK_COUNT = 2;
inline constexpr uint8_t STATUS_BLOCK_BYTE_COUNT =
    static_cast<uint8_t>(STATUS_BLOCK_COUNT * 2);

static_assert(PV_BATTERY_BLOCK_COUNT <= MAX_READ_REGISTER_COUNT &&
                  LOAD_TEMPERATURE_BLOCK_COUNT <= MAX_READ_REGISTER_COUNT,
              "Real-time reads must stay within the twelve-register envelope.");

// Battery voltage and battery temperature are the two registers already proven
// on the installed controller. The rest of the map is read from the same source
// and validated by comparison, not assumed.
inline constexpr bool BATTERY_VOLTAGE_AND_TEMPERATURE_HARDWARE_CONFIRMED = true;
inline constexpr bool REALTIME_BLOCK_HARDWARE_CONFIRMED = false;

// -------------------------------------------------------- setting registers

inline constexpr uint16_t REG_BATTERY_TYPE = 0x9000;
inline constexpr uint16_t REG_BATTERY_CAPACITY = 0x9001;
inline constexpr uint16_t REG_TEMPERATURE_COMPENSATION = 0x9002;
inline constexpr uint16_t REG_OVER_VOLTAGE_DISCONNECT = 0x9003;
inline constexpr uint16_t REG_CHARGING_LIMIT_VOLTAGE = 0x9004;
inline constexpr uint16_t REG_OVER_VOLTAGE_RECONNECT = 0x9005;
inline constexpr uint16_t REG_EQUALIZE_CHARGING_VOLTAGE = 0x9006;
inline constexpr uint16_t REG_BOOST_CHARGING_VOLTAGE = 0x9007;
inline constexpr uint16_t REG_FLOAT_CHARGING_VOLTAGE = 0x9008;
inline constexpr uint16_t REG_BOOST_RECONNECT_VOLTAGE = 0x9009;
inline constexpr uint16_t REG_LOW_VOLTAGE_RECONNECT = 0x900A;
inline constexpr uint16_t REG_UNDER_VOLTAGE_RECOVER = 0x900B;
inline constexpr uint16_t REG_UNDER_VOLTAGE_WARNING = 0x900C;
inline constexpr uint16_t REG_LOW_VOLTAGE_DISCONNECT = 0x900D;
inline constexpr uint16_t REG_DISCHARGING_LIMIT = 0x900E;
inline constexpr uint16_t REG_LOAD_CONTROL_MODE = 0x903D;
inline constexpr uint16_t REG_RATED_VOLTAGE_LEVEL = 0x9067;
inline constexpr uint16_t REG_MAX_CHARGING_CURRENT = 0x90BF;

// One transfer covers 0x9000..0x900B, a second covers 0x900C..0x900E. Reading
// the settings as one fifteen-register block would exceed the envelope the
// reference implementation stays inside.
inline constexpr uint16_t SETTINGS_BLOCK_START = REG_BATTERY_TYPE;
inline constexpr uint16_t SETTINGS_BLOCK_COUNT = 12;
inline constexpr uint8_t SETTINGS_BLOCK_BYTE_COUNT =
    static_cast<uint8_t>(SETTINGS_BLOCK_COUNT * 2);

inline constexpr uint16_t SETTINGS_TAIL_START = REG_UNDER_VOLTAGE_WARNING;
inline constexpr uint16_t SETTINGS_TAIL_COUNT = 3;
inline constexpr uint8_t SETTINGS_TAIL_BYTE_COUNT =
    static_cast<uint8_t>(SETTINGS_TAIL_COUNT * 2);

static_assert(SETTINGS_BLOCK_COUNT <= MAX_READ_REGISTER_COUNT &&
                  SETTINGS_TAIL_COUNT <= MAX_READ_REGISTER_COUNT,
              "Settings reads must stay within the twelve-register envelope.");
static_assert(SETTINGS_BLOCK_START + SETTINGS_BLOCK_COUNT == SETTINGS_TAIL_START,
              "The settings reads must cover the block without a gap.");
static_assert(SETTINGS_TAIL_START + SETTINGS_TAIL_COUNT - 1 ==
                  REG_DISCHARGING_LIMIT,
              "The settings reads must end at 0x900E.");

inline constexpr bool SETTINGS_MAP_HARDWARE_CONFIRMED = false;

// ------------------------------------------------------------------- encoding

inline constexpr uint16_t BATTERY_TYPE_USER = 0x0000;
inline constexpr uint16_t BATTERY_TYPE_AGM = 0x0001;
inline constexpr uint16_t BATTERY_TYPE_GEL = 0x0002;
inline constexpr uint16_t BATTERY_TYPE_FLOODED = 0x0003;

inline constexpr uint16_t RATED_VOLTAGE_AUTO = 0x0000;
inline constexpr uint16_t RATED_VOLTAGE_12V = 0x0001;
inline constexpr uint16_t RATED_VOLTAGE_24V = 0x0002;

// Battery voltages and currents are reported and written with a coefficient of
// 100: 1370 means 13.70 V, 200 means 2.00 A.
inline constexpr uint16_t COEFFICIENT_100 = 100;
inline constexpr float VOLTAGE_SCALE = 1.0F / COEFFICIENT_100;
inline constexpr float CURRENT_SCALE = 1.0F / COEFFICIENT_100;
inline constexpr float TEMPERATURE_SCALE = 1.0F / COEFFICIENT_100;
inline constexpr float SOC_SCALE = 1.0F;

constexpr uint16_t voltsToRaw(float volts) {
  return static_cast<uint16_t>(volts * COEFFICIENT_100 + 0.5F);
}

// --------------------------------------------------- voltage block (FC10 write)

// The twelve mutually constrained setpoints, in the order the controller
// expects them at 0x9003..0x900E. Only this block is ever written.
inline constexpr uint16_t VOLTAGE_BLOCK_START = REG_OVER_VOLTAGE_DISCONNECT;
inline constexpr uint16_t VOLTAGE_BLOCK_COUNT = 12;
inline constexpr uint8_t VOLTAGE_BLOCK_BYTE_COUNT =
    static_cast<uint8_t>(VOLTAGE_BLOCK_COUNT * 2);

namespace voltage_block {
inline constexpr size_t OVER_VOLTAGE_DISCONNECT = 0;
inline constexpr size_t CHARGING_LIMIT = 1;
inline constexpr size_t OVER_VOLTAGE_RECONNECT = 2;
inline constexpr size_t EQUALIZE = 3;
inline constexpr size_t BOOST = 4;
inline constexpr size_t FLOAT = 5;
inline constexpr size_t BOOST_RECONNECT = 6;
inline constexpr size_t LOW_VOLTAGE_RECONNECT = 7;
inline constexpr size_t UNDER_VOLTAGE_RECOVER = 8;
inline constexpr size_t UNDER_VOLTAGE_WARNING = 9;
inline constexpr size_t LOW_VOLTAGE_DISCONNECT = 10;
inline constexpr size_t DISCHARGING_LIMIT = 11;
}  // namespace voltage_block

// The controller rejects mutually inconsistent setpoints, so the block is
// checked before it is sent. This is the ordering rule from the current
// configuration sketch, kept verbatim rather than re-derived. Note that it does
// not constrain the over-voltage reconnect against the charging limit.
constexpr bool voltageBlockOrdered(const uint16_t (&v)[VOLTAGE_BLOCK_COUNT]) {
  using namespace voltage_block;
  return v[OVER_VOLTAGE_DISCONNECT] > v[CHARGING_LIMIT] &&
         v[CHARGING_LIMIT] >= v[EQUALIZE] && v[EQUALIZE] >= v[BOOST] &&
         v[BOOST] >= v[FLOAT] && v[FLOAT] > v[BOOST_RECONNECT] &&
         v[OVER_VOLTAGE_DISCONNECT] > v[OVER_VOLTAGE_RECONNECT] &&
         v[LOW_VOLTAGE_RECONNECT] > v[LOW_VOLTAGE_DISCONNECT] &&
         v[LOW_VOLTAGE_DISCONNECT] >= v[DISCHARGING_LIMIT] &&
         v[UNDER_VOLTAGE_RECOVER] > v[UNDER_VOLTAGE_WARNING] &&
         v[UNDER_VOLTAGE_WARNING] >= v[DISCHARGING_LIMIT] &&
         v[BOOST_RECONNECT] > v[LOW_VOLTAGE_DISCONNECT];
}

struct VoltageBlockField {
  uint16_t reg;
  size_t index;
  const char *label;
};

inline constexpr VoltageBlockField VOLTAGE_BLOCK_FIELDS[VOLTAGE_BLOCK_COUNT] = {
    {REG_OVER_VOLTAGE_DISCONNECT, voltage_block::OVER_VOLTAGE_DISCONNECT,
     "Over-voltage disconnect (V)"},
    {REG_CHARGING_LIMIT_VOLTAGE, voltage_block::CHARGING_LIMIT,
     "Charging limit (V)"},
    {REG_OVER_VOLTAGE_RECONNECT, voltage_block::OVER_VOLTAGE_RECONNECT,
     "Over-voltage reconnect (V)"},
    {REG_EQUALIZE_CHARGING_VOLTAGE, voltage_block::EQUALIZE,
     "Equalize charging (V)"},
    {REG_BOOST_CHARGING_VOLTAGE, voltage_block::BOOST, "Boost charging (V)"},
    {REG_FLOAT_CHARGING_VOLTAGE, voltage_block::FLOAT, "Float charging (V)"},
    {REG_BOOST_RECONNECT_VOLTAGE, voltage_block::BOOST_RECONNECT,
     "Boost reconnect (V)"},
    {REG_LOW_VOLTAGE_RECONNECT, voltage_block::LOW_VOLTAGE_RECONNECT,
     "Low-voltage reconnect (V)"},
    {REG_UNDER_VOLTAGE_RECOVER, voltage_block::UNDER_VOLTAGE_RECOVER,
     "Under-voltage recover (V)"},
    {REG_UNDER_VOLTAGE_WARNING, voltage_block::UNDER_VOLTAGE_WARNING,
     "Under-voltage warning (V)"},
    {REG_LOW_VOLTAGE_DISCONNECT, voltage_block::LOW_VOLTAGE_DISCONNECT,
     "Low-voltage disconnect (V)"},
    {REG_DISCHARGING_LIMIT, voltage_block::DISCHARGING_LIMIT,
     "Discharging limit (V)"},
};

// The table is the single source of order for writing, reading back, and
// printing, so a duplicated, mis-indexed, or out-of-order entry would silently
// mis-pair registers and values.
constexpr bool voltageBlockFieldTableValid() {
  for (size_t i = 0; i < VOLTAGE_BLOCK_COUNT; ++i) {
    if (VOLTAGE_BLOCK_FIELDS[i].label == nullptr) return false;
    if (VOLTAGE_BLOCK_FIELDS[i].index != i) return false;
    if (VOLTAGE_BLOCK_FIELDS[i].reg != VOLTAGE_BLOCK_START + i) return false;
  }
  return true;
}

static_assert(voltageBlockFieldTableValid(),
              "The voltage block table must map 0x9003..0x900E in order.");

struct SolarVoltageBlockProfile {
  // Raw register units in VOLTAGE_BLOCK_FIELDS order, so nothing is silently
  // scaled on the way to the controller.
  uint16_t values[VOLTAGE_BLOCK_COUNT]{};
};

constexpr float scaleBlockValue(size_t index, uint16_t rawValue) {
  return static_cast<float>(rawValue) * VOLTAGE_SCALE;
}

// ----------------------------------------------------------- request builders

struct ReadRegistersRequest {
  uint8_t bytes[READ_REQUEST_LENGTH_WITHOUT_CRC]{};
};

constexpr ReadRegistersRequest makeReadRegistersRequest(uint8_t slaveAddress,
                                                        uint8_t function,
                                                        uint16_t startAddress,
                                                        uint16_t registerCount) {
  return {{slaveAddress,
           function,
           static_cast<uint8_t>(startAddress >> 8U),
           static_cast<uint8_t>(startAddress & 0xFFU),
           static_cast<uint8_t>(registerCount >> 8U),
           static_cast<uint8_t>(registerCount & 0xFFU)}};
}

// Header six bytes, one byte count, then two bytes per register. Twelve
// registers make a 31-byte frame, which stays inside the transport's transmit
// buffer.
inline constexpr size_t WRITE_VOLTAGE_BLOCK_REQUEST_LENGTH =
    6 + 1 + VOLTAGE_BLOCK_BYTE_COUNT;
static_assert(WRITE_VOLTAGE_BLOCK_REQUEST_LENGTH == 31,
              "The voltage-block write frame must hold twelve registers.");

struct WriteVoltageBlockRequest {
  uint8_t bytes[WRITE_VOLTAGE_BLOCK_REQUEST_LENGTH]{};
};

constexpr WriteVoltageBlockRequest makeWriteVoltageBlockRequest(
    uint8_t slaveAddress, const uint16_t (&values)[VOLTAGE_BLOCK_COUNT]) {
  WriteVoltageBlockRequest request{};
  request.bytes[0] = slaveAddress;
  request.bytes[1] = WRITE_MULTIPLE_REGISTERS;
  request.bytes[2] = static_cast<uint8_t>(VOLTAGE_BLOCK_START >> 8U);
  request.bytes[3] = static_cast<uint8_t>(VOLTAGE_BLOCK_START & 0xFFU);
  request.bytes[4] = 0;
  request.bytes[5] = static_cast<uint8_t>(VOLTAGE_BLOCK_COUNT);
  request.bytes[6] = VOLTAGE_BLOCK_BYTE_COUNT;
  for (size_t i = 0; i < VOLTAGE_BLOCK_COUNT; ++i) {
    request.bytes[7 + i * 2] = static_cast<uint8_t>(values[i] >> 8U);
    request.bytes[8 + i * 2] = static_cast<uint8_t>(values[i] & 0xFFU);
  }
  return request;
}

struct ServiceRequest {
  uint8_t bytes[SERVICE_REQUEST_LENGTH_WITHOUT_CRC]{};
};

constexpr ServiceRequest makeServiceRequest(uint8_t payloadByte) {
  return {{SERVICE_BROADCAST_ADDRESS, SERVICE_COMMAND, 0x00, 0x01, 0x01,
           payloadByte}};
}

constexpr ServiceRequest makeServiceFindIdRequest() {
  return makeServiceRequest(SERVICE_FIND_ID_MARKER);
}

constexpr ServiceRequest makeServiceSetIdRequest(uint8_t newAddress) {
  return makeServiceRequest(newAddress);
}

// ------------------------------------------------------------------ decoders

constexpr uint16_t decodeUint16(const uint8_t *bytes) {
  return static_cast<uint16_t>((static_cast<uint16_t>(bytes[0]) << 8U) |
                               static_cast<uint16_t>(bytes[1]));
}

// Battery and device temperature are signed 16-bit values at 0.01 C, so a
// negative reading must not be read as a huge positive one.
constexpr int16_t decodeInt16(const uint8_t *bytes) {
  return static_cast<int16_t>(decodeUint16(bytes));
}

constexpr float decodeScaledUint16(const uint8_t *bytes, float scale) {
  return static_cast<float>(decodeUint16(bytes)) * scale;
}

constexpr float decodeScaledInt16(const uint8_t *bytes, float scale) {
  return static_cast<float>(decodeInt16(bytes)) * scale;
}

// ------------------------------------------------------------- address change

// Captured from the official EPEVER PC tool. Both frames are asserted against
// these exact bytes in test/test_protocol.
//   find: F8 45 00 01 01 F8 89 BE
//   set : F8 45 00 01 01 60 88 14   (new address 0x60)
inline constexpr uint8_t SERVICE_MAX_CAPTURED_RESPONSE = 32;

}  // namespace protocol
}  // namespace irrigation::pressure_node::epever_ls1024b
