#include <Arduino.h>
#include <cmath>
#include <CommandProcessor.h>
#include <ProjectConfig.h>
#include <Preferences.h>
#include <RadioLib.h>
#include <SPI.h>

#if __has_include("PumpControlLoRaSecrets.h")
#include "PumpControlLoRaSecrets.h"
#else
#include "PumpControlLoRaSecrets.example.h"
#endif

namespace {

HardwareSerial vfdSerial(2);
PrintController logger(Serial, true);
RS485Bus rs485;
DelixiCDIE100 vfd(rs485, logger, irrigation::ActiveInverter);
PumpController pump(vfd, logger, irrigation::ActiveMotor);
CommandProcessor commands(pump, vfd, logger);
SerialCommandSource serialCommands(commands, logger);

namespace secrets = irrigation::pump_control::lorawan_secrets;
constexpr int LORA_NSS_PIN = 5;
constexpr int LORA_DIO1_PIN = 26;
constexpr int LORA_RESET_PIN = 14;
constexpr int LORA_BUSY_PIN = 25;
constexpr int LORA_SCK_PIN = 18;
constexpr int LORA_MISO_PIN = 19;
constexpr int LORA_MOSI_PIN = 23;
constexpr int LORA_TXEN_PIN = 32;
constexpr int LORA_RXEN_PIN = 33;
constexpr uint8_t COMMAND_FPORT = 50;
constexpr uint8_t STATUS_FPORT = 51;
constexpr uint32_t STATUS_INTERVAL_STOPPED_MS = 60 * 1000;
constexpr uint32_t STATUS_INTERVAL_RUNNING_MS = 15 * 1000;
constexpr uint32_t STATUS_RESPONSE_MIN_GAP_MS = 5 * 1000;
constexpr uint32_t JOIN_RETRY_INITIAL_MS = 60 * 1000;
constexpr uint32_t JOIN_RETRY_MAX_MS = 15 * 60 * 1000;
constexpr uint32_t REMOTE_COMPLETION_TIMEOUT_MS = 120 * 1000;
constexpr float RUN_FREQUENCY_TOLERANCE_HZ = 0.25f;
constexpr float STOP_FREQUENCY_TOLERANCE_HZ = 0.25f;

enum class PendingRemoteCompletion : uint8_t {
  None,
  StartAtFrequency,
  StoppedAtZero
};

SPISettings loraSpiSettings(500000, MSBFIRST, SPI_MODE0);
SX1262 radio = new Module(LORA_NSS_PIN, LORA_DIO1_PIN,
                          LORA_RESET_PIN, LORA_BUSY_PIN, SPI, loraSpiSettings);
LoRaWANNode lorawan(&radio, &EU868, 0);
Preferences preferences;
bool lorawanActive = false;
bool classCActive = false;
bool statusPending = false;
uint32_t lastStatusMs = 0;
uint32_t nextJoinAttemptMs = 0;
uint32_t lastStatusSignature = 0;
bool hasLastStatusSignature = false;
uint8_t joinAttemptCount = 0;
int16_t previousJoinError = RADIOLIB_ERR_NONE;
uint16_t lastJoinRetrySeconds = 0;
uint16_t lastCommandId = 0xFFFF;
uint8_t lastCommandOp = 0;
uint16_t lastCommandArg = 0;
uint8_t lastCommandResult = 0;
PendingRemoteCompletion pendingRemoteCompletion =
    PendingRemoteCompletion::None;
uint32_t remoteCompletionStartedMs = 0;

void put16(uint8_t *destination, uint16_t value) {
  destination[0] = static_cast<uint8_t>(value >> 8);
  destination[1] = static_cast<uint8_t>(value);
}

uint16_t get16(const uint8_t *source) {
  return (static_cast<uint16_t>(source[0]) << 8) | source[1];
}

const char *radioErrorMeaning(int16_t state) {
  switch (state) {
    case RADIOLIB_ERR_NONE: return "no error";
    case RADIOLIB_ERR_CHIP_NOT_FOUND: return "radio chip not found";
    case RADIOLIB_ERR_TX_TIMEOUT: return "radio transmit timeout";
    case RADIOLIB_ERR_RX_TIMEOUT: return "radio receive timeout";
    case RADIOLIB_ERR_INVALID_FREQUENCY: return "invalid radio frequency";
    case RADIOLIB_ERR_SPI_WRITE_FAILED: return "radio SPI write failed";
    case RADIOLIB_ERR_NETWORK_NOT_JOINED: return "LoRaWAN network not joined";
    case RADIOLIB_ERR_NO_JOIN_ACCEPT:
      return "no OTAA JoinAccept received in RX1/RX2";
    default: return "unclassified RadioLib error";
  }
}

uint32_t statusSignature() {
  const PumpStatus &s = pump.status();
  uint32_t signature = s.communicationOk ? 1U : 0U;
  signature |= s.configurationValid ? (1U << 1) : 0U;
  signature |= s.running ? (1U << 2) : 0U;
  signature |= s.frequencyArmed ? (1U << 3) : 0U;
  signature |= static_cast<uint32_t>(s.runState) << 4;
  signature |= static_cast<uint32_t>(s.lastCommunicationError) << 8;
  signature |= static_cast<uint32_t>(s.vfdFaultCode) << 16;
  return signature;
}

uint32_t currentStatusIntervalMs() {
  return pump.status().running ? STATUS_INTERVAL_RUNNING_MS
                               : STATUS_INTERVAL_STOPPED_MS;
}

void scheduleJoinRetry(int16_t state) {
  previousJoinError = state;
  const uint8_t exponent = joinAttemptCount > 4 ? 4 : joinAttemptCount - 1;
  uint32_t baseMs = JOIN_RETRY_INITIAL_MS << exponent;
  if (baseMs > JOIN_RETRY_MAX_MS) baseMs = JOIN_RETRY_MAX_MS;
  const uint32_t jitterMs = baseMs / 5;
  const uint32_t retryMs = baseMs - jitterMs +
      (esp_random() % (2 * jitterMs + 1));
  nextJoinAttemptMs = millis() + retryMs;
  lastJoinRetrySeconds = static_cast<uint16_t>((retryMs + 999) / 1000);
  Serial.printf(
      "Pump LoRaWAN join attempt %u failed: %s (%d); next attempt in %u s.\n",
      joinAttemptCount, radioErrorMeaning(state), state,
      lastJoinRetrySeconds);
}

bool loadNonces() {
  // Opening read-write creates the namespace on first boot without an
  // expected NVS_NOT_FOUND error from Preferences.begin(readOnly=true).
  if (!preferences.begin("pump-lora", false)) return false;
  uint8_t buffer[RADIOLIB_LORAWAN_NONCES_BUF_SIZE] = {};
  const bool valid = preferences.isKey("nonces") &&
      preferences.getBytesLength("nonces") == sizeof(buffer) &&
      preferences.getBytes("nonces", buffer, sizeof(buffer)) == sizeof(buffer);
  preferences.end();
  return valid && lorawan.setBufferNonces(buffer) == RADIOLIB_ERR_NONE;
}

bool saveNonces() {
  if (!preferences.begin("pump-lora", false)) return false;
  const bool saved = preferences.putBytes("nonces", lorawan.getBufferNonces(),
      RADIOLIB_LORAWAN_NONCES_BUF_SIZE) == RADIOLIB_LORAWAN_NONCES_BUF_SIZE;
  preferences.end();
  return saved;
}

void loadLastCommand() {
  if (!preferences.begin("pump-cmd", false)) return;
  uint8_t saved[6] = {};
  if (preferences.isKey("last") &&
      preferences.getBytesLength("last") == sizeof(saved) &&
      preferences.getBytes("last", saved, sizeof(saved)) == sizeof(saved)) {
    lastCommandId = get16(saved);
    lastCommandOp = saved[2];
    lastCommandArg = get16(saved + 3);
    lastCommandResult = saved[5];
  }
  preferences.end();
}

bool saveLastCommand() {
  uint8_t saved[6] = {};
  put16(saved, lastCommandId);
  saved[2] = lastCommandOp;
  put16(saved + 3, lastCommandArg);
  saved[5] = lastCommandResult;
  if (!preferences.begin("pump-cmd", false)) return false;
  const bool ok = preferences.putBytes("last", saved, sizeof(saved)) == sizeof(saved);
  preferences.end();
  return ok;
}

void beginRemoteCompletion(PendingRemoteCompletion completion) {
  pendingRemoteCompletion = completion;
  remoteCompletionStartedMs = millis();
  lastCommandResult = 6;  // Accepted by the VFD; final condition pending.
  (void)saveLastCommand();
}

void finishRemoteCompletion(bool completed, const char *message) {
  lastCommandResult = completed ? 1 : 2;
  pendingRemoteCompletion = PendingRemoteCompletion::None;
  (void)saveLastCommand();
  statusPending = true;
  Serial.println(message);
}

void updateRemoteCompletion() {
  if (pendingRemoteCompletion == PendingRemoteCompletion::None) return;
  const PumpStatus &s = pump.status();
  if (s.communicationOk) {
    if (pendingRemoteCompletion == PendingRemoteCompletion::StartAtFrequency &&
        s.runState == DelixiRunState::Forward &&
        std::fabs(s.actualFrequencyHz - s.commandedFrequencyHz) <=
            RUN_FREQUENCY_TOLERANCE_HZ) {
      finishRemoteCompletion(
          true, "Pump Start complete: requested frequency reached.");
      return;
    }
    if (pendingRemoteCompletion == PendingRemoteCompletion::StoppedAtZero &&
        s.runState == DelixiRunState::Stopped &&
        s.actualFrequencyHz <= STOP_FREQUENCY_TOLERANCE_HZ) {
      finishRemoteCompletion(
          true, "Pump Stop complete: VFD stopped at approximately 0 Hz.");
      return;
    }
  }
  if (millis() - remoteCompletionStartedMs >= REMOTE_COMPLETION_TIMEOUT_MS) {
    finishRemoteCompletion(
        false, "Pump command completion timed out before the final VFD condition.");
  }
}

void buildStatus(uint8_t (&payload)[22]) {
  const PumpStatus &s = pump.status();
  uint8_t flags = 0;
  if (s.communicationOk) flags |= 0x01;
  if (s.configurationValid) flags |= 0x02;
  if (s.running) flags |= 0x04;
  if (s.frequencyArmed) flags |= 0x08;
  if (lorawanActive) flags |= 0x10;
  if (classCActive) flags |= 0x20;
  payload[0] = 2;
  payload[1] = flags;
  payload[2] = lastCommandResult;
  put16(payload + 3, lastCommandId);
  put16(payload + 5, static_cast<uint16_t>(s.commandedFrequencyHz * 100.0f + 0.5f));
  put16(payload + 7, static_cast<uint16_t>(s.actualFrequencyHz * 100.0f + 0.5f));
  put16(payload + 9, static_cast<uint16_t>(s.motorCurrentA * 100.0f + 0.5f));
  put16(payload + 11, s.vfdFaultCode);
  put16(payload + 13, static_cast<uint16_t>(s.outputVoltageV * 10.0f + 0.5f));
  payload[15] = static_cast<uint8_t>(s.runState);
  payload[16] = static_cast<uint8_t>(s.lastCommunicationError);
  put16(payload + 17, static_cast<uint16_t>(previousJoinError));
  payload[19] = joinAttemptCount;
  put16(payload + 20, lastJoinRetrySeconds);
}

bool processRemoteCommand(const uint8_t *data, size_t length, uint8_t fPort) {
  if (fPort != COMMAND_FPORT || length < 2 || data[0] != 1) {
    lastCommandResult = 4;
    statusPending = true;
    return false;
  }
  const uint8_t op = data[1];
  if (op == 5 && length == 2) {
    Serial.println("Pump status refresh requested by dashboard.");
    statusPending = true;
    return true;
  }
  if (length != 6) {
    lastCommandResult = 4;
    statusPending = true;
    return false;
  }
  const uint16_t id = get16(data + 2);
  const uint16_t arg = get16(data + 4);
  if (id == 0xFFFF || op < 1 || op > 4 ||
      (op != 3 && arg != 0) ||
      (op == 3 && (arg < irrigation::ActiveMotor.minRunFrequencyHz * 100.0f ||
                   arg > irrigation::ActiveMotor.maxRunFrequencyHz * 100.0f))) {
    lastCommandResult = 4;
    statusPending = true;
    return false;
  }
  if (id == lastCommandId) {
    lastCommandResult = (op == lastCommandOp && arg == lastCommandArg) ? 3 : 4;
    statusPending = true;
    return lastCommandResult == 3;
  }
  if (lastCommandId != 0xFFFF &&
      static_cast<int16_t>(id - lastCommandId) <= 0) {
    lastCommandResult = 4;
    statusPending = true;
    return false;
  }
  const uint16_t previousId = lastCommandId;
  const uint8_t previousOp = lastCommandOp;
  const uint16_t previousArg = lastCommandArg;
  lastCommandId = id;
  lastCommandOp = op;
  lastCommandArg = arg;
  lastCommandResult = 2;  // Record a safe failed result before any VFD command.
  if (!saveLastCommand()) {
    lastCommandId = previousId;
    lastCommandOp = previousOp;
    lastCommandArg = previousArg;
    lastCommandResult = 5;
    statusPending = true;
    return false;
  }
  bool accepted = false;
  switch (op) {
    case 1: accepted = pump.stop(); break;
    case 2: accepted = pump.emergencyStop(); break;
    case 3: accepted = pump.setSpeedHz(arg / 100.0f); break;
    case 4: accepted = pump.start(); break;
  }
  lastCommandResult = accepted ? 1 : 2;
  if (accepted && op == 4) {
    beginRemoteCompletion(PendingRemoteCompletion::StartAtFrequency);
  } else if (accepted && op == 1) {
    beginRemoteCompletion(PendingRemoteCompletion::StoppedAtZero);
  } else {
    pendingRemoteCompletion = PendingRemoteCompletion::None;
  }
  (void)saveLastCommand();
  (void)pump.poll(true);
  statusPending = true;
  return accepted;
}

bool sendStatus(bool confirmed) {
  if (!lorawanActive) return false;
  (void)pump.poll(true);
  uint8_t payload[22] = {};
  buildStatus(payload);
  uint8_t downlink[RADIOLIB_LORAWAN_MAX_PAYLOAD_SIZE] = {};
  size_t downlinkLength = 0;
  LoRaWANEvent_t event = {};
  const int16_t state = lorawan.sendReceive(payload, sizeof(payload), STATUS_FPORT,
      downlink, &downlinkLength, confirmed, nullptr, &event);
  lastStatusMs = millis();
  statusPending = false;
  if (state < RADIOLIB_ERR_NONE) {
    Serial.printf("LoRaWAN status uplink failed: %s (%d)\n",
                  radioErrorMeaning(state), state);
    return false;
  }
  lastStatusSignature = statusSignature();
  hasLastStatusSignature = true;
  if (state > RADIOLIB_ERR_NONE && downlinkLength > 0) {
    processRemoteCommand(downlink, downlinkLength, event.fPort);
  }
  return state > RADIOLIB_ERR_NONE || !confirmed;
}

bool setupLoRaWAN() {
  if (!secrets::CONFIGURED) {
    Serial.println("Pump LoRaWAN credentials are not configured.");
    return false;
  }
  if (joinAttemptCount < UINT8_MAX) ++joinAttemptCount;
  Serial.printf("Pump LoRaWAN OTAA join attempt %u.\n", joinAttemptCount);
  SPI.begin(LORA_SCK_PIN, LORA_MISO_PIN, LORA_MOSI_PIN, LORA_NSS_PIN);
  int16_t state = radio.begin(868.0, 125.0, 9, 7,
      RADIOLIB_SX126X_SYNC_WORD_PRIVATE, 10, 8, 0.0, false);
  if (state != RADIOLIB_ERR_NONE) {
    scheduleJoinRetry(state);
    return false;
  }
  radio.setRfSwitchPins(LORA_RXEN_PIN, LORA_TXEN_PIN);
  state = lorawan.beginOTAA(secrets::JOIN_EUI, secrets::DEV_EUI,
                            nullptr, secrets::APP_KEY);
  if (state != RADIOLIB_ERR_NONE) {
    scheduleJoinRetry(state);
    return false;
  }
  lorawan.setADR(true);
  (void)loadNonces();
  state = lorawan.activateOTAA();
  if (state != RADIOLIB_LORAWAN_NEW_SESSION &&
      state != RADIOLIB_LORAWAN_SESSION_RESTORED) {
    (void)saveNonces();
    scheduleJoinRetry(state);
    return false;
  }
  if (!saveNonces()) Serial.println("Warning: LoRaWAN nonces were not saved.");
  lorawanActive = true;
  nextJoinAttemptMs = 0;
  Serial.printf("Pump LoRaWAN session active after %u attempt(s).\n",
                joinAttemptCount);
  state = lorawan.setClass(RADIOLIB_LORAWAN_CLASS_C);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("Class C unavailable (%d); using Class A windows.\n", state);
    return true;
  }
  for (uint8_t attempt = 0; attempt < 3; ++attempt) {
    if (sendStatus(true)) {
      classCActive = true;
      statusPending = true;
      Serial.println("Pump LoRaWAN Class C active.");
      return true;
    }
    delay(2000);
  }
  Serial.println("Class C activation unacknowledged; using Class A windows.");
  return true;
}

