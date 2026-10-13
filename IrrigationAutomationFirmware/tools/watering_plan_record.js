// Projection from the durable store row to the mobile WateringPlanRecord.
//
// This mirrors WateringPlanService._public_plan in tools/watering_plan_api.py so
// the Python prototype and the generated Node-RED flow serve the same record
// shape: the accepted plan payload flattened at the top level, the store's
// current state and timestamps merged in, and at most one execution summary.
// The builder inlines this file verbatim into the mobile API result function,
// so keep it dependency-free.
'use strict';

function publicPlan(stored) {
  if (!stored || typeof stored !== 'object') return stored;
  let plan = stored.payload;
  if (typeof plan === 'string') {
    try { plan = JSON.parse(plan); } catch (_) { plan = {}; }
  }
  if (!plan || typeof plan !== 'object' || Array.isArray(plan)) plan = {};
  const record = Object.assign({}, plan);
  record.state = stored.state;
  record.accepted_at = stored.accepted_at;
  record.updated_at = stored.updated_at;
  if (stored.last_delay_reason) record.delay_reason = stored.last_delay_reason;
  const attempts = stored.attempts || [];
  const attempt = stored.attempt || (attempts.length ? attempts[attempts.length - 1] : null);
  if (attempt) {
    const execution = {};
    if (attempt.operation_id !== undefined && attempt.operation_id !== null) execution.operation_id = attempt.operation_id;
    if (attempt.actual_started_at !== undefined && attempt.actual_started_at !== null) execution.actual_started_at = attempt.actual_started_at;
    if (attempt.actual_ended_at !== undefined && attempt.actual_ended_at !== null) execution.actual_ended_at = attempt.actual_ended_at;
    if (attempt.delivered_m3 !== undefined && attempt.delivered_m3 !== null) execution.actual_volume_m3 = attempt.delivered_m3;
    if (attempt.final_reason !== undefined && attempt.final_reason !== null) execution.stop_reason = attempt.final_reason;
    record.execution = execution;
  }
  if ('events' in stored) record.events = stored.events;
  for (const key of ['created', 'replaced', 'idempotent']) {
    if (key in stored) record[key] = stored[key];
  }
  return record;
}

module.exports = { publicPlan };
