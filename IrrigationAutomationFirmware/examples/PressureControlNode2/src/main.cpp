#include <Arduino.h>
#include <BatteryMonitor.h>
#include <Preferences.h>
#include <PressureControlNode.h>
#include <PressureControlValve.h>
#include <PressureNode2Config.h>
#include <PressureNodeCommandProcessor.h>
#include <PressureNodeLoRaProtocol.h>
#include <PressureNodeRuntimeMode.h>
#include <PressureSensorXDB401.h>
#include <PrintController.h>
#include <RadioLib.h>
#include <SPI.h>
#include <Wire.h>
#include <esp_sleep.h>

#include <math.h>
#include <string.h>

#if __has_include("PressureNode2LoRaSecrets.h")
#include "PressureNode2LoRaSecrets.h"
#else
#include "PressureNode2LoRaSecrets.example.h"
#endif

namespace {

namespace config = irrigation::pressure_node::valve_2;
namespace lora_protocol = irrigation::pressure_node::lorawan_protocol;
namespace lora_secrets = irrigation::pressure_node::lorawan_secrets;
namespace runtime = irrigation::pressure_node::runtime;
using namespace irrigation::pressure_node;

static_assert(runtime::ACTIVE_MODE == runtime::Mode::LowPowerClassA,
              "PressureControlNode2 must use the Class A sleep policy.");

TwoWire upstreamI2c(0);
TwoWire downstreamI2c(1);

BatteryMonitor battery({
    static_cast<uint8_t>(config::pins::BATTERY_ADC),
    config::battery::DIVIDER_HIGH_OHM,
    config::battery::DIVIDER_LOW_OHM,
    config::battery::CALIBRATION,
    config::battery::SAMPLE_COUNT,
    config::battery::ADC_SETTLING_TIME_MS,
});

const Xdb401Configuration xdb401Configuration{
    config::pressure_sensor::ADDRESS_PRIMARY,
    config::pressure_sensor::ADDRESS_ALTERNATE,
    config::pressure_sensor::REG_PRESSURE,
    config::pressure_sensor::REG_TEMPERATURE,
    config::pressure_sensor::REG_MEASUREMENT,
    config::pressure_sensor::START_MEASUREMENT,
    config::pressure_sensor::BUSY_MASK,
    config::pressure_sensor::ASSUMED_FULL_SCALE_BAR,
    config::pressure_sensor::ENGINEERING_SCALE_VALIDATED,
    config::pressure_sensor::READY_POLL_ATTEMPTS,
    config::pressure_sensor::READY_POLL_INTERVAL_MS,
};

PressureSensorXDB401 upstreamPressure(upstreamI2c, xdb401Configuration);
PressureSensorXDB401 downstreamPressure(downstreamI2c, xdb401Configuration);

PressureControlValve pressureControlValve({
    static_cast<uint8_t>(config::pins::PCV_IN1),
    static_cast<uint8_t>(config::pins::PCV_IN2),
    static_cast<uint8_t>(config::pins::PCV_POWER_ENABLE),
    config::pcv::POWER_ENABLE_ACTIVE_HIGH,
    config::pcv::POWER_SETTLE_MS,
    config::pcv::SOLENOID_PULSE_MS,
    config::pcv::POST_PULSE_MS,
    config::pcv::HYDRAULIC_SETTLE_MS,
    {config::pcv::OPEN_IN1_HIGH, config::pcv::OPEN_IN2_HIGH},
    {config::pcv::CLOSE_IN1_HIGH, config::pcv::CLOSE_IN2_HIGH},
});

PressureControlNode pressureNode(
    battery, upstreamPressure, downstreamPressure, pressureControlValve,
    {
        config::energy::PERIODIC_SAMPLING_ENABLED,
        config::energy::SAMPLE_INTERVAL_MS,
        config::energy::TELEMETRY_INTERVAL_MS,
        runtime::DEEP_SLEEP_ENABLED,
    });

PrintController logger(Serial, true);
PressureNodeCommandProcessor commands(pressureNode, logger);
SerialPressureNodeCommandSource serialCommands(commands, logger);

const LoRaWANBand_t loraWanRegion = EU868;
SPISettings loraSpiSettings(500000, MSBFIRST, SPI_MODE0);
SX1262 radio = new Module(
    config::pins::LORA_NSS, config::pins::LORA_DIO1,
    config::pins::LORA_RESET, config::pins::LORA_BUSY, SPI, loraSpiSettings);
LoRaWANNode lorawan(&radio, &loraWanRegion, config::lorawan::SUB_BAND);
Preferences preferences;

inline constexpr uint32_t RTC_SESSION_MAGIC_VALUE = 0x50534331UL;
inline constexpr uint32_t NODE_STATE_MAGIC_VALUE = 0x504E5331UL;
inline constexpr uint16_t NODE_STATE_VERSION = 2;

struct PersistentNodeStateV1 {
  uint32_t magic;
  uint16_t version;
  uint16_t lastCommandId;
  uint32_t reportIntervalSeconds;
  uint8_t lastValveState;
  uint8_t statusReason;
  uint8_t hasLastCommand;
  uint8_t reserved;
  uint8_t lastCommand[lora_protocol::COMMAND_PAYLOAD_SIZE];
};

struct PersistentNodeState {
  uint32_t magic;
  uint16_t version;
  uint16_t lastCommandId;
  uint32_t reportIntervalSeconds;
  uint8_t lastValveState;
  uint8_t statusReason;
  uint8_t hasLastCommand;
  uint8_t reserved;
  uint8_t lastCommand[lora_protocol::COMMAND_PAYLOAD_SIZE];
  // Formerly the flow-total baseline (a float plus a flag) and its padding:
  // seven bytes, kept as reserved so this struct's size stays distinct from
  // PersistentNodeStateV1. loadPersistentState() chooses its migration branch by
  // blob length, so collapsing the two sizes would make the v1 branch
  // unreachable and silently reset a v1 device's stored interval and valve
  // state instead of migrating them.
  uint8_t reserved2[7];
};

static_assert(sizeof(PersistentNodeState) != sizeof(PersistentNodeStateV1),
              "Stored-state sizes must stay distinct so the v1 migration branch "
              "remains reachable.");

RTC_DATA_ATTR uint32_t rtcSessionMagic = 0;
RTC_DATA_ATTR uint8_t
    rtcLoRaWanSession[RADIOLIB_LORAWAN_SESSION_BUF_SIZE] = {0};
RTC_DATA_ATTR PersistentNodeState retainedNodeState = {};

bool radioInitialized = false;
bool loRaWanReady = false;
uint32_t nextLoRaWanActionMs = 0;

bool reportIntervalValid(uint32_t seconds) {
  return seconds >= config::lorawan::MIN_REPORT_INTERVAL_SECONDS &&
         seconds <= config::lorawan::MAX_REPORT_INTERVAL_SECONDS;
}

bool persistentStateValid(const PersistentNodeState &state) {
  return state.magic == NODE_STATE_MAGIC_VALUE &&
         state.version == NODE_STATE_VERSION &&
         reportIntervalValid(state.reportIntervalSeconds) &&
         state.lastValveState <=
             static_cast<uint8_t>(PressureControlValveState::Closed) &&
         state.statusReason <=
             static_cast<uint8_t>(
                 lora_protocol::StatusReason::FlowTotalResetFailed) &&
         state.hasLastCommand <= 1;
}

bool persistentStateV1Valid(const PersistentNodeStateV1 &state) {
  return state.magic == NODE_STATE_MAGIC_VALUE && state.version == 1 &&
         reportIntervalValid(state.reportIntervalSeconds) &&
         state.lastValveState <=
             static_cast<uint8_t>(PressureControlValveState::Closed) &&
         state.statusReason <=
             static_cast<uint8_t>(
                 lora_protocol::StatusReason::NoOpCommandAccepted) &&
         state.hasLastCommand <= 1;
}

void resetPersistentState() {
  memset(&retainedNodeState, 0, sizeof(retainedNodeState));
  retainedNodeState.magic = NODE_STATE_MAGIC_VALUE;
  retainedNodeState.version = NODE_STATE_VERSION;
  retainedNodeState.lastCommandId = UINT16_MAX;
  retainedNodeState.reportIntervalSeconds =
      config::lorawan::DEFAULT_REPORT_INTERVAL_SECONDS;
  retainedNodeState.lastValveState =
      static_cast<uint8_t>(PressureControlValveState::Unknown);
  retainedNodeState.statusReason =
      static_cast<uint8_t>(lora_protocol::StatusReason::Startup);
}

void loadPersistentState(bool wokeFromDeepSleep) {
  if (wokeFromDeepSleep && persistentStateValid(retainedNodeState)) return;

  PersistentNodeState stored{};
  bool loaded = false;
  if (preferences.begin(config::lorawan::NVS_NAMESPACE, true)) {
    const size_t length =
        preferences.getBytesLength(config::lorawan::NVS_STATE_KEY);
    if (length == sizeof(stored)) {
      loaded = preferences.getBytes(config::lorawan::NVS_STATE_KEY, &stored,
                                    sizeof(stored)) == sizeof(stored) &&
               persistentStateValid(stored);
    } else if (length == sizeof(PersistentNodeStateV1)) {
      PersistentNodeStateV1 legacy{};
      if (preferences.getBytes(config::lorawan::NVS_STATE_KEY, &legacy,
                               sizeof(legacy)) == sizeof(legacy) &&
          persistentStateV1Valid(legacy)) {
        resetPersistentState();
        retainedNodeState.reportIntervalSeconds =
            legacy.reportIntervalSeconds;
        retainedNodeState.lastValveState = legacy.lastValveState;
        // Protocol v2 changes command byte zero. Preserve operational state,
        // but discard the v1 duplicate-command fingerprint and accept a fresh
        // v2 command ID sequence.
        retainedNodeState.lastCommandId = UINT16_MAX;
        retainedNodeState.hasLastCommand = 0;
        stored = retainedNodeState;
        loaded = true;
      }
    }
    preferences.end();
  }

  if (loaded) {
    retainedNodeState = stored;
    retainedNodeState.statusReason =
        static_cast<uint8_t>(lora_protocol::StatusReason::Startup);
  } else {
    resetPersistentState();
  }
}

bool savePersistentState() {
  if (!persistentStateValid(retainedNodeState)) return false;
  if (!preferences.begin(config::lorawan::NVS_NAMESPACE, false)) return false;
  const size_t written = preferences.putBytes(
      config::lorawan::NVS_STATE_KEY, &retainedNodeState,
      sizeof(retainedNodeState));
  preferences.end();
  return written == sizeof(retainedNodeState);
}

bool loadNoncesFromNvs() {
  if (!preferences.begin(config::lorawan::NVS_NAMESPACE, true)) return false;
  const size_t length =
      preferences.getBytesLength(config::lorawan::NVS_NONCES_KEY);
  if (length != RADIOLIB_LORAWAN_NONCES_BUF_SIZE) {
    preferences.end();
    return false;
  }
  uint8_t nonces[RADIOLIB_LORAWAN_NONCES_BUF_SIZE] = {};
  const size_t read = preferences.getBytes(
      config::lorawan::NVS_NONCES_KEY, nonces, sizeof(nonces));
  preferences.end();
  return read == sizeof(nonces) &&
         lorawan.setBufferNonces(nonces) == RADIOLIB_ERR_NONE;
}

bool saveNoncesToNvs() {
  if (!preferences.begin(config::lorawan::NVS_NAMESPACE, false)) return false;
  const size_t written = preferences.putBytes(
      config::lorawan::NVS_NONCES_KEY, lorawan.getBufferNonces(),
      RADIOLIB_LORAWAN_NONCES_BUF_SIZE);
  preferences.end();
  return written == RADIOLIB_LORAWAN_NONCES_BUF_SIZE;
}

void saveSessionToRtc() {
  memcpy(rtcLoRaWanSession, lorawan.getBufferSession(),
         RADIOLIB_LORAWAN_SESSION_BUF_SIZE);
  rtcSessionMagic = RTC_SESSION_MAGIC_VALUE;
}

void printHexFrame(const char *label, const uint8_t *data, size_t length) {
  Serial.print(label);
  for (size_t i = 0; i < length; ++i) {
    if (data[i] < 0x10) Serial.print('0');
    Serial.print(data[i], HEX);
    if (i + 1 < length) Serial.print(' ');
  }
  Serial.println();
}

void printBootStatus(bool essentialHardwareReady) {
  logger.println(F("[SYSTEM] Latching-valve monitoring end node, Rev A"),
                 true);
  logger.print(F("[SYSTEM] Firmware mode: "), true);
  logger.println(runtime::modeName(runtime::ACTIVE_MODE), true);
  logger.println(F("[SYSTEM] PCV IN1=GPIO2, IN2=GPIO15."), true);
  logger.println(
      F("[SYSTEM] Dual I2C pressure sensors remain enabled for Rev A."),
      true);
  logger.println(
      PCV_NO_FLOW_ACTUATION_ENABLED
          ? F("[CONTROL] Valve pulses enabled; verify polarity and timing on this valve.")
          : F("[CONTROL] Valve pulses locked pending polarity/pulse validation."),
      true);
  if (runtime::ACTIVE_MODE == runtime::Mode::SerialOnly) {
    logger.println(F("[CONTROL] Serial commands only; LoRaWAN is disabled."),
                   true);
  } else if (runtime::ACTIVE_MODE == runtime::Mode::HybridClassC) {
    logger.println(
        lora_secrets::CONFIGURED
            ? F("[CONTROL] Serial commands plus always-listening LoRaWAN Class C.")
            : F("[LORAWAN][ERROR] OTAA credentials missing; Serial remains available."),
        true);
  } else {
    logger.println(
        lora_secrets::CONFIGURED
            ? F("[CONTROL] LoRaWAN Class A wake/status/command/ack/deep-sleep cycle.")
            : F("[LORAWAN][ERROR] OTAA credentials missing; retrying after deep sleep."),
        true);
    logger.println(
        F("[SERIAL] Diagnostic output only; Serial commands are disabled."),
        true);
  }
  logger.println(essentialHardwareReady
                     ? F("[SYSTEM] Core hardware initialization complete.")
                     : F("[SYSTEM][ERROR] Review status before actuation."),
                 true);
  if (runtime::SERIAL_COMMANDS_ENABLED)
    logger.println(F("[SYSTEM] Type: help"), true);
}

bool setupLoRaWan() {
  SPI.begin(config::pins::LORA_SCK, config::pins::LORA_MISO,
            config::pins::LORA_MOSI, config::pins::LORA_NSS);
  delay(100);

  int16_t state = radio.begin(
      868.0, 125.0, 9, 7, RADIOLIB_SX126X_SYNC_WORD_PRIVATE, 10, 8, 0.0,
      false);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("[LORAWAN] SX1262 initialization failed: %d\n", state);
    return false;
  }
  radioInitialized = true;
  radio.setRfSwitchPins(config::pins::LORA_RX_ENABLE,
                        config::pins::LORA_TX_ENABLE);

