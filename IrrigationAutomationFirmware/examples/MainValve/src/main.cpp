/*
 * ESP32-WROOM + LR-DX30/SX1262 + FOSD-05E Modbus actuator + 2302X I2C
 * pressure sensor.
 *
 * Local serial commands (115200 baud, Newline ending):
 *   status
 *   angle 30
 *   angle 45
 *   percent 50
 *   close
 *   open
 *   bus
 *   clear
 *   calibrate CONFIRM
 *   reset CONFIRM
 *
 * Remote LoRaWAN command, FPort 30, 6 bytes:
 *   [version=1, command=1, angle_x10_MSB, angle_x10_LSB,
 *    command_id_MSB, command_id_LSB]
 *
 * Status uplink, FPort 31, 15 bytes. Install the matching ChirpStack codec.
 *
 * RadioLib 7.7.1, LoRaWAN 1.0.x. This mains-powered controller requests
 * Class C. If the confirmed Class-C activation uplink is not acknowledged,
 * it continues as Class A and still checks downlinks after status uplinks.
 *
 * DANGER: the actuator uses 220 VAC. The ESP32 and RS485 transceiver must be
 * powered by the isolated 5 V output of a certified AC/DC supply. Never put
 * mains voltage on an ESP32 pin, ground, breadboard, or RS485 data terminal.
 */

#include <Arduino.h>
#include <HardwareSerial.h>
#include <Preferences.h>
#include <RadioLib.h>
#include <SPI.h>
#include <Wire.h>

#include "MainValveConfig.h"

#if __has_include("MainValveLoRaSecrets.h")
#include "MainValveLoRaSecrets.h"
#else
#include "MainValveLoRaSecrets.example.h"
#endif

namespace config = irrigation::main_valve::config;
namespace secrets = irrigation::main_valve::lorawan_secrets;

// ----------------------- User configuration -----------------------
constexpr bool LORAWAN_CREDENTIALS_CONFIGURED = secrets::CONFIGURED;
constexpr uint64_t LORAWAN_JOIN_EUI = secrets::JOIN_EUI;
constexpr uint64_t LORAWAN_DEV_EUI = secrets::DEV_EUI;
uint8_t *const LORAWAN_APP_KEY = secrets::APP_KEY;
// Match this to the ChirpStack device profile and gateway frequency plan.
const LoRaWANBand_t &LORAWAN_REGION = config::lorawan::REGION;
constexpr uint8_t LORAWAN_SUB_BAND = config::lorawan::SUB_BAND;

// The manual permits addresses 1-25. Set menu U14 to the same value.
constexpr uint8_t ACTUATOR_MODBUS_ADDRESS = 1;

// A quarter-turn butterfly valve normally has 90 degrees of calibrated travel.
constexpr float VALVE_TRAVEL_DEGREES = 90.0f;

// The pressure sensor is before (upstream of) the main valve in this system.
constexpr bool PRESSURE_IS_UPSTREAM_OF_VALVE = true;
constexpr float MAX_PRESSURE_BAR = 2.0f;

// With upstream overpressure, closing farther can dead-head the pump and raise
// pressure. This protection rejects only commands that reduce the opening.
constexpr bool BLOCK_CLOSING_DURING_UPSTREAM_OVERPRESSURE = true;

// Enable only if a failed pressure sensor must lock out all remote movement.
constexpr bool REQUIRE_VALID_PRESSURE_FOR_REMOTE_MOVE = false;

// ----------------------------- Pins -------------------------------
// Auto-direction RS485 module: no DE/RE direction pin is used.
constexpr int RS485_RX_PIN = 16;  // ESP32 RX <- RS485 module TX/RO
constexpr int RS485_TX_PIN = 17;  // ESP32 TX -> RS485 module RX/DI

constexpr int PRESSURE_SDA_PIN = 21;
constexpr int PRESSURE_SCL_PIN = 22;

constexpr int LORA_NSS_PIN = 5;
constexpr int LORA_DIO1_PIN = 26;
constexpr int LORA_RESET_PIN = 14;
constexpr int LORA_BUSY_PIN = 25;
constexpr int LORA_SCK_PIN = 18;
constexpr int LORA_MISO_PIN = 19;
constexpr int LORA_MOSI_PIN = 23;
constexpr int LORA_TXEN_PIN = 32;
constexpr int LORA_RXEN_PIN = 33;

// ----------------------- Actuator protocol ------------------------
constexpr uint32_t MODBUS_BAUD = 9600;
constexpr uint32_t MODBUS_FIRST_BYTE_TIMEOUT_MS = 250;
constexpr uint32_t MODBUS_INTERBYTE_TIMEOUT_MS = 8;
constexpr uint32_t MODBUS_MIN_GAP_MS = 30;

constexpr uint8_t MODBUS_READ_HOLDING = 0x03;
constexpr uint8_t MODBUS_WRITE_SINGLE = 0x06;

constexpr uint16_t REG_CONTROL_MODE = 0x0000;
constexpr uint16_t REG_ACTUAL_POSITION = 0x0001;
constexpr uint16_t REG_TARGET_POSITION = 0x0002;
constexpr uint16_t REG_FAULT_CODE = 0x0003;
constexpr uint16_t REG_RESET_CALIBRATION = 0x0004;

