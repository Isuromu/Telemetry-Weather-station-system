'use strict';

// Watering-plan scheduling policy, ported from tools/watering_plan_scheduler.py
// and fixed by docs/integrations/watering-plan/api-protocol.rules.json.
//
// It lives beside the store rather than in Node-RED function nodes for two
// reasons: the policy needs `process` for a monotonic anchor (the schema's
// monotonic_start_ms is a process-relative value, and function bodies have no
// process), and one tick must be a single store round trip, because a flow that
// reads the store, decides, and writes it again closes a function-node cycle
// that every flow test rejects.
//
// The class owns no transport. It returns a dispatch decision
// ({kind:'start'|'stop', ...}) that the flow's result node hands to CONTROL.

const FAILED_STOP_REASONS = [
  'meter_unavailable', 'meter_total_regressed', 'monotonic_clock_regressed',
  'hard_runtime_limit', 'unsupported_volume_target'
];
// Phases that mean the sequencer is done with the run. 'stopping' is NOT one:
// the teardown is still in progress and the run is only complete once the
// sequencer reports idle/stopped/complete.
const TERMINAL_PHASES = ['idle', 'stopped', 'complete', 'completed'];
const FAULT_PHASES = ['fault', 'error'];
const ACTIVE_ATTEMPT_STATES = ['starting', 'running', 'stopping', 'stop_requested'];

function nowIso() { return new Date().toISOString().replace(/\.\d{3}Z$/, 'Z'); }
function monotonicNow() { return Number(process.hrtime.bigint() / 1000000n); }

class PlanSchedulerError extends Error {
  constructor(code, message, details) { super(message); this.code = code; this.details = details || []; }
}

class WateringPlanScheduler {
  constructor(options) {
    this.store = options.store;
    this.clock = options.clock || nowIso;
    this.monotonicMs = options.monotonicMs || monotonicNow;
    this.ownerId = options.ownerId || 'scheduler';
    // Minted once per process: a redeploy keeps it, a service restart changes it.
    this.bootId = options.bootId || (options.mintBootId ? options.mintBootId() : String(Date.now()) + '-' + Math.random().toString(36).slice(2, 10));
    this.seenBootId = null;
  }

  durationTargetMs(plan) {
    const units = { second: 1000, seconds: 1000, minute: 60000, minutes: 60000, hour: 3600000, hours: 3600000 };
    const factor = units[String(plan.stop_unit)];
    if (!factor) throw new PlanSchedulerError('unsupported_duration_unit', 'Stop unit is not configured for scheduling', [plan.stop_unit]);
    return Number(plan.stop_value) * factor;
  }

  volumeTargetM3(plan) {
    const units = { m3: 1, liter: 0.001, liters: 0.001, litre: 0.001, litres: 0.001 };
    const factor = units[String(plan.stop_unit)];
    if (!factor) throw new PlanSchedulerError('unsupported_volume_unit', 'Stop unit is not configured for scheduling', [plan.stop_unit]);
    return Number(plan.stop_value) * factor;
  }

  // Only fresh, commissioned valve1 positive-total evidence counts as a meter.
  // A static JSON value has no sample time and must not pose as live evidence.
  meterTotal(plan, facts) {
    const valves = plan.resolved_valves || [];
    if (valves.length !== 1 || valves[0] !== 'valve1') throw new PlanSchedulerError('unsupported_volume_target', 'Volume runs require the commissioned metered valve', valves);
    const status = (facts.meter_status || {})['valve1'];
    if (!status || status.healthy !== true || status.fresh !== true || status.decoder !== 'commissioned') throw new PlanSchedulerError('meter_unavailable', 'Commissioned meter evidence is unavailable', ['valve1']);
    const total = status.positive_total_m3;
    if (typeof total !== 'number' || !Number.isFinite(total) || total < 0) throw new PlanSchedulerError('meter_unavailable', 'Meter positive total is invalid', ['valve1']);
    const maxAgeMs = Number((facts.config || {}).meter_max_age_sec || 60) * 1000;
    const sampled = typeof status.sampled_ms === 'number' ? status.sampled_ms : null;
    if (sampled === null || this.clockMs(facts) - sampled > maxAgeMs) throw new PlanSchedulerError('meter_unavailable', 'Meter total is not fresh', ['valve1']);
    return total;
  }

  clockMs(facts) { return typeof facts.now_ms === 'number' ? facts.now_ms : Date.parse(this.clock()); }

  gateReason(plan, facts) {
    const state = facts.state || {};
    const config = facts.config || {};
    if (!state.present) return 'executor_gate_failed';
    if (state.age_ms === null || state.age_ms === undefined || state.age_ms > Number(config.max_state_age_sec || 15) * 1000) return 'executor_gate_failed';
    if (String(state.phase || 'idle') !== 'idle') return 'pump_run_active';
    const key = (plan.resolved_valves || []).join(',');
    const sets = state.ready_sets || {};
    if (!(key in sets)) return 'executor_gate_failed';
    if (String(sets[key] || '') !== '') return 'executor_gate_failed';
    if (plan.stop_type === 'volume') {
      try { this.meterTotal(plan, facts); } catch (error) { return error.code || 'meter_unavailable'; }
    } else {
      try { this.durationTargetMs(plan); } catch (error) { return error.code || 'unsupported_duration_unit'; }
    }
    return '';
  }