  state = lorawan.beginOTAA(lora_secrets::JOIN_EUI, lora_secrets::DEV_EUI,
                            nullptr, lora_secrets::APP_KEY);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("[LORAWAN] beginOTAA failed: %d\n", state);
    return false;
  }
  lorawan.setADR(true);
  lorawan.setDeviceStatus(255);  // Battery SOC is not calibrated yet.

  const bool noncesRestored = loadNoncesFromNvs();
  // This is a migration of the already-joined valve_2 device. Reusing a
  // DevNonce after NVS erase can make ChirpStack reject OTAA or weaken the
  // join history. Require the prior nonce buffer or a freshly provisioned
  // device identity and explicit migration procedure.
  if (!noncesRestored) {
    Serial.println("[LORAWAN][ERROR] Existing valve_2 OTAA nonces missing; join blocked.");
    return false;
  }
  if (noncesRestored && rtcSessionMagic == RTC_SESSION_MAGIC_VALUE)
    (void)lorawan.setBufferSession(rtcLoRaWanSession);

  state = lorawan.activateOTAA();
  if (state != RADIOLIB_LORAWAN_NEW_SESSION &&
      state != RADIOLIB_LORAWAN_SESSION_RESTORED) {
    (void)saveNoncesToNvs();
    rtcSessionMagic = 0;
    Serial.printf("[LORAWAN] OTAA activation failed: %d\n", state);
    return false;
  }
  // Do not rewrite flash on every one-minute RTC session restore. DevNonce
  // changes only when a new join is performed; failed joins were saved above.
  if (state == RADIOLIB_LORAWAN_NEW_SESSION && !saveNoncesToNvs())
    Serial.println("[LORAWAN] Warning: OTAA nonces were not saved.");
  saveSessionToRtc();
  Serial.println(state == RADIOLIB_LORAWAN_NEW_SESSION
                     ? "[LORAWAN] New OTAA session established."
                     : "[LORAWAN] Session restored from RTC memory.");
  state = lorawan.setClass(RADIOLIB_LORAWAN_CLASS_A);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("[LORAWAN] Could not select Class A: %d\n", state);
    return false;
  }
  return true;
}