constexpr uint16_t POSITION_OFFSET = 1999;
constexpr uint16_t POSITION_MIN_RAW = 1999;  // 0.0%
constexpr uint16_t POSITION_MAX_RAW = 2999;  // 100.0%

// ----------------------- Pressure protocol ------------------------
// Manual gives 8-bit addresses FE(write)/FF(read); Wire uses 7-bit 0x7F.
constexpr uint8_t PRESSURE_I2C_ADDRESS = 0x7F;
constexpr uint8_t PRESSURE_DATA_REGISTER = 0x06;
constexpr uint8_t PRESSURE_CONTROL_REGISTER = 0x30;
constexpr uint8_t PRESSURE_START_COMMAND = 0x0A;
constexpr uint8_t PRESSURE_BUSY_MASK = 0x08;
constexpr float PRESSURE_FULL_SCALE_KPA = 1000.0f;  // 0-1 MPa sensor

// ---------------------------- LoRaWAN -----------------------------
constexpr uint8_t COMMAND_FPORT = 30;
constexpr uint8_t STATUS_FPORT = 31;
constexpr uint32_t STATUS_INTERVAL_MS = 5UL * 60UL * 1000UL;
constexpr uint32_t SENSOR_INTERVAL_MS = 1000;
constexpr uint32_t ACTUATOR_STATUS_INTERVAL_MS = 2000;
// Commissioning value: verify against measured full-travel time before field use.
constexpr uint32_t MOVEMENT_TIMEOUT_MS = 180000;
constexpr float MOVEMENT_START_DELTA_DEGREES = 0.5f;
constexpr float TARGET_TOLERANCE_DEGREES = 1.0f;
constexpr uint8_t TARGET_STABLE_POLLS = 2;
constexpr uint32_t CLASS_C_ACTIVATION_RETRY_MS = 3000;

constexpr char NVS_NAMESPACE[] = "main_valve";
constexpr char NVS_NONCES_KEY[] = "nonces";
constexpr uint32_t RTC_SESSION_MAGIC_VALUE = 0x4D564331UL;  // MVC1

HardwareSerial rs485(1);
Preferences preferences;
SPISettings loraSpiSettings(500000, MSBFIRST, SPI_MODE0);
SX1262 radio = new Module(LORA_NSS_PIN, LORA_DIO1_PIN,
                          LORA_RESET_PIN, LORA_BUSY_PIN,
                          SPI, loraSpiSettings);
LoRaWANNode lorawan(&radio, &LORAWAN_REGION, LORAWAN_SUB_BAND);

RTC_DATA_ATTR uint32_t rtcSessionMagic = 0;
RTC_DATA_ATTR uint8_t
  rtcLoRaWANSession[RADIOLIB_LORAWAN_SESSION_BUF_SIZE] = {0};
RTC_DATA_ATTR uint16_t rtcLastCommandId = 0xFFFF;
RTC_DATA_ATTR uint16_t rtcLastTargetAngle10 = 0xFFFF;

enum StatusReason : uint8_t {
  REASON_STARTUP = 0,
  REASON_LOCAL_COMMAND = 1,
  REASON_REMOTE_COMMAND = 2,
  REASON_DUPLICATE_COMMAND = 3,
  REASON_INVALID_COMMAND = 4,
  REASON_MODBUS_ERROR = 5,
  REASON_PRESSURE_INTERLOCK = 6,
  REASON_PRESSURE_SENSOR_ERROR = 7,
  REASON_ACTUATOR_BUSY = 8,
  REASON_MOVEMENT_TIMEOUT = 9,
  REASON_ACTUATOR_FAULT = 10,
  REASON_LOCAL_OVERRIDE = 11
};

enum CommandPhase : uint8_t {
  PHASE_NONE = 0,
  PHASE_ACCEPTED = 1,
  PHASE_MOVING = 2,
  PHASE_FINISHED = 3,
  PHASE_REJECTED = 4,
  PHASE_FAILED = 5
};

struct StatusEvent {
  uint16_t commandId;
  CommandPhase phase;
  uint8_t reason;
};

struct MovementTracking {
  bool active;
  bool movingSeen;
  uint16_t commandId;
  float startDegrees;
  float targetDegrees;
  uint32_t acceptedAtMs;
  uint8_t stablePolls;
};

struct ActuatorStatus {
  bool communicationValid;
  uint16_t mode;
  uint16_t positionRaw;
  uint16_t targetRaw;
  uint16_t faultCode;
  float actualPercent;
  float actualDegrees;
  float targetPercent;
  float targetDegrees;
};

struct PressureStatus {
  bool valid;
  float bar;
  float temperatureC;
  int32_t rawPressure;
};

ActuatorStatus actuator = {};
PressureStatus pressure = {};
uint8_t statusReason = REASON_STARTUP;
bool pressureAlarm = false;
bool lorawanActive = false;
bool classCActive = false;
bool statusPending = false;
uint32_t lastModbusTransactionMs = 0;
uint32_t lastPressureMs = 0;
uint32_t lastActuatorStatusMs = 0;
uint32_t lastStatusUplinkMs = 0;
MovementTracking movement = {};
constexpr uint8_t STATUS_EVENT_CAPACITY = 16;
StatusEvent statusEvents[STATUS_EVENT_CAPACITY] = {};
uint8_t statusEventHead = 0;
uint8_t statusEventCount = 0;
uint16_t lastReportedCommandId = 0xFFFF;
CommandPhase lastCommandPhase = PHASE_NONE;