  // A restart must never resume a run, and the only command it may send is one
  // ordered stop — and only when a freshly observed sequencer is not already at
  // rest. A stale snapshot means we do not know what the field is doing, so the
  // safe action is none; the plan stays recovery_required for the operator.
  restartSafeStop(facts) {
    const state = facts.state || {};
    if (state.present !== true) return false;
    const maxAgeMs = Number((facts.config || {}).max_state_age_sec || 15) * 1000;
    if (state.age_ms === null || state.age_ms === undefined || state.age_ms > maxAgeMs) return false;
    return !TERMINAL_PHASES.includes(String(state.phase || ''));
  }

  async startupReconcile(facts) {
    if (facts.boot_id && facts.boot_id === this.seenBootId) return { recovered: 0, restart: false };
    this.seenBootId = facts.boot_id || this.bootId;
    const recovered = await this.store.recover(this.clock());
    return { recovered: recovered.recovered || 0, restart: true };
  }

  async stopForReason(plan, attempt, reason, facts, detail) {
    await this.store.transitionAttempt(attempt.attempt_id, 'stopping', this.clock(), reason, (facts.state || {}).phase, (facts.state || {}).notice,
      detail && detail.actualRuntimeMs !== undefined ? detail.actualRuntimeMs : undefined,
      detail && detail.deliveredM3 !== undefined ? detail.deliveredM3 : undefined);
    return {
      action: reason + '_stop_dispatched', reason,
      dispatch: { kind: 'stop', plan_id: plan.plan_id, plan_revision: plan.revision, operation_id: String(attempt.operation_id || '') }
    };
  }

  async markFailed(plan, attempt, reason, facts, detail) {
    const updated = await this.store.transitionAttempt(attempt.attempt_id, 'failed', this.clock(), reason, (facts.state || {}).phase, (facts.state || {}).notice,
      detail && detail.actualRuntimeMs !== undefined ? detail.actualRuntimeMs : undefined,
      detail && detail.deliveredM3 !== undefined ? detail.deliveredM3 : undefined);
    return { action: 'failed', reason, plan: updated };
  }

  async trackTarget(plan, attempt, facts) {
    const startedAt = attempt.monotonic_start_ms;
    if (typeof startedAt !== 'number') return this.markFailed(plan, attempt, 'tracking_start_missing', facts);
    const elapsedMs = this.monotonicMs() - startedAt;
    if (elapsedMs < 0) return this.stopForReason(plan, attempt, 'monotonic_clock_regressed', facts, { actualRuntimeMs: 0 });
    const acceptedLimitMs = Number(plan.accepted_max_runtime_sec) * 1000;
    if (plan.stop_type === 'volume') {
      let total;
      try { total = this.meterTotal(plan, facts); } catch (error) { return this.stopForReason(plan, attempt, error.code || 'meter_unavailable', facts, { actualRuntimeMs: elapsedMs }); }
      const baseline = attempt.meter_baseline_m3;
      if (typeof baseline !== 'number') return this.markFailed(plan, attempt, 'meter_baseline_missing', facts, { actualRuntimeMs: elapsedMs });
      const delivered = total - baseline;
      if (delivered < 0) return this.stopForReason(plan, attempt, 'meter_total_regressed', facts, { actualRuntimeMs: elapsedMs, deliveredM3: delivered });
      await this.store.recordAttemptProgress(attempt.attempt_id, elapsedMs, delivered);
      if (delivered >= this.volumeTargetM3(plan) || Math.abs(delivered - this.volumeTargetM3(plan)) <= 1e-12) return this.stopForReason(plan, attempt, 'volume_target_reached', facts, { actualRuntimeMs: elapsedMs, deliveredM3: delivered });
      if (elapsedMs >= acceptedLimitMs) return this.stopForReason(plan, attempt, 'hard_runtime_limit', facts, { actualRuntimeMs: elapsedMs, deliveredM3: delivered });
      return { action: 'observed' };
    }
    await this.store.recordAttemptProgress(attempt.attempt_id, elapsedMs);
    if (elapsedMs >= this.durationTargetMs(plan)) return this.stopForReason(plan, attempt, 'duration_target_reached', facts, { actualRuntimeMs: elapsedMs });
    if (elapsedMs >= acceptedLimitMs) return this.stopForReason(plan, attempt, 'hard_runtime_limit', facts, { actualRuntimeMs: elapsedMs });
    return { action: 'observed' };
  }