uint32_t arduinoSerialFrame(irrigation::SerialFrame frame) {
  switch (frame) {
    case irrigation::SerialFrame::EightN2:
      return SERIAL_8N2;
    case irrigation::SerialFrame::EightE1:
      return SERIAL_8E1;
    case irrigation::SerialFrame::EightO1:
      return SERIAL_8O1;
    case irrigation::SerialFrame::EightN1:
    default:
      return SERIAL_8N1;
  }
}

void printBootProfile() {
  logger.println(F("[SYSTEM] IrrigationAutomationFirmware"), true);
  logger.print(F("[BOARD] "), true);
  logger.println(irrigation::ActiveBoard.displayName, true);
  logger.print(F("[RS485] RX=GPIO"), true);
  logger.print(irrigation::ActiveBoard.rs485RxPin, true);
  logger.print(F(", TX=GPIO"), true);
  logger.print(irrigation::ActiveBoard.rs485TxPin, true);
  logger.println(F(", automatic direction"), true);
  logger.print(F("[VFD] "), true);
  logger.println(irrigation::ActiveInverter.model, true);
  logger.print(F("[MOTOR] "), true);
  logger.print(irrigation::ActiveMotor.manufacturer, true);
  logger.print(F(" "), true);
  logger.println(irrigation::ActiveMotor.model, true);
  logger.println(
      F("[SYSTEM] GPIO2 is reserved; it is not driven by this pump example."),
      true);
}

}  // namespace

