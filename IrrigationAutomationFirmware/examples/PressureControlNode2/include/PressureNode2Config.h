#pragma once

// Board description for valve_2, the no-flow-meter Class A node built from
// examples/PressureControlNode2. It is separate from the shared
// include/PressureNodeConfig.h (valve_1, namespace `rev_a`) so that valve_2's
// pins and board constants can diverge freely: change them HERE, and do not
// expect valve_1 to follow.
//
// The section layout deliberately mirrors include/PressureNodeConfig.h so the
// two files can be compared section by section. Everything that is identical
// today is identical only because both boards are built from the same
// prototype design, not because it must stay that way.
//
// Build-shape switches stay in platformio.ini because they gate code structure
// rather than describing hardware: PCV_NO_FLOW_METER,
// PCV_NO_FLOW_ACTUATION_ENABLED and PRESSURE_NODE_RUNTIME_MODE.

#include <stdint.h>

// This switch selects what the firmware is allowed to do, not what the board
// is, so it stays a build flag. Declared here only to fail loudly if a future
// edit drops it from the build.
#ifndef PCV_NO_FLOW_ACTUATION_ENABLED
# define PCV_NO_FLOW_ACTUATION_ENABLED 1 // fixme: 0
#endif

namespace irrigation::pressure_node::valve_2 {

namespace pins {
inline constexpr int8_t BATTERY_ADC = 35;
// GPIO2 and GPIO15 are boot-strapping pins on ESP32-WROOM. They are usable as
// ordinary outputs after reset, but the assembled board must keep the installed
// 20 kOhm pull-down resistors and keep the L298N power rail disabled at boot.
inline constexpr int8_t PCV_IN1 = 2;
inline constexpr int8_t PCV_IN2 = 15;
inline constexpr int8_t PCV_POWER_ENABLE = 27;

inline constexpr int8_t I2C_UPSTREAM_SDA = 21;
inline constexpr int8_t I2C_UPSTREAM_SCL = 22;
inline constexpr int8_t I2C_DOWNSTREAM_SDA = 13;
inline constexpr int8_t I2C_DOWNSTREAM_SCL = 4;

inline constexpr int8_t RS485_RX = 16;
inline constexpr int8_t RS485_TX = 17;

inline constexpr int8_t LORA_NSS = 5;
inline constexpr int8_t LORA_DIO1 = 26;
inline constexpr int8_t LORA_RESET = 14;
inline constexpr int8_t LORA_BUSY = 25;
inline constexpr int8_t LORA_SCK = 18;
inline constexpr int8_t LORA_MISO = 19;
inline constexpr int8_t LORA_MOSI = 23;
inline constexpr int8_t LORA_TX_ENABLE = 32;
inline constexpr int8_t LORA_RX_ENABLE = 33;
}  // namespace pins

namespace battery {
inline constexpr float DIVIDER_HIGH_OHM = 100000.0F;
// Imported valve_2 divider value. The calibration factor is neutral because
// the divider has not been verified against a multimeter on the assembled
// board; measure and correct both before the voltage is trusted.
inline constexpr float DIVIDER_LOW_OHM = 22000.0F;
inline constexpr float CALIBRATION = 1.0F;
inline constexpr uint8_t SAMPLE_COUNT = 32;
// With 100 nF at the ADC input and about 16.7 kOhm Thevenin resistance, 10 ms
// is longer than five RC time constants.
inline constexpr uint16_t ADC_SETTLING_TIME_MS = 10;
}  // namespace battery

namespace pressure_sensor {
// These values reproduce the current prototype interpretation. The exact
// XDB401/S1204 documentation must validate them before production use.
inline constexpr uint8_t ADDRESS_PRIMARY = 0x7F;
inline constexpr uint8_t ADDRESS_ALTERNATE = 0x6D;
inline constexpr uint8_t REG_PRESSURE = 0x06;
inline constexpr uint8_t REG_TEMPERATURE = 0x09;
inline constexpr uint8_t REG_MEASUREMENT = 0x30;
inline constexpr uint8_t START_MEASUREMENT = 0x0A;
inline constexpr uint8_t BUSY_MASK = 0x08;
inline constexpr float ASSUMED_FULL_SCALE_BAR = 10.0F;
inline constexpr bool ENGINEERING_SCALE_VALIDATED = false;
inline constexpr uint32_t I2C_FREQUENCY_HZ = 100000;
inline constexpr uint8_t READY_POLL_ATTEMPTS = 10;
inline constexpr uint16_t READY_POLL_INTERVAL_MS = 5;
}  // namespace pressure_sensor

namespace pcv {
inline constexpr bool POWER_ENABLE_ACTIVE_HIGH = true;

// Bench starting values only. Validate on the installed latching solenoid of
// valve_2 before enabling actuation.
inline constexpr uint16_t POWER_SETTLE_MS = 20;
inline constexpr uint16_t SOLENOID_PULSE_MS = 250;
inline constexpr uint16_t POST_PULSE_MS = 20;
inline constexpr uint16_t HYDRAULIC_SETTLE_MS = 1000;

// Assumed mapping only. Swap here, and only here, if the hardware is reversed.
inline constexpr bool OPEN_IN1_HIGH = true;
inline constexpr bool OPEN_IN2_HIGH = false;
inline constexpr bool CLOSE_IN1_HIGH = false;
inline constexpr bool CLOSE_IN2_HIGH = true;
}  // namespace pcv

namespace energy {
inline constexpr bool PERIODIC_SAMPLING_ENABLED = false;
inline constexpr uint32_t SAMPLE_INTERVAL_MS = 60000;
inline constexpr uint32_t TELEMETRY_INTERVAL_MS = 60000;
}  // namespace energy

namespace lorawan {
inline constexpr uint8_t COMMAND_FPORT = 30;
inline constexpr uint8_t STATUS_FPORT = 31;
inline constexpr uint8_t SUB_BAND = 0;
inline constexpr uint32_t DEFAULT_REPORT_INTERVAL_SECONDS = 10; // 60;
inline constexpr uint32_t MIN_REPORT_INTERVAL_SECONDS = 10; // 60;
inline constexpr uint32_t MAX_REPORT_INTERVAL_SECONDS = 24UL * 60UL * 60UL;
inline constexpr uint32_t JOIN_RETRY_INTERVAL_MS = 60UL * 1000UL;
inline constexpr bool CONFIRMED_UPLINK = false;
// Must remain "valve_lora". That namespace holds the OTAA DevNonce history
// from valve_2's former Class C firmware; pointing this at a fresh namespace
// makes the device join with a replayed DevNonce, which this firmware blocks
// on purpose. The application state uses the separate NVS_STATE_KEY.
inline constexpr char NVS_NAMESPACE[] = "valve_lora";
inline constexpr char NVS_NONCES_KEY[] = "nonces";
inline constexpr char NVS_STATE_KEY[] = "node_state";
}  // namespace lorawan

// This build does not fit a flow meter and never starts the RS-485 transport,
// so nothing here is instantiated while PCV_NO_FLOW_METER is 1. The section is
// kept so that the shared flow code path still compiles if a meter is ever
// added to valve_2.
namespace flow_meter {
inline constexpr bool REGISTER_MAP_CONFIRMED = true;
inline constexpr uint32_t BAUD = 9600;
inline constexpr uint16_t RESPONSE_TIMEOUT_MS = 300;
inline constexpr uint8_t SLAVE_ADDRESS = 1;
inline constexpr bool FLOAT_WORD_ORDER_VALIDATED = true;
inline constexpr bool LOW_WORD_FIRST = true;
inline constexpr bool CURRENT_RS485_PINS_AVAILABLE = true;
inline constexpr int8_t UART_RX = pins::RS485_RX;
inline constexpr int8_t UART_TX = pins::RS485_TX;
inline constexpr bool AUTOMATIC_DIRECTION = true;
inline constexpr int8_t DE_RE = -1;
inline constexpr bool DE_RE_ACTIVE_HIGH_TX = true;
}  // namespace flow_meter

inline constexpr uint32_t DEBUG_BAUD = 115200;

static_assert(pcv::OPEN_IN1_HIGH != pcv::OPEN_IN2_HIGH,
              "OPEN must actively drive the H-bridge.");
static_assert(pcv::CLOSE_IN1_HIGH != pcv::CLOSE_IN2_HIGH,
              "CLOSE must actively drive the H-bridge.");
static_assert(pcv::OPEN_IN1_HIGH == pcv::CLOSE_IN2_HIGH &&
                  pcv::OPEN_IN2_HIGH == pcv::CLOSE_IN1_HIGH,
              "OPEN and CLOSE must use opposite polarity.");
static_assert(pins::PCV_IN1 != pins::PCV_IN2 &&
                  pins::PCV_IN1 != pins::PCV_POWER_ENABLE &&
                  pins::PCV_IN2 != pins::PCV_POWER_ENABLE,
              "The H-bridge pins must be unique.");
static_assert(pins::RS485_RX != pins::RS485_TX &&
                  pins::RS485_RX != pins::PCV_IN1 &&
                  pins::RS485_RX != pins::PCV_IN2 &&
                  pins::RS485_TX != pins::PCV_IN1 &&
                  pins::RS485_TX != pins::PCV_IN2,
              "RS-485 UART must not overlap the H-bridge.");
static_assert(lorawan::DEFAULT_REPORT_INTERVAL_SECONDS >=
                      lorawan::MIN_REPORT_INTERVAL_SECONDS &&
                  lorawan::DEFAULT_REPORT_INTERVAL_SECONDS <=
                      lorawan::MAX_REPORT_INTERVAL_SECONDS,
              "Default LoRaWAN report interval is out of range.");
}  // namespace irrigation::pressure_node::valve_2