  async reconcileActive(plan, facts) {
    const attempt = plan.attempt;
    if (!attempt) return { action: 'recovery_required', reason: 'start_outcome_uncertain' };
    const phase = String((facts.state || {}).phase || '');
    if (plan.state === 'stop_requested') return this.stopForReason(plan, attempt, 'cancel_stop_dispatched', facts);
    if (FAULT_PHASES.includes(phase)) return this.markFailed(plan, attempt, 'sequencer_fault', facts);
    if (TERMINAL_PHASES.includes(phase)) {
      // A start that never reached running and was aborted has an uncertain
      // outcome unless CONTROL recorded a stop reason for it.
      if (plan.state === 'starting') {
        if ((facts.state || {}).stop_reason) return this.markFailed(plan, attempt, 'sequencer_fault', facts);
        // Recorded, not merely reported: recovery_required is also what stops
        // the next tick from claiming again behind an unresolved run.
        const parked = await this.store.transitionAttempt(attempt.attempt_id, 'recovery_required', this.clock(), 'start_outcome_uncertain', phase, (facts.state || {}).notice);
        return { action: 'recovery_required', reason: 'start_outcome_uncertain', plan: parked };
      }
      if (FAILED_STOP_REASONS.includes(attempt.final_reason)) return this.markFailed(plan, attempt, attempt.final_reason, facts);
      const reason = attempt.final_reason === 'cancel_stop_dispatched' ? 'cancelled' : 'sequence_completed';
      const updated = await this.store.transitionAttempt(attempt.attempt_id, 'completed', this.clock(), reason, phase, (facts.state || {}).notice);
      return { action: 'completed', reason, plan: updated };
    }
    if (phase === 'running') {
      if (plan.state === 'starting') {
        const owner = (facts.state || {}).plan_operation;
        if (owner && attempt.operation_id && owner !== attempt.operation_id) return { action: 'recovery_required', reason: 'start_outcome_uncertain', plan };
        await this.store.startAttemptTracking(attempt.attempt_id, this.monotonicMs(), this.clock());
        // No reason here: final_reason is COALESCEd, so it must be left free for
        // the stop or failure reason that ends the run.
        const running = await this.store.transitionAttempt(attempt.attempt_id, 'running', this.clock(), undefined, phase);
        return { action: 'running', plan: running };
      }
      if (plan.state === 'running') return this.trackTarget(plan, attempt, facts);
    }
    return { action: 'observed' };
  }

  // One tick. Order matters and mirrors the reference policy: boot reconcile,
  // clock gate, expiry, then a single active run, then at most one claim.
  async tick(facts) {
    if (!facts || facts.enabled !== true) return { action: 'disabled' };
    const now = this.clock();
    const boot = await this.startupReconcile(facts);
    if (boot.recovered > 0 && this.restartSafeStop(facts)) {
      return { action: 'restart_safe_stop', dispatch: { kind: 'stop', plan_id: null, operation_id: 'recovery:' + this.bootId } };
    }
    if (facts.clock_synchronized !== true) {
      // An untrusted clock cannot establish due-ness or expiry: a forward jump
      // would otherwise expire every pending plan.
      const active = await this.store.activePlan();
      if (active) return this.reconcileActive(active, facts);
      return { action: 'clock_unsynchronized' };
    }
    await this.store.expireDue(now);
    const active = await this.store.activePlan();
    if (active) return this.reconcileActive(active, facts);
    const due = await this.store.nextDue(now);
    if (!due) return { action: 'idle' };
    const reason = this.gateReason(due, facts);
    if (reason !== '') {
      const delayed = await this.store.markDelayed(due.plan_id, reason, now, (facts.config || {}).delay_retry_sec);
      return { action: 'delayed', reason, plan: delayed };
    }
    const claimed = await this.store.claim(now, this.ownerId);
    if (!claimed) return { action: 'idle' };
    if (claimed.stop_type === 'volume') {
      // Capture the commissioned total before dispatch so a restart cannot
      // derive a duration from wall-clock time.
      try {
        const baseline = this.meterTotal(claimed, facts);
        await this.store.setMeterBaseline(claimed.attempt.attempt_id, baseline, now);
      } catch (error) {
        const released = await this.store.releaseClaim(claimed.plan_id, error.code || 'meter_unavailable', now);
        return { action: 'delayed', reason: error.code || 'meter_unavailable', plan: released };
      }
    }
    const valves = claimed.resolved_valves || [];
    return {
      action: 'started', phase: 'starting', plan: claimed,
      dispatch: { kind: 'start', plan_id: claimed.plan_id, plan_revision: claimed.revision, operation_id: claimed.attempt.operation_id, valve1: valves.indexOf('valve1') !== -1, valve2: valves.indexOf('valve2') !== -1 }
    };
  }
}

module.exports = { WateringPlanScheduler, PlanSchedulerError, FAILED_STOP_REASONS, ACTIVE_ATTEMPT_STATES };