void queueStatusEvent(uint16_t commandId, CommandPhase phase, uint8_t reason) {
  lastReportedCommandId = commandId;
  lastCommandPhase = phase;
  if (statusEventCount == STATUS_EVENT_CAPACITY) {
    // Preserve the newest result if the network has been unavailable.
    statusEventHead = (statusEventHead + 1) % STATUS_EVENT_CAPACITY;
    --statusEventCount;
    Serial.println("Status event queue full; oldest event dropped.");
  }
  const uint8_t tail = (statusEventHead + statusEventCount) % STATUS_EVENT_CAPACITY;
  statusEvents[tail] = {commandId, phase, reason};
  ++statusEventCount;
  statusPending = true;
}

uint16_t modbusCrc(const uint8_t *data, size_t length) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < length; ++i) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc & 1U) ? static_cast<uint16_t>((crc >> 1) ^ 0xA001U)
                       : static_cast<uint16_t>(crc >> 1);
    }
  }
  return crc;
}

void waitForModbusGap() {
  const uint32_t elapsed = millis() - lastModbusTransactionMs;
  if (elapsed < MODBUS_MIN_GAP_MS) {
    delay(MODBUS_MIN_GAP_MS - elapsed);
  }
}

void clearRs485Input() {
  while (rs485.available()) {
    (void)rs485.read();
  }
}

void sendModbusRequest(uint8_t function, uint16_t reg, uint16_t value) {
  waitForModbusGap();
  uint8_t frame[8] = {
    ACTUATOR_MODBUS_ADDRESS,
    function,
    static_cast<uint8_t>(reg >> 8),
    static_cast<uint8_t>(reg),
    static_cast<uint8_t>(value >> 8),
    static_cast<uint8_t>(value),
    0,
    0
  };
  const uint16_t crc = modbusCrc(frame, 6);
  frame[6] = static_cast<uint8_t>(crc);
  frame[7] = static_cast<uint8_t>(crc >> 8);

  clearRs485Input();
  rs485.write(frame, sizeof(frame));
  rs485.flush();
  lastModbusTransactionMs = millis();
}

bool readModbusFrame(uint8_t *frame, size_t capacity, size_t &length) {
  length = 0;
  const uint32_t started = millis();
  uint32_t lastByte = started;
  bool receivedAny = false;

  while (millis() - started < MODBUS_FIRST_BYTE_TIMEOUT_MS) {
    while (rs485.available()) {
      if (length >= capacity) {
        Serial.println("Modbus response exceeded buffer.");
        clearRs485Input();
        return false;
      }
      frame[length++] = static_cast<uint8_t>(rs485.read());
      lastByte = millis();
      receivedAny = true;
    }
    if (receivedAny && millis() - lastByte >= MODBUS_INTERBYTE_TIMEOUT_MS) {
      break;
    }
    delay(1);
  }

  lastModbusTransactionMs = millis();
  if (length < 5) {
    Serial.println("Modbus timeout or short response.");
    return false;
  }

  const uint16_t receivedCrc =
    static_cast<uint16_t>(frame[length - 2]) |
    (static_cast<uint16_t>(frame[length - 1]) << 8);
  if (receivedCrc != modbusCrc(frame, length - 2)) {
    Serial.println("Modbus CRC mismatch.");
    return false;
  }
  if (frame[0] != ACTUATOR_MODBUS_ADDRESS) {
    Serial.println("Modbus response came from a different address.");
    return false;
  }
  if ((frame[1] & 0x80U) != 0) {
    Serial.printf("Actuator Modbus exception: function=0x%02X code=0x%02X\n",
                  frame[1], frame[2]);
    return false;
  }
  return true;
}

bool readHoldingRegisters(uint16_t firstReg, uint16_t count,
                          uint16_t *values) {
  if (count == 0 || count > 8) {
    return false;
  }
  sendModbusRequest(MODBUS_READ_HOLDING, firstReg, count);
  uint8_t response[32] = {};
  size_t length = 0;
  if (!readModbusFrame(response, sizeof(response), length)) {
    return false;
  }

  const size_t expectedLength = 5U + 2U * count;
  if (length != expectedLength || response[1] != MODBUS_READ_HOLDING ||
      response[2] != 2U * count) {
    Serial.println("Malformed Modbus read response.");
    return false;
  }
  for (uint16_t i = 0; i < count; ++i) {
    values[i] =
      (static_cast<uint16_t>(response[3 + 2 * i]) << 8) |
      static_cast<uint16_t>(response[4 + 2 * i]);
  }
  return true;
}

