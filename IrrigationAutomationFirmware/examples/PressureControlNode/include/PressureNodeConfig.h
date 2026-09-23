#pragma once

#include <stdint.h>

#ifndef PCV_LORAWAN_DEFAULT_INTERVAL_SECONDS
#define PCV_LORAWAN_DEFAULT_INTERVAL_SECONDS 10 // 60
#endif

#ifndef PCV_LORAWAN_MIN_INTERVAL_SECONDS
#define PCV_LORAWAN_MIN_INTERVAL_SECONDS 10 // 60
#endif

#ifndef PCV_LORAWAN_NVS_NAMESPACE
#define PCV_LORAWAN_NVS_NAMESPACE "pcv_node"
#endif

#ifndef PCV_BATTERY_DIVIDER_LOW_OHM
#define PCV_BATTERY_DIVIDER_LOW_OHM 20000.0F
#endif

#ifndef PCV_BATTERY_CALIBRATION
#define PCV_BATTERY_CALIBRATION 0.9883F
#endif

#ifndef PCV_NO_FLOW_ACTUATION_ENABLED
#define PCV_NO_FLOW_ACTUATION_ENABLED 0
#endif

namespace irrigation::pressure_node::valve_1 {

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
// The default reflects valve_1's measured 100 kOhm / 20 kOhm divider. The
// no-flow-meter build overrides it with valve_2's imported 22 kOhm assumption;
// that board still needs a multimeter calibration before voltage is trusted.
inline constexpr float DIVIDER_LOW_OHM = PCV_BATTERY_DIVIDER_LOW_OHM;
inline constexpr float CALIBRATION = PCV_BATTERY_CALIBRATION;
inline constexpr uint8_t SAMPLE_COUNT = 32;
// With 100 nF at the ADC input and about 16.7 kOhm Thevenin resistance, 10 ms
// is longer than five RC time constants.
inline constexpr uint16_t ADC_SETTLING_TIME_MS = 10;
}  // namespace battery

namespace pressure_sensor {
// These values reproduce the current prototype/trainee interpretation. The
// exact XDB401/S1204 documentation must validate them before production use.
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

// Bench starting values only. Validate on the installed latching solenoid.
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
// PlatformIO may lower these only for an explicitly marked commissioning
// build. The production-safe shared default remains 60 seconds.
inline constexpr uint32_t DEFAULT_REPORT_INTERVAL_SECONDS =
    PCV_LORAWAN_DEFAULT_INTERVAL_SECONDS;
inline constexpr uint32_t MIN_REPORT_INTERVAL_SECONDS =
    PCV_LORAWAN_MIN_INTERVAL_SECONDS;
inline constexpr uint32_t MAX_REPORT_INTERVAL_SECONDS =
    24UL * 60UL * 60UL;
inline constexpr uint32_t JOIN_RETRY_INTERVAL_MS = 60UL * 1000UL;
inline constexpr bool CONFIRMED_UPLINK = false;
inline constexpr char NVS_NAMESPACE[] = PCV_LORAWAN_NVS_NAMESPACE;
inline constexpr char NVS_NONCES_KEY[] = "nonces";
inline constexpr char NVS_STATE_KEY[] = "node_state";
}  // namespace lorawan

namespace flow_meter {
// Confirmed by the TUF-2000M technical manual: Modbus RTU function 03,
// factory serial framing 9600 8N1, REG0001-0002 flow rate in m3/h, and
// REG0005-0006 velocity in m/s. M63 must be set to MODBUS_RTU.
inline constexpr bool REGISTER_MAP_CONFIRMED = true;
inline constexpr uint32_t BAUD = 9600;
inline constexpr uint16_t RESPONSE_TIMEOUT_MS = 300;

// Confirmed on the commissioned TUF-2000M front panel (M46).
inline constexpr uint8_t SLAVE_ADDRESS = 1;

// Hardware-verified on the commissioned meter: REG0221..REG0222 returned
// 00 00 42 64, which decodes to the configured 57.0 mm inner diameter only
// when the two 16-bit Modbus words are swapped before IEEE-754 decoding.
inline constexpr bool FLOAT_WORD_ORDER_VALIDATED = true;
inline constexpr bool LOW_WORD_FIRST = true;

// GPIO16/17 are reserved for the auto-direction RS-485 converter. Normal
// on-demand flow reads and the explicit diagnostic probe use this transport.
inline constexpr bool CURRENT_RS485_PINS_AVAILABLE = true;
inline constexpr int8_t UART_RX = pins::RS485_RX;
inline constexpr int8_t UART_TX = pins::RS485_TX;
inline constexpr bool AUTOMATIC_DIRECTION = true;
inline constexpr int8_t DE_RE = -1;
inline constexpr bool DE_RE_ACTIVE_HIGH_TX = true;
}  // namespace flow_meter

