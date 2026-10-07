#include <Arduino.h>
#include <cmath>
#include <cstdio>
#include <CommandProcessor.h>
#include <ProjectConfig.h>
#include <Preferences.h>
#include <RadioLib.h>
#include <RadioErrorMeaning.h>
#include <SerialAccess.h>
#include <SPI.h>

// PlatformIO's dependency finder only scans src/, so the framework
// BluetoothSerial library is pulled in here rather than only from the project
// header. Including it does not use the deprecated type, so it warns nothing.
#include <BluetoothSerial.h>
#include "BluetoothConsole.h"

#if __has_include("PumpControlLoRaSecrets.h")
#include "PumpControlLoRaSecrets.h"
#else
#include "PumpControlLoRaSecrets.example.h"
#endif

namespace {

irrigation::SerialAccess serialAccess(Serial);
#undef Serial

// Logs fan out to USB and to the password-gated Bluetooth console. Each channel
// gates itself, so a locked or absent one discards its copy rather than
// stalling the other. Commands are polled from each channel separately below.
WirelessConsole wirelessConsole;
LogTee logTee(serialAccess, wirelessConsole);
#define Serial logTee

HardwareSerial vfdSerial(2);
PrintController logger(logTee, true);
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

// AUTO/MANUAL selector input. The switch connects GPIO27 to ESP32 GND.
// INPUT_PULLUP means HIGH = AUTO and LOW = MANUAL.
constexpr int MANUAL_MODE_PIN = 27;
constexpr uint32_t MANUAL_DEBOUNCE_MS = 50;

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

// Debounced AUTO/MANUAL selector state. It is telemetry-only; the selector
// does not yet block local or LoRaWAN pump commands.
bool manualMode = false;
bool manualModeRaw = false;
uint32_t manualModeRawChangedMs = 0;
uint32_t lastStatusMs = 0;
uint32_t nextJoinAttemptMs = 0;
uint64_t lastStatusSignature = 0;
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
// The last frequency this node wrote, in hundredths of a hertz, remembered so a
// restart reports it instead of the minRunFrequencyHz seed from pump.begin().
// Zero means nothing has been written yet, which 0 Hz cannot be: the profile's
// minimum run frequency is 10 Hz (config/motors/Grandfar_2CP50_160B.h).
uint16_t persistedFrequencyHz100 = 0;

void put16(uint8_t *destination, uint16_t value) {
  destination[0] = static_cast<uint8_t>(value >> 8);
  destination[1] = static_cast<uint8_t>(value);
}

uint16_t get16(const uint8_t *source) {
  return (static_cast<uint16_t>(source[0]) << 8) | source[1];
}

void printHexFrame(const char *label, const uint8_t *data, size_t length) {
  Serial.print(label);
  for (size_t index = 0; index < length; ++index) {
    if (data[index] < 0x10) Serial.print('0');
    Serial.print(data[index], HEX);
    if (index + 1 < length) Serial.print(' ');
  }
  Serial.println();
}

const char *commandOpName(uint8_t op) {
  switch (op) {
    case 1:
      return "stop";
    case 2:
      return "estop";
    case 3:
      return "set_frequency";
    case 4:
      return "start";
    default:
      return "unknown";
  }
}

// Last VFD status the poll reported, so a state that persists does not reprint at
// the 1 Hz poll rate. statusSignature() detects the change that schedules a
// status uplink; this snapshot supplies the field names for the serial line.
struct ReportedStatus {
  bool communicationOk{false};
  bool configurationValid{false};
  bool frequencyArmed{false};
  DelixiRunState runState{DelixiRunState::Unknown};
  uint16_t faultCode{0};
};
ReportedStatus lastReportedStatus;
bool hasLastReportedStatus = false;

void logStatusChanges() {
  const PumpStatus &s = pump.status();
  if (!hasLastReportedStatus) {
    lastReportedStatus = {s.communicationOk, s.configurationValid,
                          s.frequencyArmed, s.runState, s.vfdFaultCode};
    hasLastReportedStatus = true;
    return;
  }
  if (s.communicationOk != lastReportedStatus.communicationOk) {
    Serial.printf("[VFD] Communication %s.\n",
                  s.communicationOk ? "restored" : "failed");
  }
  if (s.configurationValid != lastReportedStatus.configurationValid) {
    Serial.printf("[VFD] Configuration %s.\n",
                  s.configurationValid ? "valid" : "invalid");
  }
  if (s.frequencyArmed != lastReportedStatus.frequencyArmed) {
    Serial.printf("[PUMP] Frequency %s.\n",
                  s.frequencyArmed ? "armed" : "not armed");
  }
  if (s.runState != lastReportedStatus.runState) {
    Serial.printf("[PUMP] Run state changed to %s.\n",
                  DelixiCDIE100::runStateName(s.runState));
  }
  if (s.vfdFaultCode != lastReportedStatus.faultCode) {
    Serial.printf("[VFD] Fault changed to %s [0x%04X].\n",
                  DelixiCDIE100::faultName(s.vfdFaultCode), s.vfdFaultCode);
  }
  lastReportedStatus = {s.communicationOk, s.configurationValid,
                        s.frequencyArmed, s.runState, s.vfdFaultCode};
}

bool readManualModePin() {
  return digitalRead(MANUAL_MODE_PIN) == LOW;
}

void updateManualMode() {
  const bool raw = readManualModePin();
  const uint32_t now = millis();

  if (raw != manualModeRaw) {
    manualModeRaw = raw;
    manualModeRawChangedMs = now;
  }

  if (manualMode != manualModeRaw &&
      now - manualModeRawChangedMs >= MANUAL_DEBOUNCE_MS) {
    manualMode = manualModeRaw;
    Serial.printf("[CONTROL] Mode changed to %s\n",
                  manualMode ? "MANUAL" : "AUTO");
    statusPending = true;
  }
}

// The status packed so a change schedules an uplink. The commanded frequency is part
// of it because a local Serial command reaches the VFD through CommandProcessor,
// whose SerialCommandSource::poll() cannot flag the status dirty: without this term,
// `pump freq 24` changed nothing in the signature and waited for the periodic
// interval instead of reporting. The actual frequency is deliberately excluded — it
// moves while the motor ramps and would schedule an uplink on every poll.
uint64_t statusSignature() {
  const PumpStatus &s = pump.status();
  uint64_t signature = s.communicationOk ? 1ULL : 0ULL;
  signature |= s.configurationValid ? (1ULL << 1) : 0ULL;
  signature |= s.running ? (1ULL << 2) : 0ULL;
  signature |= s.frequencyArmed ? (1ULL << 3) : 0ULL;
  signature |= static_cast<uint64_t>(s.runState) << 4;
  signature |= static_cast<uint64_t>(s.lastCommunicationError) << 8;
  signature |= static_cast<uint64_t>(s.vfdFaultCode) << 16;
  signature |= static_cast<uint64_t>(static_cast<uint16_t>(
                   s.commandedFrequencyHz * 100.0f + 0.5f))
               << 32;
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
      "[LORAWAN][ERROR] Join attempt %u failed: %s [%d]; next attempt in %u s.\n",
      joinAttemptCount, irrigation::diagnostics::radioErrorMeaning(state), state,
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

// Store the frequency a later boot should report, in hundredths of a hertz.
bool saveCommandedFrequency(float hz) {
  const uint16_t stored = static_cast<uint16_t>(hz * 100.0f + 0.5f);
  if (!preferences.begin("pump-cmd", false)) return false;
  const bool ok = preferences.putUShort("hz", stored) == sizeof(stored);
  preferences.end();
  return ok;
}

uint16_t loadCommandedFrequencyHz100() {
  if (!preferences.begin("pump-cmd", false)) return 0;
  const uint16_t stored = preferences.getUShort("hz", 0);
  preferences.end();
  return stored;
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
          true, "[PUMP] Start complete: requested frequency reached.");
      return;
    }
    if (pendingRemoteCompletion == PendingRemoteCompletion::StoppedAtZero &&
        s.runState == DelixiRunState::Stopped &&
        s.actualFrequencyHz <= STOP_FREQUENCY_TOLERANCE_HZ) {
      finishRemoteCompletion(
          true, "[PUMP] Stop complete: VFD stopped at approximately 0 Hz.");
      return;
    }
  }
  if (millis() - remoteCompletionStartedMs >= REMOTE_COMPLETION_TIMEOUT_MS) {
    finishRemoteCompletion(
        false,
        "[PUMP][ERROR] Command completion timed out before the final VFD condition.");
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
  if (manualMode) flags |= 0x40;
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
    Serial.printf(
        "[LORAWAN][ERROR] Command rejected: FPort %u, length %u, version %u.\n",
        fPort, static_cast<unsigned>(length),
        static_cast<unsigned>(length > 0 ? data[0] : 0));
    lastCommandResult = 4;
    statusPending = true;
    return false;
  }
  const uint8_t op = data[1];
  if (op == 5 && length == 2) {
    Serial.println("[LORAWAN] Status refresh requested by dashboard.");
    statusPending = true;
    return true;
  }
  if (length != 6) {
    Serial.printf("[LORAWAN][ERROR] Command rejected: %u bytes, expected 6.\n",
                  static_cast<unsigned>(length));
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
    Serial.printf(
        "[LORAWAN][ERROR] Command rejected: op %u, id %u, argument %u out of range.\n",
        op, id, arg);
    lastCommandResult = 4;
    statusPending = true;
    return false;
  }
  if (id == lastCommandId) {
    const bool identical = op == lastCommandOp && arg == lastCommandArg;
    if (identical) {
      Serial.printf("[LORAWAN] Duplicate command %u ignored safely.\n", id);
    } else {
      Serial.printf(
          "[LORAWAN][ERROR] Command ID %u was reused with different data.\n", id);
    }
    lastCommandResult = identical ? 3 : 4;
    statusPending = true;
    return lastCommandResult == 3;
  }
  if (lastCommandId != 0xFFFF &&
      static_cast<int16_t>(id - lastCommandId) <= 0) {
    Serial.printf(
        "[LORAWAN][ERROR] Command ID %u is not newer than the accepted ID %u.\n",
        id, lastCommandId);
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
    Serial.printf("[LORAWAN][ERROR] Command ID %u was not saved to NVS.\n", id);
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
  Serial.printf("[LORAWAN] Command %s id=%u %s.\n", commandOpName(op), id,
                accepted ? "accepted" : "refused by the controller");
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
  printHexFrame("[LORAWAN] Status FPort 51: ", payload, sizeof(payload));
  const int16_t state = lorawan.sendReceive(payload, sizeof(payload), STATUS_FPORT,
      downlink, &downlinkLength, confirmed, nullptr, &event);
  lastStatusMs = millis();
  statusPending = false;
  if (state < RADIOLIB_ERR_NONE) {
    Serial.printf("[LORAWAN][ERROR] Status uplink failed: %s [%d]\n",
                  irrigation::diagnostics::radioErrorMeaning(state), state);
    return false;
  }
  lastStatusSignature = statusSignature();
  hasLastStatusSignature = true;
  Serial.println("[LORAWAN] Status uplink sent.");
  if (state > RADIOLIB_ERR_NONE && downlinkLength > 0) {
    printHexFrame("[LORAWAN] Downlink: ", downlink, downlinkLength);
    processRemoteCommand(downlink, downlinkLength, event.fPort);
  }
  return state > RADIOLIB_ERR_NONE || !confirmed;
}