void setup() {
  Serial.begin(irrigation::ActiveBoard.debugBaud);
  delay(300);
  printBootProfile();

  const Rs485DirectionMode directionMode =
      irrigation::ActiveBoard.rs485DirectionMode ==
              irrigation::Rs485DirectionMode::Automatic
          ? Rs485DirectionMode::Automatic
          : Rs485DirectionMode::Manual;
  rs485.setDirectionMode(directionMode,
                         irrigation::ActiveBoard.rs485DirectionPin,
                         irrigation::ActiveBoard.rs485DirectionActiveHigh);
  rs485.setDebug(&logger);
  rs485.setTimings(0, 0, 5);
  rs485.begin(vfdSerial, irrigation::ActiveInverter.modbusBaud,
              irrigation::ActiveBoard.rs485RxPin,
              irrigation::ActiveBoard.rs485TxPin,
              arduinoSerialFrame(irrigation::ActiveInverter.modbusFrame));

  vfd.setDebug(DEBUG_MODBUS != 0);
  vfd.begin();
  pump.begin();

  if (vfd.ping()) {
    // Boot policy: issue only a stop command; never send a run command.
    pump.stop();
    const DelixiConfigurationReport report =
        vfd.checkConfiguration(irrigation::ActiveMotor);
    pump.setConfigurationValid(report.valid());
  } else {
    pump.setConfigurationValid(false);
  }

  logger.println(F("[SYSTEM] Boot complete. Pump start is never automatic."),
                 true);
  logger.println(F("[SYSTEM] Type: help"), true);
  loadLastCommand();
  if (lastCommandResult == 6) {
    // A reset interrupted completion tracking. Do not report the old command
    // as still progressing when no in-memory target tracker exists.
    lastCommandResult = 2;
    (void)saveLastCommand();
  }
  (void)setupLoRaWAN();
  statusPending = lorawanActive;
}

