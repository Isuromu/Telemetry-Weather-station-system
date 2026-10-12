// Shared validation and canonicalization for watering-plan API adapters.
// Keep this dependency-free so a Node-RED function/node adapter and future
// local server implement the same version-1 plan acceptance rules.

const crypto = require('crypto');

const ISO_WITH_OFFSET = /^(\d{4})-(\d{2})-(\d{2})T(\d{2}):(\d{2}):(\d{2})(?:\.\d{1,9})?(?:Z|[+-](?:0\d|1\d|2[0-3]):[0-5]\d)$/;
const PLAN_KEYS = new Set([
  'schema_version', 'plan_id', 'revision', 'status', 'source', 'plan_group_id',
  'summary', 'created_at', 'timezone', 'start_at', 'valid_until', 'targets',
  'run_mode', 'stop', 'hard_max_runtime_sec',
]);
const TARGET_KEYS = new Set(['mode', 'field_ids']);
const STOP_KEYS = new Set(['type', 'value', 'unit']);

const fail = (code, message, details) => ({ ok: false, error: { code, message, details: details || [] } });
const isObject = value => value !== null && typeof value === 'object' && !Array.isArray(value);
const finitePositive = value => typeof value === 'number' && Number.isFinite(value) && value > 0;

function unexpectedKeys(object, allowed) {
  return Object.keys(object).filter(key => !allowed.has(key));
}

function parseTimestamp(value, field) {
  if (typeof value !== 'string') return fail('invalid_timestamp', field + ' must be an ISO 8601 string with an explicit offset');
  const match = ISO_WITH_OFFSET.exec(value);
  if (!match) return fail('invalid_timestamp', field + ' must include an explicit UTC offset');
  const year = Number(match[1]);
  const month = Number(match[2]);
  const day = Number(match[3]);
  const hour = Number(match[4]);
  const minute = Number(match[5]);
  const second = Number(match[6]);
  const calendar = new Date(Date.UTC(year, month - 1, day));
  if (month < 1 || month > 12 || day < 1 || calendar.getUTCFullYear() !== year ||
      calendar.getUTCMonth() !== month - 1 || calendar.getUTCDate() !== day ||
      hour > 23 || minute > 59 || second > 59) {
    return fail('invalid_timestamp', field + ' is not a valid calendar timestamp');
  }
  const milliseconds = Date.parse(value);
  if (!Number.isFinite(milliseconds)) return fail('invalid_timestamp', field + ' could not be parsed');
  return { ok: true, milliseconds };
}

function validTimeZone(value) {
  if (typeof value !== 'string' || !value) return false;
  try {
    new Intl.DateTimeFormat('en-US', { timeZone: value }).format();
    return true;
  } catch (_) {
    return false;
  }
}

function canonicalize(value) {
  if (value === null || typeof value === 'boolean' || typeof value === 'string') return value;
  if (typeof value === 'number') {
    if (!Number.isFinite(value)) throw new TypeError('Canonical JSON cannot contain a non-finite number');
    return value;
  }
  if (Array.isArray(value)) return value.map(canonicalize);
  if (!isObject(value)) throw new TypeError('Canonical JSON supports only JSON values');
  const output = {};
  for (const key of Object.keys(value).sort()) output[key] = canonicalize(value[key]);
  return output;
}

function canonicalJson(value) {
  return JSON.stringify(canonicalize(value));
}

function canonicalPayload(payload) {
  const copy = canonicalize(payload);
  if (copy.targets && Array.isArray(copy.targets.field_ids)) {
    copy.targets.field_ids.sort();
  }
  return JSON.stringify(copy);
}

function canonicalDigest(payload) {
  const text = canonicalPayload(payload);
  return { canonicalPayload: text, payloadSha256: crypto.createHash('sha256').update(text).digest('hex') };
}

function normalizeRegistry(fields) {
  if (!Array.isArray(fields)) return fail('gateway_configuration_invalid', 'Field registry is not configured');
  const byFieldId = new Map();
  for (const field of fields) {
    if (!isObject(field) || typeof field.field_id !== 'string' || !field.field_id ||
        typeof field.valve_id !== 'string' || !field.valve_id) {
      return fail('gateway_configuration_invalid', 'Field registry contains an invalid field mapping');
    }
    if (byFieldId.has(field.field_id)) {
      return fail('gateway_configuration_invalid', 'Field registry contains a duplicate field id', [field.field_id]);
    }
    byFieldId.set(field.field_id, field);
  }
  return { ok: true, byFieldId };
}

function resolveTargets(targets, registry) {
  if (!isObject(targets) || unexpectedKeys(targets, TARGET_KEYS).length) {
    return fail('invalid_targets', 'targets contains unsupported fields');
  }
  let fieldIds;
  if (targets.mode === 'selected') {
    if (!Array.isArray(targets.field_ids) || !targets.field_ids.length ||
        targets.field_ids.some(value => typeof value !== 'string' || !value)) {
      return fail('invalid_targets', 'Selected plans require one or more field_ids');
    }
    if (new Set(targets.field_ids).size !== targets.field_ids.length) {
      return fail('duplicate_target', 'Selected field_ids must be unique');
    }
    fieldIds = [...targets.field_ids].sort();
  } else if (targets.mode === 'all') {
    if (Object.prototype.hasOwnProperty.call(targets, 'field_ids')) {
      return fail('invalid_targets', 'All-fields plans must not carry field_ids');
    }
    fieldIds = [...registry.byFieldId.values()]
      .filter(field => field.enabled === true)
      .map(field => field.field_id)
      .sort();
  } else {
    return fail('invalid_targets', 'targets.mode must be selected or all');
  }
  if (!fieldIds.length) return fail('no_enabled_targets', 'No enabled fields match this plan');
  const fields = [];
  for (const fieldId of fieldIds) {
    const field = registry.byFieldId.get(fieldId);
    if (!field || field.enabled !== true) {
      return fail('unknown_or_disabled_field', 'Plan targets an unknown or disabled field', [fieldId]);
    }
    fields.push(field);
  }
  const valveIds = fields.map(field => field.valve_id);
  if (new Set(valveIds).size !== valveIds.length) {
    return fail('gateway_configuration_invalid', 'Field registry maps selected fields to the same valve');
  }
  return { ok: true, fieldIds, fields, valveIds: valveIds.sort() };
}