bool setupLoRaWAN() {
  if (!secrets::CONFIGURED) {
    Serial.println("[LORAWAN][ERROR] Credentials are not configured.");
    return false;
  }
  if (joinAttemptCount < UINT8_MAX) ++joinAttemptCount;
  Serial.printf("[LORAWAN] OTAA join attempt %u.\n", joinAttemptCount);
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
  if (!saveNonces()) Serial.println("[LORAWAN][WARN] Nonces were not saved.");
  lorawanActive = true;
  nextJoinAttemptMs = 0;
  Serial.printf("[LORAWAN] Session active after %u attempt(s).\n",
                joinAttemptCount);
  state = lorawan.setClass(RADIOLIB_LORAWAN_CLASS_C);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("[LORAWAN][ERROR] Class C unavailable: %s [%d]; using Class A windows.\n",
                  irrigation::diagnostics::radioErrorMeaning(state), state);
    return true;
  }
  for (uint8_t attempt = 0; attempt < 3; ++attempt) {
    if (sendStatus(true)) {
      classCActive = true;
      statusPending = true;
      Serial.println("[LORAWAN] Class C active.");
      return true;
    }
    delay(2000);
  }
  Serial.println(
      "[LORAWAN][WARN] Class C activation unacknowledged; using Class A windows.");
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

