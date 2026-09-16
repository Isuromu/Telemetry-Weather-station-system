/* ChirpStack v4 codec for the MainValve Class C example.
 * Downlink FPort 30: protocol-v1 target-angle command.
 * Status uplink FPort 31: protocol-v2 actuator, pressure, and command phase.
 */

function readU16(bytes, offset) {
  return ((bytes[offset] << 8) | bytes[offset + 1]) >>> 0;
}

function decodeUplink(input) {
  if (input.fPort !== 31) {
    return { errors: ["MainValve status must use FPort 31"] };
  }
  if (!input.bytes ||
      !((input.bytes[0] === 1 && input.bytes.length === 15) ||
        (input.bytes[0] === 2 && input.bytes.length === 18))) {
    return { errors: ["MainValve status must be protocol v1/15 bytes or v2/18 bytes"] };
  }

  var b = input.bytes;
  var flags = b[1];
  var actuatorOnline = !!(flags & 0x01);
  var pressureValid = !!(flags & 0x02);
  var reasons = {
    0: "startup", 1: "local_command", 2: "remote_command",
    3: "duplicate_command", 4: "invalid_command", 5: "modbus_error",
    6: "pressure_interlock", 7: "pressure_sensor_error",
    8: "actuator_busy", 9: "movement_timeout", 10: "actuator_fault",
    11: "local_override"
  };
  var phases = {
    0: "none", 1: "accepted", 2: "moving", 3: "finished",
    4: "rejected", 5: "failed"
  };
  var actual10 = readU16(b, 3);
  var target10 = readU16(b, 5);
  var pressure100 = readU16(b, 7);
  var commandId = readU16(b, 11);
  var warnings = [];
  if (flags & 0x80) {
    warnings.push("Unknown MainValve status flag bits are set");
  }

  var data = {
    actuator_online: actuatorOnline,
    pressure_valid: pressureValid,
    rs485_bus_mode: (flags & 0x04) !== 0,
    valve_moving: (flags & 0x08) !== 0,
    overpressure: (flags & 0x10) !== 0,
    lorawan_active: (flags & 0x20) !== 0,
    class_c_active: (flags & 0x40) !== 0,
    reason_code: b[2],
    reason: reasons[b[2]] || "unknown",
    actual_angle_deg: actuatorOnline && actual10 !== 0xFFFF ? actual10 / 10.0 : null,
    target_angle_deg: actuatorOnline && target10 !== 0xFFFF ? target10 / 10.0 : null,
    pressure_bar: pressureValid && pressure100 !== 0xFFFF ? pressure100 / 100.0 : null,
    pressure_temperature_c: pressureValid ? b[14] - 40 : null,
    actuator_fault_code: actuatorOnline ? readU16(b, 9) : null,
    last_command_id: commandId !== 0xFFFF ? commandId : null,
    actuator_mode: b[13] === 1 ? "rs485_bus" :
      (b[13] === 0 ? "analog" : "unavailable"),
    protocol_version: b[0],
    reported_command_id: b[0] === 2 && readU16(b, 15) !== 0xFFFF ?
      readU16(b, 15) : null,
    command_phase: b[0] === 2 ? (phases[b[17]] || "unknown") : "unavailable"
  };
  return { data: data, warnings: warnings };
}

function encodeDownlink(input) {
  var data = input.data;
  if (!data || typeof data !== "object") {
    return { errors: ["Downlink data must be an object"] };
  }
  if (typeof data.angle_deg !== "number" ||
      !isFinite(data.angle_deg) ||
      data.angle_deg < 0 || data.angle_deg > 90) {
    return { errors: ["angle_deg must be a number from 0 to 90"] };
  }
  if (typeof data.command_id !== "number" || !isFinite(data.command_id) ||
      Math.floor(data.command_id) !== data.command_id ||
      data.command_id < 0 || data.command_id > 65534) {
    // 0xFFFF is reserved by the firmware as its "no previous command" value.
    return { errors: ["command_id must be an integer from 0 to 65534"] };
  }
  var angle10 = Math.round(data.angle_deg * 10);
  var commandId = data.command_id;
  return {
    fPort: 30,
    bytes: [1, 1, (angle10 >> 8) & 0xFF, angle10 & 0xFF,
      (commandId >> 8) & 0xFF, commandId & 0xFF]
  };
}
