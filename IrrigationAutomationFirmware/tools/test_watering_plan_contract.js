const assert = require('assert');
const { canonicalDigest, validatePlan } = require('./watering_plan_contract.js');

const config = {
  clockSynchronized: true,
  gatewayMaxRuntimeSec: 14400,
  durationUnits: ['sec'],
  volumeUnits: ['m3'],
  fields: [
    { field_id: 'field-001', valve_id: 'valve1', flow_meter: 'commissioned', enabled: true },
    { field_id: 'field-002', valve_id: 'valve2', flow_meter: 'simulated_meter', enabled: true },
  ],
  // This test fixture permits valve2 alone so the volume-capability assertion
  // reaches the meter check. It does not commission valve2 for deployment.
  allowedValveSets: [['valve1'], ['valve2'], ['valve1', 'valve2']],
  meterStatusByValve: {
    valve1: { healthy: true, fresh: true, decoder: 'commissioned' },
  },
};

const plan = {
  schema_version: 1,
  plan_id: 'mobile-plan-001',
  revision: 1,
  status: 'scheduled',
  source: 'ai',
  plan_group_id: 'recommendation-2026-10-12',
  summary: 'Approved duration plan',
  created_at: '2026-10-11T10:00:00+05:00',
  timezone: 'Asia/Tashkent',
  start_at: '2026-10-12T06:00:00+05:00',
  valid_until: '2026-10-12T16:00:00+05:00',
  targets: { mode: 'selected', field_ids: ['field-001'] },
  run_mode: 'parallel',
  stop: { type: 'duration', value: 900, unit: 'sec' },
  hard_max_runtime_sec: 18000,
};

const clone = value => JSON.parse(JSON.stringify(value));
const accepted = validatePlan(plan, { ...config, pathPlanId: plan.plan_id });
assert.equal(accepted.ok, true, JSON.stringify(accepted));
assert.deepEqual(accepted.value.resolvedFieldIds, ['field-001']);
assert.deepEqual(accepted.value.resolvedValveIds, ['valve1']);
assert.equal(accepted.value.acceptedMaxRuntimeSec, 14400);
assert.match(accepted.value.payloadSha256, /^[a-f0-9]{64}$/);

const volume = clone(plan);
volume.plan_id = 'mobile-volume-001';
volume.stop = { type: 'volume', value: 40, unit: 'm3' };
const acceptedVolume = validatePlan(volume, { ...config, pathPlanId: volume.plan_id });
assert.equal(acceptedVolume.ok, true, JSON.stringify(acceptedVolume));

const unmeteredVolume = clone(volume);
unmeteredVolume.targets.field_ids = ['field-002'];
const unmeteredResult = validatePlan(unmeteredVolume, { ...config, pathPlanId: unmeteredVolume.plan_id });
assert.equal(unmeteredResult.ok, false);
assert.equal(unmeteredResult.error.code, 'unsupported_volume_target');

const noOffset = clone(plan);
noOffset.start_at = '2026-10-12T06:00:00';
const noOffsetResult = validatePlan(noOffset, { ...config, pathPlanId: noOffset.plan_id });
assert.equal(noOffsetResult.ok, false);
assert.equal(noOffsetResult.error.code, 'invalid_timestamp');

const unsynchronized = validatePlan(plan, { ...config, pathPlanId: plan.plan_id, clockSynchronized: false });
assert.equal(unsynchronized.ok, false);
assert.equal(unsynchronized.error.code, 'clock_unsynchronized');

const allFields = clone(plan);
allFields.plan_id = 'mobile-all-001';
allFields.targets = { mode: 'all' };
const allFieldsResult = validatePlan(allFields, { ...config, pathPlanId: allFields.plan_id });
assert.equal(allFieldsResult.ok, true, JSON.stringify(allFieldsResult));
assert.deepEqual(allFieldsResult.value.resolvedFieldIds, ['field-001', 'field-002']);
assert.deepEqual(allFieldsResult.value.resolvedValveIds, ['valve1', 'valve2']);

const reordered = clone(allFields);
reordered.targets = { mode: 'selected', field_ids: ['field-002', 'field-001'] };
const ordered = clone(allFields);
ordered.targets = { mode: 'selected', field_ids: ['field-001', 'field-002'] };
assert.deepEqual(canonicalDigest(reordered), canonicalDigest(ordered));

console.log('Watering-plan contract tests passed');