void loop() {
  serialCommands.poll(Serial);
  pump.poll();
  updateRemoteCompletion();
  if (lorawanActive && hasLastStatusSignature &&
      statusSignature() != lastStatusSignature) {
    statusPending = true;
  }
  if (classCActive) {
    uint8_t downlink[RADIOLIB_LORAWAN_MAX_PAYLOAD_SIZE] = {};
    size_t downlinkLength = 0;
    LoRaWANEvent_t event = {};
    const int16_t state = lorawan.getDownlinkClassC(
        downlink, &downlinkLength, &event);
    if (state > 0 && downlinkLength > 0) {
      (void)processRemoteCommand(downlink, downlinkLength, event.fPort);
    } else if (state < RADIOLIB_ERR_NONE) {
      Serial.printf("Pump Class C receive error: %d\n", state);
    }
  }
  const uint32_t now = millis();
  if (lorawanActive &&
      ((statusPending && now - lastStatusMs >= STATUS_RESPONSE_MIN_GAP_MS) ||
       now - lastStatusMs >= currentStatusIntervalMs())) {
    (void)sendStatus(false);
  }
  if (!lorawanActive && secrets::CONFIGURED &&
      nextJoinAttemptMs != 0 &&
      static_cast<int32_t>(now - nextJoinAttemptMs) >= 0) {
    (void)setupLoRaWAN();
  }
  delay(5);
}
