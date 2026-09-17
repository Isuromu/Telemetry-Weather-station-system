// ChirpStack v4 JavaScript codec for the SoilNode Class A example.
// Uplink FPort 10: 8-byte telemetry. FPort 11: 8-byte command result.
// Downlink FPort 10: opcode, optional command ID, uint32 sleep seconds.

function readU16(bytes, offset) {
  return (bytes[offset] << 8) | bytes[offset + 1];
}

function readI16(bytes, offset) {
  var value = readU16(bytes, offset);
  return value >= 0x8000 ? value - 0x10000 : value;
}

function decodeUplink(input) {
  if (input.fPort === 11) {
    if (!input.bytes || input.bytes.length !== 8 || input.bytes[0] !== 1)
      return { errors: ["SoilNode command result must be v1 and 8 bytes"] };
    var status = input.bytes[3];
    if (status > 2)
      return { errors: ["Unknown SoilNode command result status"] };
    var commandId = readU16(input.bytes, 1);
    var seconds = ((input.bytes[4] * 0x1000000) +
      (input.bytes[5] << 16) + (input.bytes[6] << 8) + input.bytes[7]) >>> 0;
    return { data: {
      runtime_mode: "soil_node_class_a",
      message_type: "command_ack",
      command_id: commandId === 0xffff ? null : commandId,
      command_status: ["applied", "invalid", "storage_failed"][status],
      active_sleep_seconds: seconds
    } };
  }
  if (input.fPort !== 10)
    return { errors: ["SoilNode uplink must use FPort 10 or 11"] };
  if (!input.bytes || input.bytes.length !== 8)
    return { errors: ["SoilNode telemetry must contain 8 bytes"] };

  var flags = input.bytes[0];
  var sensorValid = !!(flags & 0x01);
  var warnings = [];
  if (flags & 0xfe)
    warnings.push("Unknown SoilNode telemetry flag bits are set");

  return {
    data: {
      runtime_mode: "soil_node_class_a",
      sensor_valid: sensorValid,
      temperature_c: sensorValid ? readI16(input.bytes, 1) / 100.0 : null,
      vwc_percent: sensorValid ? readU16(input.bytes, 3) / 100.0 : null,
      ec_ms_cm: sensorValid ? readU16(input.bytes, 5) / 1000.0 : null,
      battery_voltage_v: 2.0 + input.bytes[7] / 100.0
    },
    warnings: warnings
  };
}

function encodeDownlink(input) {
  var data = input.data;
  if (!data || typeof data !== "object")
    return { errors: ["Downlink data must be an object"] };

  var hasSeconds = Object.prototype.hasOwnProperty.call(data, "sleep_seconds");
  var hasMinutes = Object.prototype.hasOwnProperty.call(data, "sleep_minutes");
  if (hasSeconds === hasMinutes) {
    return {
      errors: ["Provide exactly one of sleep_seconds or sleep_minutes"]
    };
  }

  var seconds = hasSeconds ? data.sleep_seconds : data.sleep_minutes * 60;
  if (typeof seconds !== "number" || !isFinite(seconds) ||
      Math.floor(seconds) !== seconds || seconds < 10 || seconds > 86400) {
    return { errors: ["Sleep interval must be an integer from 10 to 86400 seconds"] };
  }

  var bytes = [0x01];
  if (Object.prototype.hasOwnProperty.call(data, "command_id")) {
    var id = data.command_id;
    if (typeof id !== "number" || !isFinite(id) || Math.floor(id) !== id ||
        id < 1 || id > 65534)
      return { errors: ["command_id must be an integer from 1 to 65534"] };
    bytes.push((id >>> 8) & 0xff, id & 0xff);
  }
  bytes.push((seconds >>> 24) & 0xff, (seconds >>> 16) & 0xff,
    (seconds >>> 8) & 0xff, seconds & 0xff);
  return { fPort: 10, bytes: bytes };
}