bool isExactDuplicate(const uint8_t *payload,
                      const lora_protocol::DownlinkCommand &command) {
  return retainedNodeState.hasLastCommand != 0 &&
         command.commandId == retainedNodeState.lastCommandId &&
         memcmp(payload, retainedNodeState.lastCommand,
                lora_protocol::COMMAND_PAYLOAD_SIZE) == 0;
}

void processRemoteCommand(const uint8_t *payload, size_t length,
                          uint8_t fPort) {
  lora_protocol::DownlinkCommand command{};
  const lora_protocol::CommandDecodeStatus decoded =
      lora_protocol::decodeDownlink(
          payload, length, fPort, config::lorawan::COMMAND_FPORT,
          config::lorawan::MIN_REPORT_INTERVAL_SECONDS,
          config::lorawan::MAX_REPORT_INTERVAL_SECONDS, command);
  if (decoded != lora_protocol::CommandDecodeStatus::Ok) {
    retainedNodeState.statusReason = static_cast<uint8_t>(
        lora_protocol::StatusReason::InvalidCommandRejected);
    Serial.printf("[LORAWAN] Downlink rejected: %s.\n",
                  lora_protocol::commandDecodeStatusName(decoded));
    return;
  }

  if (command.hasFlowTotalReset) {
    retainedNodeState.statusReason = static_cast<uint8_t>(
        lora_protocol::StatusReason::InvalidCommandRejected);
    Serial.println("[LORAWAN] Flow-total reset rejected: no flow meter fitted.");
    return;
  }
  if (command.hasValveAction &&
      command.valveAction != lora_protocol::ValveAction::None &&
      !PCV_NO_FLOW_ACTUATION_ENABLED) {
    retainedNodeState.statusReason = static_cast<uint8_t>(
        lora_protocol::StatusReason::InvalidCommandRejected);
    Serial.println("[LORAWAN] Valve command rejected: actuation locked.");
    return;
  }

  if (retainedNodeState.hasLastCommand != 0 &&
      command.commandId == retainedNodeState.lastCommandId) {
    const bool exactDuplicate = isExactDuplicate(payload, command);
    retainedNodeState.statusReason = static_cast<uint8_t>(
        exactDuplicate
            ? lora_protocol::StatusReason::DuplicateCommandIgnored
            : lora_protocol::StatusReason::InvalidCommandRejected);
    Serial.printf(exactDuplicate
                      ? "[LORAWAN] Duplicate command %u ignored safely.\n"
                      : "[LORAWAN] Command ID %u was reused with different data.\n",
                  command.commandId);
    return;
  }

  const bool requestsValveActuation =
      command.hasValveAction &&
      command.valveAction != lora_protocol::ValveAction::None;
  bool valveApplied = false;
  if (command.hasReportInterval)
    retainedNodeState.reportIntervalSeconds = command.reportIntervalSeconds;

  if (requestsValveActuation) {
    const PressureControlValveState desiredState =
        command.valveAction == lora_protocol::ValveAction::Open
            ? PressureControlValveState::Open
            : PressureControlValveState::Closed;
    Serial.printf("[LORAWAN] Applying remote PCV command %s, ID=%u.\n",
                  valveStateName(desiredState), command.commandId);
    const ValveCommandResult result = pressureNode.commandValve(desiredState);
    valveApplied = result.ok();
    if (valveApplied) {
      retainedNodeState.lastValveState =
          static_cast<uint8_t>(desiredState);
    } else {
      retainedNodeState.statusReason = static_cast<uint8_t>(
          lora_protocol::StatusReason::ValveActuationFailed);
      Serial.printf("[LORAWAN] PCV actuation failed: %s.\n",
                    valveActuationStatusName(result.actuation));
    }
  }

  // A flow-total reset never reaches this point: it is rejected above, because
  // this node has no meter to hold a baseline.
  if (!requestsValveActuation || valveApplied) {
    if (requestsValveActuation && command.hasReportInterval) {
      retainedNodeState.statusReason = static_cast<uint8_t>(
          lora_protocol::StatusReason::ValveAndReportIntervalApplied);
    } else if (requestsValveActuation) {
      retainedNodeState.statusReason = static_cast<uint8_t>(
          command.valveAction == lora_protocol::ValveAction::Open
              ? lora_protocol::StatusReason::RemoteOpenApplied
              : lora_protocol::StatusReason::RemoteCloseApplied);
    } else if (command.hasReportInterval) {
      retainedNodeState.statusReason = static_cast<uint8_t>(
          lora_protocol::StatusReason::ReportIntervalApplied);
    } else {
      retainedNodeState.statusReason = static_cast<uint8_t>(
          lora_protocol::StatusReason::NoOpCommandAccepted);
    }
  }

  retainedNodeState.lastCommandId = command.commandId;
  retainedNodeState.hasLastCommand = 1;
  memcpy(retainedNodeState.lastCommand, payload,
         lora_protocol::COMMAND_PAYLOAD_SIZE);
  if (!savePersistentState())
    Serial.println("[LORAWAN] Warning: node state was not saved to NVS.");
  if (runtime::DEEP_SLEEP_ENABLED) {
    Serial.printf("[POWER] Deep-sleep interval: %lu seconds.\n",
                  static_cast<unsigned long>(
                      retainedNodeState.reportIntervalSeconds));
  } else {
    Serial.printf("[LORAWAN] Report interval: %lu seconds; ESP32 remains awake.\n",
                  static_cast<unsigned long>(
                      retainedNodeState.reportIntervalSeconds));
  }
}

