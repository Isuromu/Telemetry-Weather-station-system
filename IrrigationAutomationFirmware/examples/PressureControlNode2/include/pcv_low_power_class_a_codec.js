// ChirpStack codec for pcv_low_power_class_a.
// Uplink FPort 31: 32-byte status. Downlink FPort 30: 10-byte command.

function readU16(b, i) { return (b[i] << 8) | b[i + 1]; }
function readI16(b, i) {
  var v = readU16(b, i);
  return v >= 0x8000 ? v - 0x10000 : v;
}
function readU32(b, i) {
  return (((b[i] << 24) >>> 0) | (b[i + 1] << 16) |
          (b[i + 2] << 8) | b[i + 3]) >>> 0;
}
function readI32(b, i) {
  return (b[i] << 24) | (b[i + 1] << 16) |
         (b[i + 2] << 8) | b[i + 3];
}
function putU16(b, i, v) {
  b[i] = (v >>> 8) & 0xff;
  b[i + 1] = v & 0xff;
}
function putU32(b, i, v) {
  b[i] = (v >>> 24) & 0xff;
  b[i + 1] = (v >>> 16) & 0xff;
  b[i + 2] = (v >>> 8) & 0xff;
  b[i + 3] = v & 0xff;
}

function decodeUplink(input) {
  if (input.fPort !== 31)
    return { errors: ["pcv_low_power_class_a status must use FPort 31"] };
  if (!input.bytes || input.bytes.length !== 32)
    return { errors: ["PCV protocol v2 status must contain 32 bytes"] };
  if (input.bytes[0] !== 2)
    return { errors: ["unsupported PCV protocol version"] };

  var b = input.bytes;
  var flags = b[1];
  var pcvStateNames = ["unknown", "open", "closed"];
  var reasonNames = [
    "startup", "periodic_wake", "remote_open_applied",
    "remote_close_applied", "sleep_interval_applied",
    "pcv_and_sleep_interval_applied", "duplicate_command_ignored",
    "invalid_command_rejected", "pcv_actuation_failed", "lorawan_error",
    "local_command_applied", "no_op_command_accepted",
    "flow_total_reset_applied", "flow_total_reset_failed"
  ];
  var velocity = readI16(b, 18);
  var sleepSeconds = readU32(b, 22);
  var lastCommandId = readU16(b, 26);
  var totalLiters = readU32(b, 28);

  return { data: {
    runtime_mode: "pcv_low_power_class_a",
    protocol_version: b[0],
    status_reason: reasonNames[b[3]] || "unknown",
    pcv_last_commanded: pcvStateNames[b[2]] || "unknown",
    pcv_position_verified: false,
    upstream_pressure_bar: (flags & 0x01) ? readI16(b, 4) / 100.0 : null,
    upstream_temperature_c: (flags & 0x01) ? readI16(b, 6) / 100.0 : null,
    upstream_scale_validated: !!(flags & 0x10),
    downstream_pressure_bar: (flags & 0x02) ? readI16(b, 8) / 100.0 : null,
    downstream_temperature_c: (flags & 0x02) ? readI16(b, 10) / 100.0 : null,
    downstream_scale_validated: !!(flags & 0x20),
    battery_voltage_v: (flags & 0x04) ? readU16(b, 12) / 1000.0 : null,
    battery_soc_percent: null,
    flow_rate_m3_h: (flags & 0x08) ? readI32(b, 14) / 1000.0 : null,
    water_velocity_m_s: (flags & 0x08) && velocity !== -32768
      ? velocity / 1000.0 : null,
    tuf_error_bits: (flags & 0x80) ? readU16(b, 20) : null,
    sleep_seconds: sleepSeconds,
    sleep_minutes: sleepSeconds / 60.0,
    last_command_id: lastCommandId === 0xffff ? null : lastCommandId,
    flow_total_since_reset_m3: totalLiters === 0xffffffff
      ? null : totalLiters / 1000.0,
    flow_total_since_reset_liters: totalLiters === 0xffffffff
      ? null : totalLiters
  } };
}

function encodeDownlink(input) {
  var data = input.data || {};
  var flags = 0;
  var action = 0;
  if (data.valve !== undefined)
    return { errors: ["use pcv, not valve; valve refers to the separate Main Valve"] };
  if (data.pcv !== undefined && data.pcv !== null) {
    var pcv = String(data.pcv).toLowerCase();
    if (pcv !== "open" && pcv !== "close" && pcv !== "none")
      return { errors: ["pcv must be open, close, none, or omitted"] };
    flags |= 0x01;
    action = pcv === "open" ? 1 : (pcv === "close" ? 2 : 0);
  }

  var hasMinutes = data.sleep_minutes !== undefined;
  var hasSeconds = data.sleep_seconds !== undefined;
  if (hasMinutes && hasSeconds)
    return { errors: ["use sleep_minutes or sleep_seconds, not both"] };
  var sleepSeconds = 0;
  if (hasMinutes) {
    if (typeof data.sleep_minutes !== "number" ||
        !isFinite(data.sleep_minutes))
      return { errors: ["sleep_minutes must be numeric"] };
    sleepSeconds = Math.round(data.sleep_minutes * 60);
    flags |= 0x02;
  } else if (hasSeconds) {
    if (!Number.isInteger(data.sleep_seconds))
      return { errors: ["sleep_seconds must be an integer"] };
    sleepSeconds = data.sleep_seconds;
    flags |= 0x02;
  }
  // Ten seconds is enabled only for the valve_1 commissioning firmware.
  // Restore this lower bound to 60 together with the PlatformIO build flags
  // before field deployment.
  if ((flags & 0x02) && (sleepSeconds < 10 || sleepSeconds > 86400))
    return { errors: ["sleep interval must be 10..86400 seconds"] };
  if (data.flow_total_reset !== undefined) {
    if (data.flow_total_reset !== true)
      return { errors: ["flow_total_reset must be true or omitted"] };
    flags |= 0x04;
  }
  if (flags === 0)
    return { errors: ["set the PCV action, sleep interval, flow_total_reset, or a combination"] };
  if (!Number.isInteger(data.command_id) ||
      data.command_id < 0 || data.command_id > 65535)
    return { errors: ["command_id must be an integer from 0 to 65535"] };

  var bytes = [2, flags, action, 0, 0, 0, 0, 0, 0, 0];
  putU32(bytes, 4, sleepSeconds >>> 0);
  putU16(bytes, 8, data.command_id);
  return { fPort: 30, bytes: bytes };
}