void startBluetoothConsole() {
  const uint64_t mac = ESP.getEfuseMac();
  char name[24];
  snprintf(name, sizeof(name), "PumpNode-%02X%02X",
           static_cast<unsigned>((mac >> 8) & 0xFF),
           static_cast<unsigned>(mac & 0xFF));
  if (wirelessConsole.begin(name)) {
    logger.print(F("[SYSTEM] Bluetooth console "), true);
    logger.print(name, true);
    logger.println(F(" ready; enter the password over the wireless stream."),
                   true);
  } else {
    logger.println(
        F("[SYSTEM] Bluetooth console unavailable; USB Serial only."), true);
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
  serialAccess.begin(irrigation::ActiveBoard.debugBaud);
  delay(300);

  pinMode(MANUAL_MODE_PIN, INPUT_PULLUP);
  manualMode = readManualModePin();
  manualModeRaw = manualMode;
  manualModeRawChangedMs = millis();

  // Start the wireless console before the USB authentication wait so the node
  // is discoverable during that 30-second window.
  startBluetoothConsole();
  (void)serialAccess.waitForAuthentication();

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
  persistedFrequencyHz100 = loadCommandedFrequencyHz100();
  pump.restoreCommandedFrequency(
      static_cast<float>(persistedFrequencyHz100) / 100.0f);
  // Say which of the two the status is about to report, so a boot log shows
  // whether NVS still holds a frequency.
  if (pump.status().commandedSet) {
    logger.print(F("[PUMP] Restored commanded frequency: "), true);
    logger.print(pump.status().commandedFrequencyHz, true, "", 2);
    logger.println(F(" Hz"), true);
  } else {
    logger.println(F("[PUMP] No commanded frequency stored yet."), true);
  }

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
  serialAccess.poll();
  wirelessConsole.poll();
  updateManualMode();
  serialCommands.poll(serialAccess);
  serialCommands.poll(wirelessConsole);
  pump.poll();
  logStatusChanges();
  updateRemoteCompletion();
  // Persist the frequency this node writes, from a LoRaWAN command or the
  // console, so the next boot reports it rather than the begin() seed. Both
  // paths end at PumpController::setSpeedHz.
  const PumpStatus &pumpStatus = pump.status();
  if (pumpStatus.commandedSet) {
    const uint16_t writtenHz100 =
        static_cast<uint16_t>(pumpStatus.commandedFrequencyHz * 100.0f + 0.5f);
    if (writtenHz100 != persistedFrequencyHz100 &&
        saveCommandedFrequency(pumpStatus.commandedFrequencyHz)) {
      persistedFrequencyHz100 = writtenHz100;
    }
  }
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
      Serial.println("[LORAWAN] Class C downlink received.");
      printHexFrame("[LORAWAN] Downlink: ", downlink, downlinkLength);
      (void)processRemoteCommand(downlink, downlinkLength, event.fPort);
    } else if (state < RADIOLIB_ERR_NONE) {
      Serial.printf("[LORAWAN][ERROR] Class C receive error: %s [%d]\n",
                    irrigation::diagnostics::radioErrorMeaning(state), state);
    }
  }
  // A command handled above changes the VFD state in the same iteration, so report
  // those changes before the status uplink that carries them.
  logStatusChanges();
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