struct StatusUplinkResult {
  bool sent{false};
  bool applicationDownlinkReceived{false};
};

void buildCurrentStatusPayload(uint8_t *uplink) {
  lora_protocol::buildStatusPayload(
      pressureNode.status(),
      static_cast<lora_protocol::StatusReason>(retainedNodeState.statusReason),
      retainedNodeState.reportIntervalSeconds,
      retainedNodeState.lastCommandId, uplink);
}

StatusUplinkResult sendStatusUplink() {
  const PressureControlNodeStatus &status = pressureNode.refreshStatus();
  // This payload carries the last *completed* cycle's reason; the successful
  // send below then replaces it with PeriodicReport for the next one. The
  // ordering is deliberate and must not be "corrected": a cycle that fails to
  // join or fails to transmit produces no uplink at all, so the following
  // uplink is the only place a join failure can ever reach the server. Moving
  // the assignment above this call would silently delete that telemetry. A
  // failure that persists keeps reporting itself, because the assignment is
  // reached only after a successful send.
  uint8_t uplink[lora_protocol::STATUS_PAYLOAD_SIZE] = {};
  lora_protocol::buildStatusPayload(
      status,
      static_cast<lora_protocol::StatusReason>(
          retainedNodeState.statusReason),
      retainedNodeState.reportIntervalSeconds,
      retainedNodeState.lastCommandId,
      uplink);
  printHexFrame("[LORAWAN] Status FPort 31: ", uplink, sizeof(uplink));

  // RadioLib copies the complete decrypted application payload before our
  // protocol-length validation, so the buffer must hold its documented
  // LoRaWAN maximum even though valid PCV commands are exactly 10 bytes.
  uint8_t downlink[RADIOLIB_LORAWAN_MAX_PAYLOAD_SIZE] = {};
  size_t downlinkLength = sizeof(downlink);
  LoRaWANEvent_t downlinkEvent = {};
  const int16_t state = lorawan.sendReceive(
      uplink, sizeof(uplink), config::lorawan::STATUS_FPORT, downlink,
      &downlinkLength, config::lorawan::CONFIRMED_UPLINK, nullptr,
      &downlinkEvent);
  saveSessionToRtc();

  if (state < RADIOLIB_ERR_NONE) {
    retainedNodeState.statusReason =
        static_cast<uint8_t>(lora_protocol::StatusReason::LoRaWanError);
    Serial.printf("[LORAWAN] Uplink failed: %d\n", state);
    return {};
  }

  Serial.println("[LORAWAN] Status uplink sent; RX1/RX2 completed.");
  retainedNodeState.statusReason =
      static_cast<uint8_t>(lora_protocol::StatusReason::PeriodicReport);
  if (state > RADIOLIB_ERR_NONE && downlinkLength > 0) {
    printHexFrame("[LORAWAN] Downlink: ", downlink, downlinkLength);
    processRemoteCommand(downlink, downlinkLength, downlinkEvent.fPort);
    return {true, true};
  }
  return {true, false};
}

