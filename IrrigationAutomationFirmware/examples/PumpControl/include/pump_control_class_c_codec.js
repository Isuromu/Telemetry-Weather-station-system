/* ChirpStack v4 codec for PumpControl protocol version 1. */

function readU16(bytes, offset) {
  return ((bytes[offset] << 8) | bytes[offset + 1]) >>> 0;
}

function decodeUplink(input) {
  if (input.fPort !== 51 || !input.bytes || input.bytes.length !== 17 ||
      input.bytes[0] !== 1) {
    return { errors: ["Expected PumpControl v1 status: FPort 51, 17 bytes"] };
  }
  var b = input.bytes;
  var f = b[1];
  var results = {
    0: "none", 1: "accepted", 2: "failed", 3: "duplicate",
    4: "invalid", 5: "storage_error"
  };
  var states = { 0: "unknown", 1: "forward", 2: "reverse", 3: "stopped" };
  var id = readU16(b, 3);
  return { data: {
    communication_ok: !!(f & 1),
    configuration_valid: !!(f & 2),
    running: !!(f & 4),
    frequency_armed: !!(f & 8),
    lorawan_active: !!(f & 16),
    class_c_active: !!(f & 32),
    command_result: results[b[2]] || "unknown",
    last_command_id: id === 65535 ? null : id,
    commanded_frequency_hz: readU16(b, 5) / 100,
    actual_frequency_hz: readU16(b, 7) / 100,
    motor_current_a: readU16(b, 9) / 100,
    vfd_fault_code: readU16(b, 11),
    output_voltage_v: readU16(b, 13) / 10,
    run_state: states[b[15]] || "unknown",
    communication_error_code: b[16]
  } };
}

function encodeDownlink(input) {
  var d = input.data || {};
  var operations = { stop: 1, estop: 2, set_frequency: 3, start: 4 };
  var op = operations[d.command];
  var id = d.command_id;
  if (!op || !Number.isInteger(id) || id < 0 || id > 65534) {
    return { errors: ["Use command stop, estop, set_frequency, or start and command_id 0..65534"] };
  }
  var arg = 0;
  if (op === 3) {
    if (typeof d.frequency_hz !== "number" || !Number.isFinite(d.frequency_hz) ||
        d.frequency_hz < 10 || d.frequency_hz > 50) {
      return { errors: ["frequency_hz must be 10..50"] };
    }
    arg = Math.round(d.frequency_hz * 100);
  }
  return { fPort: 50, bytes: [1, op, id >> 8, id & 255, arg >> 8, arg & 255] };
}
