#include <Arduino.h>
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
constexpr uint32_t STATUS_INTERVAL_MS = 60000;
constexpr uint32_t JOIN_RETRY_MS = 60000;

SPISettings loraSpiSettings(500000, MSBFIRST, SPI_MODE0);
SX1262 radio = new Module(LORA_NSS_PIN, LORA_DIO1_PIN,
                          LORA_RESET_PIN, LORA_BUSY_PIN, SPI, loraSpiSettings);
LoRaWANNode lorawan(&radio, &EU868, 0);
Preferences preferences;
bool lorawanActive = false;
bool classCActive = false;
bool statusPending = false;
uint32_t lastStatusMs = 0;
uint32_t lastJoinAttemptMs = 0;
uint16_t lastCommandId = 0xFFFF;
uint8_t lastCommandOp = 0;
uint16_t lastCommandArg = 0;
uint8_t lastCommandResult = 0;

void put16(uint8_t *destination, uint16_t value) {
  destination[0] = static_cast<uint8_t>(value >> 8);
  destination[1] = static_cast<uint8_t>(value);
}

uint16_t get16(const uint8_t *source) {
  return (static_cast<uint16_t>(source[0]) << 8) | source[1];
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

void buildStatus(uint8_t (&payload)[17]) {
  const PumpStatus &s = pump.status();
  uint8_t flags = 0;
  if (s.communicationOk) flags |= 0x01;
  if (s.configurationValid) flags |= 0x02;
  if (s.running) flags |= 0x04;
  if (s.frequencyArmed) flags |= 0x08;
  if (lorawanActive) flags |= 0x10;
  if (classCActive) flags |= 0x20;
  payload[0] = 1;
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
}

bool processRemoteCommand(const uint8_t *data, size_t length, uint8_t fPort) {
  if (fPort != COMMAND_FPORT || length != 6 || data[0] != 1) {
    lastCommandResult = 4;
    statusPending = true;
    return false;
  }
  const uint8_t op = data[1];
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
  (void)saveLastCommand();
  (void)pump.poll(true);
  statusPending = true;
  return accepted;
}

bool sendStatus(bool confirmed) {
  if (!lorawanActive) return false;
  (void)pump.poll(true);
  uint8_t payload[17] = {};
  buildStatus(payload);
  uint8_t downlink[RADIOLIB_LORAWAN_MAX_PAYLOAD_SIZE] = {};
  size_t downlinkLength = 0;
  LoRaWANEvent_t event = {};
  const int16_t state = lorawan.sendReceive(payload, sizeof(payload), STATUS_FPORT,
      downlink, &downlinkLength, confirmed, nullptr, &event);
  lastStatusMs = millis();
  statusPending = false;
  if (state < RADIOLIB_ERR_NONE) {
    Serial.printf("LoRaWAN status uplink failed: %d\n", state);
    return false;
  }
  if (state > RADIOLIB_ERR_NONE && downlinkLength > 0) {
    processRemoteCommand(downlink, downlinkLength, event.fPort);
  }
  return state > RADIOLIB_ERR_NONE || !confirmed;
}

bool setupLoRaWAN() {
  lastJoinAttemptMs = millis();
  if (!secrets::CONFIGURED) {
    Serial.println("Pump LoRaWAN credentials are not configured.");
    return false;
  }
  SPI.begin(LORA_SCK_PIN, LORA_MISO_PIN, LORA_MOSI_PIN, LORA_NSS_PIN);
  int16_t state = radio.begin(868.0, 125.0, 9, 7,
      RADIOLIB_SX126X_SYNC_WORD_PRIVATE, 10, 8, 0.0, false);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("SX1262 initialization failed: %d\n", state);
    return false;
  }
  radio.setRfSwitchPins(LORA_RXEN_PIN, LORA_TXEN_PIN);
  state = lorawan.beginOTAA(secrets::JOIN_EUI, secrets::DEV_EUI,
                            nullptr, secrets::APP_KEY);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("LoRaWAN beginOTAA failed: %d\n", state);
    return false;
  }
  lorawan.setADR(true);
  (void)loadNonces();
  state = lorawan.activateOTAA();
  if (state != RADIOLIB_LORAWAN_NEW_SESSION &&
      state != RADIOLIB_LORAWAN_SESSION_RESTORED) {
    (void)saveNonces();
    Serial.printf("Pump OTAA activation failed: %d\n", state);
    return false;
  }
  if (!saveNonces()) Serial.println("Warning: LoRaWAN nonces were not saved.");
  lorawanActive = true;
  Serial.println("Pump LoRaWAN session active.");
  state = lorawan.setClass(RADIOLIB_LORAWAN_CLASS_C);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("Class C unavailable (%d); using Class A windows.\n", state);
    return true;
  }
  for (uint8_t attempt = 0; attempt < 3; ++attempt) {
    if (sendStatus(true)) {
      classCActive = true;
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
  (void)setupLoRaWAN();
  statusPending = lorawanActive;
}

void loop() {
  serialCommands.poll(Serial);
  pump.poll();
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
      ((statusPending && now - lastStatusMs >= 5000) ||
       now - lastStatusMs >= STATUS_INTERVAL_MS)) {
    (void)sendStatus(false);
  }
  if (!lorawanActive && secrets::CONFIGURED &&
      now - lastJoinAttemptMs >= JOIN_RETRY_MS) {
    (void)setupLoRaWAN();
  }
  delay(5);
}
