#pragma once

#include <stdint.h>

namespace irrigation {

enum class Rs485DirectionMode : uint8_t {
  Manual,
  Automatic
};

enum class SerialFrame : uint8_t {
  EightN2,
  EightE1,
  EightO1,
  EightN1
};

struct BoardProfile {
  const char *id;
  const char *displayName;
  bool pinoutComplete;
  uint32_t debugBaud;
  int8_t rs485RxPin;
  int8_t rs485TxPin;
  Rs485DirectionMode rs485DirectionMode;
  int8_t rs485DirectionPin;
  bool rs485DirectionActiveHigh;
  int8_t devicePowerControlPin;
  bool devicePowerControlUsedByPumpExample;

  // Reserved for the future DX-LR30-900M22S integration.
  int8_t loraMosiPin;
  int8_t loraMisoPin;
  int8_t loraSckPin;
  int8_t loraNssPin;
  int8_t loraResetPin;
  int8_t loraBusyPin;
  int8_t loraDio1Pin;
  int8_t loraDio2Pin;
  int8_t loraRxEnablePin;
  int8_t loraTxEnablePin;
};

struct InverterProfile {
  const char *id;
  const char *manufacturer;
  const char *series;
  const char *model;
  float ratedPowerKw;
  float ratedOutputCurrentA;
  uint16_t ratedInputVoltageV;
  uint8_t modbusAddress;
  uint32_t modbusBaud;
  SerialFrame modbusFrame;
  uint16_t responseDelayMs;
  uint16_t requestTimeoutMs;
  uint8_t requestRetries;
  float maximumFrequencyHz;
  float upperFrequencyHz;
  float accelerationSeconds;
  float decelerationSeconds;
  float communicationTimeoutSeconds;
};

struct MotorProfile {
  const char *id;
  const char *manufacturer;
  const char *model;
  float ratedPowerKw;
  uint16_t ratedVoltageV;
  float ratedCurrentA;
  float ratedFrequencyHz;
  uint16_t ratedRpm;
  float minRunFrequencyHz;
  float maxRunFrequencyHz;
  bool allowReverse;
  float maximumFlowLitersPerMinute;
  float maximumHeadMeters;
  float suctionDepthMeters;
};

}  // namespace irrigation