bool sendCommandAcknowledgementUplink() {
  uint8_t uplink[lora_protocol::STATUS_PAYLOAD_SIZE] = {};
  buildCurrentStatusPayload(uplink);
  printHexFrame("[LORAWAN] Command acknowledgement FPort 31: ", uplink,
                sizeof(uplink));

  const int16_t state = lorawan.sendReceive(
      uplink, sizeof(uplink), config::lorawan::STATUS_FPORT,
      config::lorawan::CONFIRMED_UPLINK);
  saveSessionToRtc();
  if (state < RADIOLIB_ERR_NONE) {
    retainedNodeState.statusReason =
        static_cast<uint8_t>(lora_protocol::StatusReason::LoRaWanError);
    Serial.printf("[LORAWAN] Command acknowledgement failed: %d\n", state);
    return false;
  }

  Serial.println("[LORAWAN] Command acknowledgement status sent.");
  retainedNodeState.statusReason =
      static_cast<uint8_t>(lora_protocol::StatusReason::PeriodicReport);
  return true;
}

void persistLocalStateIfChanged() {
  bool changed = false;
  const PressureControlValveState current =
      pressureNode.status().valve.lastCommanded;
  if (current != PressureControlValveState::Unknown &&
      retainedNodeState.lastValveState != static_cast<uint8_t>(current)) {
    retainedNodeState.lastValveState = static_cast<uint8_t>(current);
    retainedNodeState.statusReason = static_cast<uint8_t>(
        lora_protocol::StatusReason::LocalCommandApplied);
    changed = true;
  }

  if (changed && !savePersistentState())
    Serial.println("[SYSTEM] Warning: local node state was not saved.");
}