// EPEVER LandStar LS1024B solar charge controller. Commissioned as Modbus RTU
// slave 0x60 at 115200 8N1; the controller is powered directly from the battery,
// so the firmware only owns the RS-485 path. Register provenance, including what
// has and has not been confirmed on the installed controller, is in
// docs/EPEVER_LS1024B.md.
// The solar test build is a bench-only target: it hands the RS-485 branch to the
// LS1024B and compiles the TUF-2000M out. Every other target leaves this 0.
#ifndef PCV_SOLAR_TEST
#define PCV_SOLAR_TEST 0
#endif

#ifndef SOLAR_CONTROLLER_WRITES_ENABLED
#define SOLAR_CONTROLLER_WRITES_ENABLED 0
#endif

namespace solar_controller {
inline constexpr uint8_t SLAVE_ADDRESS = 0x60;
inline constexpr uint32_t BAUD = 115200;
inline constexpr uint16_t RESPONSE_TIMEOUT_MS = 300;

// GPIO16/17 is the node's single RS-485 branch, so only one device may own it in
// a given build: the TUF-2000M at 9600 or the LS1024B at 115200. The solar test
// build compiles the flow meter out instead of re-bauding the trunk.
inline constexpr int8_t UART_RX = pins::RS485_RX;
inline constexpr int8_t UART_TX = pins::RS485_TX;
inline constexpr bool AUTOMATIC_DIRECTION = true;
inline constexpr int8_t DE_RE = -1;
inline constexpr bool DE_RE_ACTIVE_HIGH_TX = true;

// Charge-setting writes are compiled out unless the build opts in, and the
// serial command must still end in CONFIRM. See docs/SERIAL_COMMANDS.md.
inline constexpr bool WRITES_ENABLED = SOLAR_CONTROLLER_WRITES_ENABLED != 0;

// Preconditions for applying the charge profile. The twelve setpoints below are
// user-defined-battery setpoints, so the controller must already be configured
// as a User battery on a 12 V system; anything else aborts before a write.
inline constexpr uint16_t EXPECTED_BATTERY_TYPE = 0x0000;  // User
inline constexpr uint16_t EXPECTED_RATED_VOLTAGE_LEVEL = 0x0001;  // 12 V

// Single 12 V 9 Ah VRLA battery, 25 W PV. These are the setpoints of the current
// LS1024B configuration sketch and they deliberately replace the older profile
// that treated two 12 V 9 Ah batteries in parallel as one 18 Ah bank.
//
// They are written as one 0x9003..0x900E block and must stay mutually ordered;
// the ordering rule is the library's and is asserted next to the profile in
// main.cpp. Battery type, capacity, temperature compensation, rated voltage, and
// maximum charging current are NOT written by this firmware: the controller is
// expected to already hold the correct values for them.
inline constexpr float PROFILE_OVER_VOLTAGE_DISCONNECT_V = 14.80F;
inline constexpr float PROFILE_CHARGING_LIMIT_V = 14.40F;
inline constexpr float PROFILE_OVER_VOLTAGE_RECONNECT_V = 14.60F;
inline constexpr float PROFILE_EQUALIZE_V = 14.40F;
inline constexpr float PROFILE_BOOST_V = 14.40F;
inline constexpr float PROFILE_FLOAT_V = 13.70F;
inline constexpr float PROFILE_BOOST_RECONNECT_V = 13.20F;
inline constexpr float PROFILE_LOW_VOLTAGE_RECONNECT_V = 12.50F;
inline constexpr float PROFILE_UNDER_VOLTAGE_RECOVER_V = 12.20F;
inline constexpr float PROFILE_UNDER_VOLTAGE_WARNING_V = 12.00F;
inline constexpr float PROFILE_LOW_VOLTAGE_DISCONNECT_V = 11.80F;
inline constexpr float PROFILE_DISCHARGING_LIMIT_V = 10.60F;
// For reference only, not written by this firmware: 9 Ah, about 2 A maximum
// charge current, and equalization duration zero for a VRLA battery.
}  // namespace solar_controller

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
static_assert(solar_controller::UART_RX != solar_controller::UART_TX,
              "The solar-controller RS-485 UART needs distinct pins.");
// The charge-setpoint ordering rule lives with the device protocol
// (voltageBlockOrdered) and is asserted against the assembled profile in
// examples/PressureControlNode/src/main.cpp, so there is one rule, not two.
}  // namespace irrigation::pressure_node::valve_1