bool writeSingleRegister(uint16_t reg, uint16_t value) {
  sendModbusRequest(MODBUS_WRITE_SINGLE, reg, value);
  uint8_t response[16] = {};
  size_t length = 0;
  if (!readModbusFrame(response, sizeof(response), length)) {
    return false;
  }
  if (length != 8 || response[1] != MODBUS_WRITE_SINGLE ||
      response[2] != static_cast<uint8_t>(reg >> 8) ||
      response[3] != static_cast<uint8_t>(reg) ||
      response[4] != static_cast<uint8_t>(value >> 8) ||
      response[5] != static_cast<uint8_t>(value)) {
    Serial.println("Malformed Modbus write echo.");
    return false;
  }
  return true;
}

float rawPositionToPercent(uint16_t raw) {
  if (raw < POSITION_MIN_RAW || raw > POSITION_MAX_RAW) {
    return NAN;
  }
  return static_cast<float>(raw - POSITION_OFFSET) / 10.0f;
}

float percentToDegrees(float percent) {
  return percent * VALVE_TRAVEL_DEGREES / 100.0f;
}

uint16_t degreesToRaw(float degrees) {
  const float percent = degrees * 100.0f / VALVE_TRAVEL_DEGREES;
  const int32_t raw = static_cast<int32_t>(lroundf(percent * 10.0f)) +
                      POSITION_OFFSET;
  return static_cast<uint16_t>(constrain(raw,
                                         static_cast<int32_t>(POSITION_MIN_RAW),
                                         static_cast<int32_t>(POSITION_MAX_RAW)));
}

bool readActuatorStatus() {
  uint16_t values[4] = {};
  if (!readHoldingRegisters(REG_CONTROL_MODE, 4, values)) {
    actuator.communicationValid = false;
    statusReason = REASON_MODBUS_ERROR;
    return false;
  }

  actuator.mode = values[0];
  actuator.positionRaw = values[1];
  actuator.targetRaw = values[2];
  actuator.faultCode = values[3];
  actuator.actualPercent = rawPositionToPercent(actuator.positionRaw);
  actuator.targetPercent = rawPositionToPercent(actuator.targetRaw);
  actuator.actualDegrees = percentToDegrees(actuator.actualPercent);
  actuator.targetDegrees = percentToDegrees(actuator.targetPercent);
  actuator.communicationValid = isfinite(actuator.actualPercent) && isfinite(actuator.targetPercent);
  return actuator.communicationValid;
}

bool ensureBusMode() {
  uint16_t mode = 0xFFFF;
  if (!readHoldingRegisters(REG_CONTROL_MODE, 1, &mode)) {
    return false;
  }
  if (mode == 1) {
    actuator.mode = 1;
    return true;
  }
  if (!writeSingleRegister(REG_CONTROL_MODE, 0x0001)) {
    return false;
  }
  actuator.mode = 1;
  Serial.println("Actuator changed to RS485 bus mode.");
  return true;
}

bool pressureWriteRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(PRESSURE_I2C_ADDRESS);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission(true) == 0;
}

bool pressureReadRegisters(uint8_t firstReg, uint8_t *data, size_t length) {
  Wire.beginTransmission(PRESSURE_I2C_ADDRESS);
  Wire.write(firstReg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }
  const size_t received = Wire.requestFrom(
    static_cast<uint8_t>(PRESSURE_I2C_ADDRESS), length, true);
  if (received != length) {
    while (Wire.available()) (void)Wire.read();
    return false;
  }
  for (size_t i = 0; i < length; ++i) {
    data[i] = static_cast<uint8_t>(Wire.read());
  }
  return true;
}

int32_t decodeSigned24(uint8_t msb, uint8_t mid, uint8_t lsb) {
  uint32_t value = (static_cast<uint32_t>(msb) << 16) |
                   (static_cast<uint32_t>(mid) << 8) | lsb;
  if ((value & 0x00800000UL) != 0) value |= 0xFF000000UL;
  return static_cast<int32_t>(value);
}

bool readPressure() {
  pressure.valid = false;
  pressureAlarm = false;
  if (!pressureWriteRegister(PRESSURE_CONTROL_REGISTER,
                             PRESSURE_START_COMMAND)) {
    statusReason = REASON_PRESSURE_SENSOR_ERROR;
    return false;
  }

  const uint32_t started = millis();
  bool conversionComplete = false;
  while (millis() - started < 200) {
    uint8_t control = 0;
    if (!pressureReadRegisters(PRESSURE_CONTROL_REGISTER, &control, 1)) {
      statusReason = REASON_PRESSURE_SENSOR_ERROR;
      return false;
    }
    if ((control & PRESSURE_BUSY_MASK) == 0) {
      conversionComplete = true;
      break;
    }
    delay(2);
  }
  if (!conversionComplete) {
    Serial.println("Pressure conversion timed out.");
    statusReason = REASON_PRESSURE_SENSOR_ERROR;
    return false;
  }

  uint8_t data[5] = {};
  if (!pressureReadRegisters(PRESSURE_DATA_REGISTER, data, sizeof(data))) {
    statusReason = REASON_PRESSURE_SENSOR_ERROR;
    return false;
  }
  pressure.rawPressure = decodeSigned24(data[0], data[1], data[2]);
  const int16_t rawTemperature = static_cast<int16_t>(
    (static_cast<uint16_t>(data[3]) << 8) | data[4]);
  const float kPa =
    static_cast<float>(pressure.rawPressure) / 8388608.0f *
    PRESSURE_FULL_SCALE_KPA;
  pressure.bar = kPa / 100.0f;
  pressure.temperatureC = static_cast<float>(rawTemperature) / 256.0f;
  pressure.valid = isfinite(pressure.bar) &&
                   pressure.bar >= -0.2f && pressure.bar <= 10.5f;
  pressureAlarm = pressure.valid && pressure.bar > MAX_PRESSURE_BAR;
  return pressure.valid;
}