bool deadlineReached(uint32_t now, uint32_t deadline) {
  return static_cast<int32_t>(now - deadline) >= 0;
}

void setRfSwitchOff() {
  pinMode(config::pins::LORA_RX_ENABLE, OUTPUT);
  pinMode(config::pins::LORA_TX_ENABLE, OUTPUT);
  digitalWrite(config::pins::LORA_RX_ENABLE, LOW);
  digitalWrite(config::pins::LORA_TX_ENABLE, LOW);
}

void sleepRadio() {
  if (radioInitialized) {
    const int16_t state = radio.sleep(true);
    if (state != RADIOLIB_ERR_NONE)
      Serial.printf("[LORAWAN] Radio sleep warning: %d\n", state);
  }
  setRfSwitchOff();
}

void scheduleNextReport(uint32_t delayMs) {
  nextLoRaWanActionMs = millis() + delayMs;
  Serial.printf("[LORAWAN] Next status/retry in %lu seconds; Serial remains active.\n",
                static_cast<unsigned long>(delayMs / 1000UL));
}

// The cadence used for the next wake: the configured interval, except while a
// valve is open, when the fixed short interval applies. Only ever shortens, so a
// configured interval below it still wins and a bench build is not slowed down.
uint32_t effectiveReportIntervalSeconds() {
  const uint32_t configured = retainedNodeState.reportIntervalSeconds;
  if (retainedNodeState.lastValveState !=
      static_cast<uint8_t>(PressureControlValveState::Open))
    return configured;
  const uint32_t openInterval =
      config::lorawan::VALVE_OPEN_REPORT_INTERVAL_SECONDS;
  return configured < openInterval ? configured : openInterval;
}

