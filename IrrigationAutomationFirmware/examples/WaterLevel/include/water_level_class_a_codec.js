// ChirpStack codec for the WaterLevel Class A node.
// Uplink FPort 40: 10-byte telemetry protocol v1. No application downlink is
// currently supported.

function readU16(bytes, offset) {
  return (bytes[offset] << 8) | bytes[offset + 1];
}

function readI16(bytes, offset) {
  var value = readU16(bytes, offset);
  return value >= 0x8000 ? value - 0x10000 : value;
}

function decodeUplink(input) {
  if (input.fPort !== 40)
    return { errors: ["WaterLevel telemetry must use FPort 40"] };
  if (!input.bytes || input.bytes.length !== 10)
    return { errors: ["WaterLevel telemetry v1 must contain 10 bytes"] };
  if (input.bytes[0] !== 1)
    return { errors: ["Unsupported WaterLevel protocol version"] };

  var flags = input.bytes[1];
  var pressureValid = !!(flags & 0x01);
  return { data: {
    runtime_mode: "water_level_class_a",
    protocol_version: input.bytes[0],
    pressure_valid: pressureValid,
    load_on: !!(flags & 0x02),
    battery_voltage_v: readU16(input.bytes, 2) / 1000.0,
    pressure_bar: pressureValid ? readI16(input.bytes, 4) / 1000.0 : null,
    depth_m: pressureValid ? readU16(input.bytes, 6) / 1000.0 : null,
    water_level_percent: pressureValid
      ? readU16(input.bytes, 8) / 10.0
      : null
  } };
}

function encodeDownlink(input) {
  return {
    errors: [
      "WaterLevel is telemetry-only; application downlinks are disabled"
    ]
  };
}