bool movementAllowed(float requestedDegrees, bool remoteCommand) {
  if (remoteCommand && REQUIRE_VALID_PRESSURE_FOR_REMOTE_MOVE &&
      !pressure.valid) {
    Serial.println("Remote movement rejected: pressure is unavailable.");
    statusReason = REASON_PRESSURE_SENSOR_ERROR;
    return false;
  }
  if (PRESSURE_IS_UPSTREAM_OF_VALVE &&
      BLOCK_CLOSING_DURING_UPSTREAM_OVERPRESSURE && pressureAlarm &&
      actuator.communicationValid &&
      requestedDegrees < actuator.actualDegrees) {
    Serial.println("Movement rejected: do not close farther during upstream overpressure.");
    statusReason = REASON_PRESSURE_INTERLOCK;
    return false;
  }
  return true;
}

bool setValveAngle(float degrees, bool remoteCommand) {
  if (!isfinite(degrees) || degrees < 0.0f ||
      degrees > VALVE_TRAVEL_DEGREES) {
    Serial.printf("Angle must be 0.0-%.1f degrees.\n", VALVE_TRAVEL_DEGREES);
    statusReason = REASON_INVALID_COMMAND;
    return false;
  }
  readPressure();
  readActuatorStatus();
  if (!movementAllowed(degrees, remoteCommand)) {
    return false;
  }
  if (!ensureBusMode()) {
    Serial.println("Cannot select RS485 bus mode.");
    statusReason = REASON_MODBUS_ERROR;
    return false;
  }

  const uint16_t raw = degreesToRaw(degrees);
  if (!writeSingleRegister(REG_TARGET_POSITION, raw)) {
    statusReason = REASON_MODBUS_ERROR;
    return false;
  }
  const float commandedPercent = rawPositionToPercent(raw);
  Serial.printf("Target set: %.1f degrees (%.1f%%, raw=%u).\n",
                percentToDegrees(commandedPercent), commandedPercent, raw);
  if (!remoteCommand && movement.active) {
    const uint16_t interruptedId = movement.commandId;
    movement.active = false;
    queueStatusEvent(interruptedId, PHASE_FAILED, REASON_LOCAL_OVERRIDE);
  }
  statusReason = remoteCommand ? REASON_REMOTE_COMMAND : REASON_LOCAL_COMMAND;
  statusPending = true;
  return true;
}

void trackMovement() {
  if (!movement.active) return;
  const uint32_t now = millis();
  if (actuator.communicationValid) {
    if (actuator.faultCode != 0) {
      movement.active = false;
      statusReason = REASON_ACTUATOR_FAULT;
      queueStatusEvent(movement.commandId, PHASE_FAILED, statusReason);
      return;
    }
    if (!movement.movingSeen &&
        fabsf(actuator.actualDegrees - movement.startDegrees) >=
          MOVEMENT_START_DELTA_DEGREES) {
      movement.movingSeen = true;
      queueStatusEvent(movement.commandId, PHASE_MOVING, REASON_REMOTE_COMMAND);
    }
    if (fabsf(actuator.actualDegrees - movement.targetDegrees) <=
        TARGET_TOLERANCE_DEGREES) {
      if (movement.stablePolls < TARGET_STABLE_POLLS) ++movement.stablePolls;
      if (movement.stablePolls >= TARGET_STABLE_POLLS) {
        movement.active = false;
        queueStatusEvent(movement.commandId, PHASE_FINISHED, REASON_REMOTE_COMMAND);
        return;
      }
    } else {
      movement.stablePolls = 0;
    }
  }
  if (now - movement.acceptedAtMs >= MOVEMENT_TIMEOUT_MS) {
    movement.active = false;
    statusReason = REASON_MOVEMENT_TIMEOUT;
    queueStatusEvent(movement.commandId, PHASE_FAILED, statusReason);
  }
}

uint16_t scaledOrFFFF(float value, float multiplier) {
  if (!isfinite(value) || value < 0.0f) return 0xFFFF;
  const float scaled = value * multiplier;
  return static_cast<uint16_t>(constrain(
    static_cast<int32_t>(lroundf(scaled)), 0L, 65534L));
}