void scheduleNextConfiguredReport() {
  scheduleNextReport(effectiveReportIntervalSeconds() * 1000UL);
}

void enterDeepSleep() {
  if (!pressureNode.prepareForSleep()) {
    Serial.println("[POWER][ERROR] Low-power firmware rejected deep sleep.");
    return;
  }

  sleepRadio();
  upstreamI2c.end();
  downstreamI2c.end();
  SPI.end();

  const uint32_t sleepSeconds = effectiveReportIntervalSeconds();
  const uint64_t sleepMicroseconds =
      static_cast<uint64_t>(sleepSeconds) * 1000000ULL;
  esp_sleep_enable_timer_wakeup(sleepMicroseconds);
  Serial.printf("[POWER] Deep sleeping for %lu seconds%s.\n",
                static_cast<unsigned long>(sleepSeconds),
                sleepSeconds != retainedNodeState.reportIntervalSeconds
                    ? " (valve open)"
                    : "");
  Serial.flush();
  delay(20);
  esp_deep_sleep_start();
  while (true) delay(1000);
}

bool initializeHybridClassC() {
  Serial.println("[LORAWAN] Initializing always-listening Class C session.");
  if (!setupLoRaWan()) return false;

  const int16_t classState = lorawan.setClass(RADIOLIB_LORAWAN_CLASS_C);
  if (classState != RADIOLIB_ERR_NONE) {
    Serial.printf("[LORAWAN] Class C activation failed: %d\n", classState);
    return false;
  }
  loRaWanReady = true;
  saveSessionToRtc();
  Serial.println("[LORAWAN] Class C continuous receive is active.");

  const StatusUplinkResult initialStatus = sendStatusUplink();
  if (!initialStatus.sent) {
    loRaWanReady = false;
    setRfSwitchOff();
    return false;
  }
  if (initialStatus.applicationDownlinkReceived)
    (void)sendCommandAcknowledgementUplink();
  scheduleNextConfiguredReport();
  return true;
}