function allowedValveSet(valveIds, configuredSets) {
  if (!Array.isArray(configuredSets)) return false;
  const expected = valveIds.join('\u0000');
  return configuredSets.some(set => Array.isArray(set) && [...set].sort().join('\u0000') === expected);
}

function validatePlan(payload, options) {
  const config = options || {};
  if (!isObject(payload)) return fail('invalid_payload', 'Plan payload must be a JSON object');
  const unknown = unexpectedKeys(payload, PLAN_KEYS);
  if (unknown.length) return fail('invalid_payload', 'Plan payload contains unsupported fields', unknown.sort());
  if (payload.schema_version !== 1) return fail('unsupported_schema_version', 'Only schema_version 1 is supported');
  if (typeof payload.plan_id !== 'string' || !payload.plan_id) return fail('invalid_plan_id', 'plan_id is required');
  if (config.pathPlanId !== undefined && payload.plan_id !== config.pathPlanId) {
    return fail('plan_id_mismatch', 'URL plan_id does not match payload plan_id');
  }
  if (!Number.isInteger(payload.revision) || payload.revision < 1) return fail('invalid_revision', 'revision must be a positive integer');
  if (payload.status !== 'scheduled' && payload.status !== 'cancelled') return fail('invalid_status', 'status must be scheduled or cancelled');
  if (payload.source !== 'human' && payload.source !== 'ai') return fail('invalid_source', 'source must be human or ai');
  if (!validTimeZone(payload.timezone)) return fail('invalid_timezone', 'timezone must be a valid IANA timezone');
  for (const field of ['created_at', 'start_at', 'valid_until']) {
    const result = parseTimestamp(payload[field], field);
    if (!result.ok) return result;
  }
  const startAt = parseTimestamp(payload.start_at, 'start_at').milliseconds;
  const validUntil = parseTimestamp(payload.valid_until, 'valid_until').milliseconds;
  if (startAt >= validUntil) return fail('invalid_time_window', 'start_at must be before valid_until');
  if (config.clockSynchronized !== true) return fail('clock_unsynchronized', 'Gateway clock is not synchronized');
  if (payload.run_mode !== 'parallel') return fail('unsupported_run_mode', 'Only parallel run_mode is supported');
  if (!Number.isInteger(payload.hard_max_runtime_sec) || payload.hard_max_runtime_sec < 1) {
    return fail('invalid_hard_max_runtime', 'hard_max_runtime_sec must be a positive integer');
  }
  if (!Number.isInteger(config.gatewayMaxRuntimeSec) || config.gatewayMaxRuntimeSec < 1) {
    return fail('gateway_configuration_invalid', 'Gateway maximum run duration is not configured');
  }
  const registry = normalizeRegistry(config.fields);
  if (!registry.ok) return registry;
  const targets = resolveTargets(payload.targets, registry);
  if (!targets.ok) return targets;
  if (!allowedValveSet(targets.valveIds, config.allowedValveSets)) {
    return fail('parallel_capacity_not_commissioned', 'Selected valve set is not approved for this pump');
  }
  if (!isObject(payload.stop) || unexpectedKeys(payload.stop, STOP_KEYS).length ||
      !finitePositive(payload.stop.value) || typeof payload.stop.unit !== 'string' || !payload.stop.unit) {
    return fail('invalid_stop_rule', 'stop must contain a supported positive type, value, and unit');
  }
  if (payload.stop.type === 'duration') {
    if (!Array.isArray(config.durationUnits) || !config.durationUnits.includes(payload.stop.unit)) {
      return fail('unsupported_duration_unit', 'Duration unit is not configured');
    }
  } else if (payload.stop.type === 'volume') {
    if (!Array.isArray(config.volumeUnits) || !config.volumeUnits.includes(payload.stop.unit)) {
      return fail('unsupported_volume_unit', 'Volume unit is not configured');
    }
    // Phase 3 implements total-baseline completion only for the deployed
    // field-001 -> valve1 commissioning path. A future registry entry must not
    // turn simulated or newly labelled meter data into a production stop rule.
    if (targets.fields.length !== 1 || targets.valveIds[0] !== 'valve1' ||
        targets.fields[0].flow_meter !== 'commissioned') {
      return fail('unsupported_volume_target', 'Volume plans require commissioned valve1 metering');
    }
    const meter = config.meterStatusByValve && config.meterStatusByValve[targets.valveIds[0]];
    if (!meter || meter.healthy !== true || meter.fresh !== true || meter.decoder !== 'commissioned') {
      return fail('meter_unavailable', 'Commissioned flow-meter evidence is unavailable');
    }
  } else {
    return fail('unsupported_stop_type', 'Only duration and volume stop rules are supported');
  }
  const digest = canonicalDigest(payload);
  return {
    ok: true,
    value: {
      ...digest,
      plan: payload,
      resolvedFieldIds: targets.fieldIds,
      resolvedValveIds: targets.valveIds,
      acceptedMaxRuntimeSec: Math.min(payload.hard_max_runtime_sec, config.gatewayMaxRuntimeSec),
    },
  };
}

module.exports = { canonicalJson, canonicalDigest, validatePlan };