void buildStatusPayload(uint8_t payload[18], const StatusEvent *event) {
  const uint16_t actualAngle10 = actuator.communicationValid
    ? scaledOrFFFF(actuator.actualDegrees, 10.0f) : 0xFFFF;
  const uint16_t targetAngle10 = actuator.communicationValid
    ? scaledOrFFFF(actuator.targetDegrees, 10.0f) : 0xFFFF;
  const uint16_t pressure100 = pressure.valid
    ? scaledOrFFFF(pressure.bar, 100.0f) : 0xFFFF;

  uint8_t flags = 0;
  if (actuator.communicationValid) flags |= 0x01;
  if (pressure.valid) flags |= 0x02;
  if (actuator.mode == 1) flags |= 0x04;
  if (actuator.communicationValid &&
      fabsf(actuator.actualDegrees - actuator.targetDegrees) > 1.0f) flags |= 0x08;
  if (pressureAlarm) flags |= 0x10;
  if (lorawanActive) flags |= 0x20;
  if (classCActive) flags |= 0x40;

  payload[0] = 2;
  payload[1] = flags;
  payload[2] = event ? event->reason : statusReason;
  payload[3] = static_cast<uint8_t>(actualAngle10 >> 8);
  payload[4] = static_cast<uint8_t>(actualAngle10);
  payload[5] = static_cast<uint8_t>(targetAngle10 >> 8);
  payload[6] = static_cast<uint8_t>(targetAngle10);
  payload[7] = static_cast<uint8_t>(pressure100 >> 8);
  payload[8] = static_cast<uint8_t>(pressure100);
  payload[9] = static_cast<uint8_t>(actuator.faultCode >> 8);
  payload[10] = static_cast<uint8_t>(actuator.faultCode);
  payload[11] = static_cast<uint8_t>(rtcLastCommandId >> 8);
  payload[12] = static_cast<uint8_t>(rtcLastCommandId);
  payload[13] = actuator.communicationValid
    ? static_cast<uint8_t>(actuator.mode) : 0xFF;
  payload[14] = static_cast<uint8_t>(
    constrain(lroundf(pressure.temperatureC + 40.0f), 0L, 255L));
  const uint16_t reportedId = event ? event->commandId : lastReportedCommandId;
  payload[15] = static_cast<uint8_t>(reportedId >> 8);
  payload[16] = static_cast<uint8_t>(reportedId);
  payload[17] = event ? event->phase : lastCommandPhase;
}

void saveSessionToRtc() {
  memcpy(rtcLoRaWANSession, lorawan.getBufferSession(),
         RADIOLIB_LORAWAN_SESSION_BUF_SIZE);
  rtcSessionMagic = RTC_SESSION_MAGIC_VALUE;
}

bool loadNoncesFromNvs() {
  if (!preferences.begin(NVS_NAMESPACE, true)) return false;
  const size_t length = preferences.getBytesLength(NVS_NONCES_KEY);
  if (length != RADIOLIB_LORAWAN_NONCES_BUF_SIZE) {
    preferences.end();
    return false;
  }
  uint8_t nonces[RADIOLIB_LORAWAN_NONCES_BUF_SIZE] = {};
  const size_t read = preferences.getBytes(NVS_NONCES_KEY, nonces,
                                           sizeof(nonces));
  preferences.end();
  return read == sizeof(nonces) &&
         lorawan.setBufferNonces(nonces) == RADIOLIB_ERR_NONE;
}

bool saveNoncesToNvs() {
  if (!preferences.begin(NVS_NAMESPACE, false)) return false;
  const size_t written = preferences.putBytes(
    NVS_NONCES_KEY, lorawan.getBufferNonces(),
    RADIOLIB_LORAWAN_NONCES_BUF_SIZE);
  preferences.end();
  return written == RADIOLIB_LORAWAN_NONCES_BUF_SIZE;
}

bool processRemoteCommand(const uint8_t *data, size_t length, uint8_t fPort) {
  if (fPort != COMMAND_FPORT || length != 6 || data[0] != 1 || data[1] != 1) {
    Serial.printf("Invalid downlink: FPort=%u length=%u.\n",
                  fPort, static_cast<unsigned>(length));
    statusReason = REASON_INVALID_COMMAND;
    queueStatusEvent(0xFFFF, PHASE_REJECTED, statusReason);
    return false;
  }
  const uint16_t angle10 =
    (static_cast<uint16_t>(data[2]) << 8) | data[3];
  const uint16_t commandId =
    (static_cast<uint16_t>(data[4]) << 8) | data[5];
  if (angle10 > static_cast<uint16_t>(VALVE_TRAVEL_DEGREES * 10.0f)) {
    statusReason = REASON_INVALID_COMMAND;
    queueStatusEvent(commandId, PHASE_REJECTED, statusReason);
    return false;
  }
  if (commandId == rtcLastCommandId) {
    if (angle10 == rtcLastTargetAngle10) {
      Serial.printf("Duplicate remote command %u ignored safely.\n", commandId);
      statusReason = REASON_DUPLICATE_COMMAND;
      queueStatusEvent(commandId, PHASE_ACCEPTED, statusReason);
      return true;
    }
    Serial.printf("Rejected reused command ID %u with a different angle.\n",
                  commandId);
    statusReason = REASON_INVALID_COMMAND;
    queueStatusEvent(commandId, PHASE_REJECTED, statusReason);
    return false;
  }

  if (movement.active) {
    statusReason = REASON_ACTUATOR_BUSY;
    queueStatusEvent(commandId, PHASE_REJECTED, statusReason);
    return false;
  }

  const bool applied = setValveAngle(angle10 / 10.0f, true);
  if (applied) {
    rtcLastCommandId = commandId;
    rtcLastTargetAngle10 = angle10;
    movement = {true, false, commandId,
                actuator.communicationValid ? actuator.actualDegrees : NAN,
                angle10 / 10.0f, millis(), 0};
    queueStatusEvent(commandId, PHASE_ACCEPTED, REASON_REMOTE_COMMAND);
  } else {
    queueStatusEvent(commandId, PHASE_REJECTED, statusReason);
  }
  return applied;
}

