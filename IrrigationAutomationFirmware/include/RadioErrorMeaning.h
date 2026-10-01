#pragma once

#include <RadioLib.h>

#include <stdint.h>

namespace irrigation::diagnostics {

// RadioLib 7.7.1 status-code meanings for the radio and LoRaWAN APIs used by
// the examples. Keep the numeric code in the caller's log so field reports can
// be matched to RadioLib documentation and gateway traces.
inline const char *radioErrorMeaning(int16_t state) {
  switch (state) {
    case RADIOLIB_ERR_NONE: return "no error";
    case RADIOLIB_ERR_UNKNOWN: return "unknown RadioLib error";
    case RADIOLIB_ERR_CHIP_NOT_FOUND: return "radio chip not found";
    case RADIOLIB_ERR_MEMORY_ALLOCATION_FAILED: return "memory allocation failed";
    case RADIOLIB_ERR_PACKET_TOO_LONG: return "packet is too long";
    case RADIOLIB_ERR_TX_TIMEOUT: return "radio transmit timeout";
    case RADIOLIB_ERR_RX_TIMEOUT: return "radio receive timeout";
    case RADIOLIB_ERR_CRC_MISMATCH: return "packet CRC mismatch";
    case RADIOLIB_ERR_INVALID_BANDWIDTH: return "invalid radio bandwidth";
    case RADIOLIB_ERR_INVALID_SPREADING_FACTOR:
      return "invalid LoRa spreading factor";
    case RADIOLIB_ERR_INVALID_CODING_RATE: return "invalid LoRa coding rate";
    case RADIOLIB_ERR_INVALID_BIT_RANGE: return "invalid bit range";
    case RADIOLIB_ERR_INVALID_FREQUENCY: return "invalid radio frequency";
    case RADIOLIB_ERR_INVALID_OUTPUT_POWER: return "invalid radio output power";
    case RADIOLIB_ERR_SPI_WRITE_FAILED: return "radio SPI write failed";
    case RADIOLIB_ERR_INVALID_CURRENT_LIMIT: return "invalid radio current limit";
    case RADIOLIB_ERR_INVALID_PREAMBLE_LENGTH:
      return "invalid radio preamble length";
    case RADIOLIB_ERR_INVALID_GAIN: return "invalid radio gain";
    case RADIOLIB_ERR_WRONG_MODEM: return "wrong radio modem";
    case RADIOLIB_ERR_INVALID_NUM_SAMPLES: return "invalid sample count";
    case RADIOLIB_ERR_INVALID_RSSI_OFFSET: return "invalid RSSI offset";
    case RADIOLIB_ERR_INVALID_ENCODING: return "invalid packet encoding";
    case RADIOLIB_ERR_LORA_HEADER_DAMAGED: return "LoRa header damaged";
    case RADIOLIB_ERR_UNSUPPORTED: return "unsupported radio operation";
    case RADIOLIB_ERR_INVALID_DIO_PIN: return "invalid radio DIO pin";
    case RADIOLIB_ERR_INVALID_RSSI_THRESHOLD: return "invalid RSSI threshold";
    case RADIOLIB_ERR_NULL_POINTER: return "null pointer argument";
    case RADIOLIB_ERR_INVALID_IRQ: return "invalid radio interrupt";
    case RADIOLIB_ERR_PACKET_TOO_SHORT: return "packet is too short";
    case RADIOLIB_ERR_INVALID_BIT_RATE: return "invalid bit rate";
    case RADIOLIB_ERR_INVALID_FREQUENCY_DEVIATION:
      return "invalid frequency deviation";
    case RADIOLIB_ERR_INVALID_BIT_RATE_BW_RATIO:
      return "invalid bit-rate to bandwidth ratio";
    case RADIOLIB_ERR_INVALID_RX_BANDWIDTH: return "invalid receive bandwidth";
    case RADIOLIB_ERR_INVALID_SYNC_WORD: return "invalid sync word";
    case RADIOLIB_ERR_INVALID_DATA_SHAPING: return "invalid data shaping";
    case RADIOLIB_ERR_INVALID_MODULATION: return "invalid modulation";
    case RADIOLIB_ERR_INVALID_OOK_RSSI_PEAK_TYPE:
      return "invalid OOK RSSI peak type";
    case RADIOLIB_ERR_INVALID_BIT_RATE_TOLERANCE_VALUE:
      return "invalid bit-rate tolerance";
    case RADIOLIB_ERR_INVALID_SYMBOL: return "invalid symbol";
    case RADIOLIB_ERR_INVALID_MIC_E_TELEMETRY:
      return "invalid Mic-E telemetry";
    case RADIOLIB_ERR_INVALID_MIC_E_TELEMETRY_LENGTH:
      return "invalid Mic-E telemetry length";
    case RADIOLIB_ERR_MIC_E_TELEMETRY_STATUS:
      return "invalid Mic-E telemetry status";
    case RADIOLIB_ERR_INVALID_SSDV_MODE: return "invalid SSDV mode";
    case RADIOLIB_ERR_INVALID_IMAGE_SIZE: return "invalid image size";
    case RADIOLIB_ERR_INVALID_IMAGE_QUALITY: return "invalid image quality";
    case RADIOLIB_ERR_INVALID_SUBSAMPLING: return "invalid image subsampling";
    case RADIOLIB_ERR_INVALID_RTTY_SHIFT: return "invalid RTTY shift";
    case RADIOLIB_ERR_UNSUPPORTED_ENCODING: return "unsupported encoding";
    case RADIOLIB_ERR_INVALID_DATA_RATE: return "invalid data rate";
    case RADIOLIB_ERR_INVALID_ADDRESS_WIDTH: return "invalid address width";
    case RADIOLIB_ERR_INVALID_PIPE_NUMBER: return "invalid pipe number";
    case RADIOLIB_ERR_ACK_NOT_RECEIVED: return "acknowledgement not received";
    case RADIOLIB_ERR_INVALID_NUM_BROAD_ADDRS:
      return "invalid number of broadcast addresses";
    case RADIOLIB_ERR_INVALID_CRC_CONFIGURATION:
      return "invalid CRC configuration";
    case RADIOLIB_ERR_INVALID_TCXO_VOLTAGE: return "invalid TCXO voltage";
    case RADIOLIB_ERR_INVALID_MODULATION_PARAMETERS:
      return "invalid modulation parameters";
    case RADIOLIB_ERR_SPI_CMD_TIMEOUT: return "radio SPI command timeout";
    case RADIOLIB_ERR_SPI_CMD_INVALID: return "invalid radio SPI command";
    case RADIOLIB_ERR_SPI_CMD_FAILED: return "radio SPI command failed";
    case RADIOLIB_ERR_INVALID_SLEEP_PERIOD: return "invalid radio sleep period";
    case RADIOLIB_ERR_INVALID_RX_PERIOD: return "invalid radio receive period";
    case RADIOLIB_ERR_INVALID_CALLSIGN: return "invalid callsign";
    case RADIOLIB_ERR_INVALID_NUM_REPEATERS: return "invalid repeater count";
    case RADIOLIB_ERR_INVALID_REPEATER_CALLSIGN:
      return "invalid repeater callsign";
    case RADIOLIB_ERR_RANGING_TIMEOUT: return "ranging timeout";
    case RADIOLIB_ERR_INVALID_PAYLOAD: return "invalid payload";
    case RADIOLIB_ERR_ADDRESS_NOT_FOUND: return "address not found";
    case RADIOLIB_ERR_INVALID_FUNCTION: return "invalid function";
    case RADIOLIB_ERR_NETWORK_NOT_JOINED: return "LoRaWAN network not joined";
    case RADIOLIB_ERR_DOWNLINK_MALFORMED: return "malformed LoRaWAN downlink";
    case RADIOLIB_ERR_INVALID_REVISION: return "invalid LoRaWAN revision";
    case RADIOLIB_ERR_INVALID_PORT: return "invalid LoRaWAN FPort";
    case RADIOLIB_ERR_NO_RX_WINDOW: return "no LoRaWAN receive window";
    case RADIOLIB_ERR_NO_CHANNEL_AVAILABLE:
      return "no LoRaWAN channel available";
    case RADIOLIB_ERR_INVALID_CID: return "invalid LoRaWAN MAC command ID";
    case RADIOLIB_ERR_UPLINK_UNAVAILABLE: return "LoRaWAN uplink unavailable";
    case RADIOLIB_ERR_COMMAND_QUEUE_FULL: return "LoRaWAN MAC command queue full";
    case RADIOLIB_ERR_COMMAND_QUEUE_ITEM_NOT_FOUND:
      return "LoRaWAN MAC command queue item not found";
    case RADIOLIB_ERR_JOIN_NONCE_INVALID: return "invalid LoRaWAN join nonce";
    case RADIOLIB_ERR_MIC_MISMATCH: return "LoRaWAN MIC mismatch";
    case RADIOLIB_ERR_MULTICAST_FCNT_INVALID:
      return "invalid LoRaWAN multicast frame counter";
    case RADIOLIB_ERR_DWELL_TIME_EXCEEDED: return "LoRaWAN dwell time exceeded";
    case RADIOLIB_ERR_CHECKSUM_MISMATCH: return "LoRaWAN checksum mismatch";
    case RADIOLIB_ERR_NO_JOIN_ACCEPT:
      return "no OTAA JoinAccept received in RX1/RX2";
    case RADIOLIB_LORAWAN_SESSION_RESTORED: return "LoRaWAN session restored";
    case RADIOLIB_LORAWAN_NEW_SESSION: return "new LoRaWAN session established";
    case RADIOLIB_ERR_NONCES_DISCARDED: return "LoRaWAN nonces discarded";
    case RADIOLIB_ERR_SESSION_DISCARDED: return "LoRaWAN session discarded";
    case RADIOLIB_ERR_INVALID_MODE: return "invalid LoRaWAN mode";
    case RADIOLIB_ERR_INVALID_MULTICAST_GROUP:
      return "invalid LoRaWAN multicast group";
    case RADIOLIB_ERR_INVALID_WIFI_TYPE: return "invalid Wi-Fi type";
    case RADIOLIB_ERR_GNSS_SUBFRAME_NOT_AVAILABLE:
      return "GNSS subframe not available";
    case RADIOLIB_ERR_GNSS_DEMOD_OFFSET: return "GNSS demodulation error";
    case RADIOLIB_ERR_GNSS_SOLVER_OFFSET: return "GNSS solver error";
    case RADIOLIB_ERR_FRONTEND_CALIBRATION_FAILED:
      return "radio front-end calibration failed";
    case RADIOLIB_ERR_INVALID_SIDE_DETECT: return "invalid side detect setting";
    case RADIOLIB_ERR_ADSB_INVALID_MSG_TYPE: return "invalid ADS-B message type";
    case RADIOLIB_ERR_ADSB_INVALID_CATEGORY: return "invalid ADS-B category";
    default: return "unclassified RadioLib status";
  }
}

}  // namespace irrigation::diagnostics