void serviceHybridClassC() {
  if (!loRaWanReady) {
    if (!deadlineReached(millis(), nextLoRaWanActionMs)) return;
    if (!initializeHybridClassC())
      scheduleNextReport(config::lorawan::JOIN_RETRY_INTERVAL_MS);
    return;
  }

  uint8_t downlink[RADIOLIB_LORAWAN_MAX_PAYLOAD_SIZE] = {};
  size_t downlinkLength = sizeof(downlink);
  LoRaWANEvent_t downlinkEvent = {};
  const int16_t downlinkState = lorawan.getDownlinkClassC(
      downlink, &downlinkLength, &downlinkEvent);
  if (downlinkState < RADIOLIB_ERR_NONE) {
    Serial.printf("[LORAWAN] Class C downlink error: %d\n", downlinkState);
  } else if (downlinkState > RADIOLIB_ERR_NONE) {
    saveSessionToRtc();
    if (downlinkLength > 0) {
      printHexFrame("[LORAWAN] Class C downlink: ", downlink,
                    downlinkLength);
      processRemoteCommand(downlink, downlinkLength, downlinkEvent.fPort);
      (void)sendCommandAcknowledgementUplink();
      scheduleNextConfiguredReport();
    }
  }

  if (!deadlineReached(millis(), nextLoRaWanActionMs)) return;
  Serial.println("[LORAWAN] Sending periodic Class C status.");
  const StatusUplinkResult result = sendStatusUplink();
  if (!result.sent) {
    loRaWanReady = false;
    setRfSwitchOff();
    scheduleNextReport(config::lorawan::JOIN_RETRY_INTERVAL_MS);
    return;
  }
  if (result.applicationDownlinkReceived)
    (void)sendCommandAcknowledgementUplink();
  scheduleNextConfiguredReport();
}

void runLowPowerClassACycle() {
  if (!lora_secrets::CONFIGURED) {
    retainedNodeState.statusReason =
        static_cast<uint8_t>(lora_protocol::StatusReason::LoRaWanError);
    enterDeepSleep();
    return;
  }

  Serial.println("[LORAWAN] Starting low-power Class A cycle.");
  if (!setupLoRaWan()) {
    retainedNodeState.statusReason =
        static_cast<uint8_t>(lora_protocol::StatusReason::LoRaWanError);
    enterDeepSleep();
    return;
  }

  const StatusUplinkResult result = sendStatusUplink();
  if (result.applicationDownlinkReceived)
    (void)sendCommandAcknowledgementUplink();
  enterDeepSleep();
}

}  // namespace

void setup() {
  // Safe outputs are asserted before Serial, sensors, UART, or radio start.
  // The assembled board must also hold GPIO27 at the bridge-OFF level in
  // hardware; firmware cannot control any GPIO during reset.
  pressureControlValve.begin();
  setRfSwitchOff();

  Serial.begin(config::DEBUG_BAUD);
  delay(300);

  const bool wokeFromDeepSleep =
      esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER;
  loadPersistentState(wokeFromDeepSleep);

  upstreamI2c.begin(config::pins::I2C_UPSTREAM_SDA,
                    config::pins::I2C_UPSTREAM_SCL,
                    config::pressure_sensor::I2C_FREQUENCY_HZ);
  downstreamI2c.begin(config::pins::I2C_DOWNSTREAM_SDA,
                      config::pins::I2C_DOWNSTREAM_SCL,
                      config::pressure_sensor::I2C_FREQUENCY_HZ);

  const bool essentialHardwareReady = pressureNode.begin();
  const auto retainedValveState = static_cast<PressureControlValveState>(
      retainedNodeState.lastValveState);
  if (retainedValveState != PressureControlValveState::Unknown)
    (void)pressureNode.restoreValveCommandedState(retainedValveState);
  printBootStatus(essentialHardwareReady);

  if (runtime::ACTIVE_MODE == runtime::Mode::SerialOnly) return;

  if (runtime::ACTIVE_MODE == runtime::Mode::LowPowerClassA) {
    runLowPowerClassACycle();
    return;
  }

  if (!lora_secrets::CONFIGURED) return;
  if (!initializeHybridClassC())
    scheduleNextReport(config::lorawan::JOIN_RETRY_INTERVAL_MS);
}

void loop() {
  if (runtime::SERIAL_COMMANDS_ENABLED) {
    (void)serialCommands.poll(Serial);
    persistLocalStateIfChanged();
  }
  if (runtime::CLASS_C_ENABLED && lora_secrets::CONFIGURED)
    serviceHybridClassC();
  delay(1);
}