bool sendStatusUplink(bool confirmed) {
  if (!lorawanActive) return false;
  readPressure();
  readActuatorStatus();
  const StatusEvent *event = statusEventCount ? &statusEvents[statusEventHead] : nullptr;
  uint8_t uplink[18] = {};
  buildStatusPayload(uplink, event);
  uint8_t downlink[RADIOLIB_LORAWAN_MAX_PAYLOAD_SIZE] = {};
  size_t downlinkLength = 0;
  LoRaWANEvent_t downlinkEvent = {};

  const int16_t state = lorawan.sendReceive(
    uplink, sizeof(uplink), STATUS_FPORT,
    downlink, &downlinkLength, confirmed,
    nullptr, &downlinkEvent);
  saveSessionToRtc();
  lastStatusUplinkMs = millis();

  if (state < RADIOLIB_ERR_NONE) {
    Serial.printf("LoRaWAN status uplink failed: %d\n", state);
    return false;
  }
  if (event) {
    statusEventHead = (statusEventHead + 1) % STATUS_EVENT_CAPACITY;
    --statusEventCount;
  }
  statusPending = statusEventCount > 0;
  Serial.println("LoRaWAN status uplink sent.");
  if (state > RADIOLIB_ERR_NONE && downlinkLength > 0) {
    processRemoteCommand(downlink, downlinkLength, downlinkEvent.fPort);
  }
  return state > RADIOLIB_ERR_NONE || !confirmed;
}

bool setupLoRaWAN() {
  SPI.begin(LORA_SCK_PIN, LORA_MISO_PIN, LORA_MOSI_PIN, LORA_NSS_PIN);
  delay(100);
  int16_t state = radio.begin(
    868.0, 125.0, 9, 7, RADIOLIB_SX126X_SYNC_WORD_PRIVATE,
    10, 8, 0.0, false);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("SX1262 initialization failed: %d\n", state);
    return false;
  }
  radio.setRfSwitchPins(LORA_RXEN_PIN, LORA_TXEN_PIN);

  if (!LORAWAN_CREDENTIALS_CONFIGURED) {
    Serial.println("LoRaWAN credentials are placeholders; local control remains available.");
    return false;
  }
  state = lorawan.beginOTAA(LORAWAN_JOIN_EUI, LORAWAN_DEV_EUI,
                            nullptr, LORAWAN_APP_KEY);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("beginOTAA failed: %d\n", state);
    return false;
  }
  lorawan.setADR(true);
  const bool noncesRestored = loadNoncesFromNvs();
  if (noncesRestored && rtcSessionMagic == RTC_SESSION_MAGIC_VALUE) {
    (void)lorawan.setBufferSession(rtcLoRaWANSession);
  }

  state = lorawan.activateOTAA();
  if (state != RADIOLIB_LORAWAN_NEW_SESSION &&
      state != RADIOLIB_LORAWAN_SESSION_RESTORED) {
    (void)saveNoncesToNvs();
    Serial.printf("OTAA activation failed: %d\n", state);
    rtcSessionMagic = 0;
    return false;
  }
  if (!saveNoncesToNvs()) {
    Serial.println("Warning: OTAA nonces could not be saved to NVS.");
  }
  saveSessionToRtc();
  lorawanActive = true;
  Serial.println(state == RADIOLIB_LORAWAN_NEW_SESSION
    ? "New LoRaWAN session established."
    : "LoRaWAN session restored.");

  state = lorawan.setClass(RADIOLIB_LORAWAN_CLASS_C);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("Could not request Class C: %d\n", state);
    return true;  // Class-A fallback.
  }

  // RadioLib requires a confirmed uplink and its ACK to complete Class C.
  for (uint8_t attempt = 1; attempt <= 3; ++attempt) {
    Serial.printf("Class-C activation uplink, attempt %u...\n", attempt);
    if (sendStatusUplink(true)) {
      classCActive = true;
      Serial.println("Class C active: continuous downlink reception enabled.");
      return true;
    }
    delay(CLASS_C_ACTIVATION_RETRY_MS);
  }
  Serial.println("Class-C activation was not acknowledged; using Class-A fallback.");
  return true;
}

void printStatus() {
  readPressure();
  readActuatorStatus();
  if (actuator.communicationValid) {
    Serial.printf("Actuator: actual %.1f deg (%.1f%%), target %.1f deg (%.1f%%), "
                  "mode=%s, fault=0x%04X\n",
                  actuator.actualDegrees, actuator.actualPercent,
                  actuator.targetDegrees, actuator.targetPercent,
                  actuator.mode == 1 ? "RS485 bus" : "analog",
                  actuator.faultCode);
  } else {
    Serial.println("Actuator: communication unavailable.");
  }

  if (pressure.valid) {
    Serial.printf("Pressure: %.3f bar, temperature %.2f C%s\n",
                  pressure.bar, pressure.temperatureC,
                  pressureAlarm ? " - OVER LIMIT" : "");
  } else {
    Serial.println("Pressure: unavailable.");
  }
  Serial.printf("LoRaWAN: %s, class: %s\n",
                lorawanActive ? "connected" : "unavailable",
                classCActive ? "C" : "A/local only");
}

void printHelp() {
  Serial.println("Commands:");
  Serial.println("  status          read actuator and pressure");
  Serial.println("  angle 30        set 30 degrees (valid 0-90)");
  Serial.println("  percent 50      set 50% travel (=45 degrees)");
  Serial.println("  close / open    set 0 / 90 degrees");
  Serial.println("  bus             select RS485 bus mode");
  Serial.println("  clear           clear actuator faults");
  Serial.println("  calibrate CONFIRM  start 30-120 second calibration");
  Serial.println("  reset CONFIRM      reset the positioner");
}

bool parseFloatStrict(const String &text, float &value) {
  String copy = text;
  copy.trim();
  if (copy.length() == 0 || copy.length() >= 24) return false;
  char buffer[24] = {};
  copy.toCharArray(buffer, sizeof(buffer));
  char *end = nullptr;
  value = strtof(buffer, &end);
  while (end != nullptr && *end == ' ') ++end;
  return end != buffer && end != nullptr && *end == '\0' && isfinite(value);
}

void processSerialCommand(String command) {
  command.trim();
  command.toLowerCase();
  if (command == "status") {
    printStatus();
  } else if (command == "open") {
    setValveAngle(VALVE_TRAVEL_DEGREES, false);
  } else if (command == "close") {
    setValveAngle(0.0f, false);
  } else if (command.startsWith("angle ")) {
    float degrees = NAN;
    if (!parseFloatStrict(command.substring(6), degrees)) {
      Serial.println("Invalid angle; no movement performed.");
    } else {
      setValveAngle(degrees, false);
    }
  } else if (command.startsWith("percent ")) {
    float percent = NAN;
    if (!parseFloatStrict(command.substring(8), percent)) {
      Serial.println("Invalid percent; no movement performed.");
    } else if (percent < 0.0f || percent > 100.0f) {
      Serial.println("Percent must be 0-100.");
    } else {
      setValveAngle(percentToDegrees(percent), false);
    }
  } else if (command == "bus") {
    Serial.println(ensureBusMode() ? "RS485 bus mode ready." : "Bus mode failed.");
  } else if (command == "clear") {
    Serial.println(writeSingleRegister(REG_FAULT_CODE, 0)
      ? "Actuator faults cleared." : "Could not clear faults.");
  } else if (command == "calibrate confirm") {
    Serial.println(writeSingleRegister(REG_RESET_CALIBRATION, 0x000A)
      ? "Calibration started; allow 30-120 seconds." : "Calibration command failed.");
  } else if (command == "reset confirm") {
    Serial.println(writeSingleRegister(REG_RESET_CALIBRATION, 0x0F30)
      ? "Positioner reset command accepted." : "Reset command failed.");
  } else if (command == "help") {
    printHelp();
  } else if (command.length() > 0) {
    Serial.println("Unknown command. Type help.");
  }
}

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(100);
  delay(1200);
  Serial.println("\nESP32 main butterfly-valve controller");
  Serial.println("FOSD-05E Modbus RTU: 9600 baud, 8N1, address 1");

  rs485.begin(MODBUS_BAUD, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
  Wire.begin(PRESSURE_SDA_PIN, PRESSURE_SCL_PIN, 100000);
  delay(500);

  printHelp();
  printStatus();
  if (!ensureBusMode()) {
    Serial.println("WARNING: actuator is not responding or bus mode could not be selected.");
  }
  setupLoRaWAN();
  statusPending = lorawanActive;
}

void loop() {
  const uint32_t now = millis();

  if (Serial.available()) {
    processSerialCommand(Serial.readStringUntil('\n'));
  }

  if (now - lastPressureMs >= SENSOR_INTERVAL_MS) {
    lastPressureMs = now;
    const bool wasAlarm = pressureAlarm;
    readPressure();
    if (pressureAlarm != wasAlarm) statusPending = true;
  }

  if (now - lastActuatorStatusMs >= ACTUATOR_STATUS_INTERVAL_MS) {
    lastActuatorStatusMs = now;
    readActuatorStatus();
    trackMovement();
  }

  if (classCActive) {
    uint8_t downlink[RADIOLIB_LORAWAN_MAX_PAYLOAD_SIZE] = {};
    size_t downlinkLength = 0;
    LoRaWANEvent_t event = {};
    const int16_t state = lorawan.getDownlinkClassC(
      downlink, &downlinkLength, &event);
    if (state > 0 && downlinkLength > 0) {
      Serial.println("Class-C downlink received.");
      processRemoteCommand(downlink, downlinkLength, event.fPort);
      saveSessionToRtc();
    } else if (state < RADIOLIB_ERR_NONE) {
      Serial.printf("Class-C receive error: %d\n", state);
    }
  }

  if (lorawanActive &&
      ((statusPending && now - lastStatusUplinkMs >= 5000) ||
       now - lastStatusUplinkMs >= STATUS_INTERVAL_MS)) {
    sendStatusUplink(false);
  }
  delay(5);
}
