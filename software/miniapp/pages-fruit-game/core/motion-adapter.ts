import type {
  MotionFrame,
  MotionQuality,
  MotionWarning,
  RepEvent,
  TrainingState
} from '../types/motion'

export type RejectReason =
  | 'invalid_json' | 'invalid_shape' | 'stale_seq' | 'timestamp_regression'
  | 'count_regression' | 'target_changed' | 'parameter_changed'
  | 'completion_regression' | 'overall_completion_regression'
  | 'invalid_set_transition' | 'invalid_rest_event'
  | 'invalid_state_transition' | 'after_terminal'

export type FrameDiagnosticCode = 'seq_gap' | 'timestamp_gap' | 'count_jump' | 'latency_drift'
export interface FrameDiagnostic { code: FrameDiagnosticCode; value: number }
export type AdaptResult =
  | { accepted: true; frame: MotionFrame; diagnostics: FrameDiagnostic[] }
  | { accepted: false; reason: RejectReason }
export interface MotionFrameAdapterOptions { gapDiagnosticMs?: number; latencyDriftMs?: number }

const TRAINING_STATES: TrainingState[] = ['IDLE', 'RUNNING', 'PAUSED', 'REST', 'FINISHED', 'STOPPED']
const REP_EVENTS: RepEvent[] = ['none', 'left_rep_done', 'right_rep_done', 'both_rep_done']
const QUALITY_VALUES: MotionQuality[] = ['READY', 'SIGNAL_LOST', 'REST', 'ROM_LOW', 'ASYMMETRY', 'GOOD']
const WARNING_VALUES: MotionWarning[] = ['none', 'imu_signal_lost', 'resting', 'range_too_small', 'left_right_asymmetry']

function isFiniteNumber(value: unknown): value is number { return typeof value === 'number' && Number.isFinite(value) }
function isNonNegativeInteger(value: unknown): value is number { return Number.isInteger(value) && Number(value) >= 0 }

export class MotionFrameAdapter {
  private lastFrame: MotionFrame | null = null
  private arrivalOffsetBaselineMs: number | null = null
  private readonly gapDiagnosticMs: number
  private readonly latencyDriftMs: number

  constructor(options: MotionFrameAdapterOptions = {}) {
    this.gapDiagnosticMs = options.gapDiagnosticMs ?? 1000
    this.latencyDriftMs = options.latencyDriftMs ?? 500
  }

  reset(): void { this.lastFrame = null; this.arrivalOffsetBaselineMs = null }
  getLastSeq(): number { return this.lastFrame?.seq ?? -1 }

  ingest(raw: unknown, arrivalMs = Date.now()): AdaptResult {
    let value = raw
    if (typeof raw === 'string') {
      try { value = JSON.parse(raw) } catch { return { accepted: false, reason: 'invalid_json' } }
    }
    if (!this.isMotionFrame(value)) return { accepted: false, reason: 'invalid_shape' }
    const previous = this.lastFrame
    if (previous) {
      const rejection = this.getSemanticReject(previous, value)
      if (rejection) return { accepted: false, reason: rejection }
    }
    if (value.training_state === 'REST' && value.rep_event !== 'none') {
      return { accepted: false, reason: 'invalid_rest_event' }
    }
    const diagnostics = this.collectDiagnostics(previous, value, arrivalMs)
    if (this.arrivalOffsetBaselineMs === null) this.arrivalOffsetBaselineMs = arrivalMs - value.timestamp_ms
    this.lastFrame = { ...value }
    return { accepted: true, frame: { ...value }, diagnostics }
  }

  private getSemanticReject(previous: MotionFrame, frame: MotionFrame): RejectReason | null {
    if (frame.seq <= previous.seq) return 'stale_seq'
    if (previous.training_state === 'FINISHED' || previous.training_state === 'STOPPED') return 'after_terminal'
    if (frame.timestamp_ms < previous.timestamp_ms) return 'timestamp_regression'
    if (!this.isAllowedTransition(previous.training_state, frame.training_state)) return 'invalid_state_transition'
    if (frame.target_count !== previous.target_count || frame.target_sets !== previous.target_sets) return 'target_changed'
    if (
      frame.target_angle_deg !== previous.target_angle_deg ||
      frame.valid_angle_deg !== previous.valid_angle_deg ||
      frame.return_angle_deg !== previous.return_angle_deg
    ) return 'parameter_changed'
    if (frame.overall_completion_percent < previous.overall_completion_percent) return 'overall_completion_regression'

    if (frame.set_index === previous.set_index) {
      if (frame.left_count < previous.left_count || frame.right_count < previous.right_count) return 'count_regression'
      if (frame.completion_percent < previous.completion_percent) return 'completion_regression'
    } else {
      const validNextSet = previous.training_state === 'REST' && frame.training_state === 'RUNNING' &&
        frame.set_index === previous.set_index + 1 && frame.left_count === 0 && frame.right_count === 0 &&
        frame.completion_percent === 0
      if (!validNextSet) return 'invalid_set_transition'
    }
    return null
  }

  private isAllowedTransition(previous: TrainingState, next: TrainingState): boolean {
    if (previous === 'IDLE') return next === 'IDLE' || next === 'RUNNING' || next === 'STOPPED'
    if (previous === 'RUNNING') return ['RUNNING', 'PAUSED', 'REST', 'FINISHED', 'STOPPED'].includes(next)
    if (previous === 'PAUSED') return ['PAUSED', 'RUNNING', 'STOPPED'].includes(next)
    if (previous === 'REST') return ['REST', 'RUNNING', 'FINISHED', 'STOPPED'].includes(next)
    return false
  }

  private collectDiagnostics(previous: MotionFrame | null, frame: MotionFrame, arrivalMs: number): FrameDiagnostic[] {
    const diagnostics: FrameDiagnostic[] = []
    if (previous) {
      const seqGap = frame.seq - previous.seq
      if (seqGap > 1) diagnostics.push({ code: 'seq_gap', value: seqGap })
      const timestampGap = frame.timestamp_ms - previous.timestamp_ms
      if (timestampGap > this.gapDiagnosticMs) diagnostics.push({ code: 'timestamp_gap', value: timestampGap })
      if (frame.set_index === previous.set_index) {
        const countJump = Math.max(frame.left_count - previous.left_count, frame.right_count - previous.right_count)
        if (countJump > 1) diagnostics.push({ code: 'count_jump', value: countJump })
      }
    }
    if (this.arrivalOffsetBaselineMs !== null) {
      const drift = Math.abs(arrivalMs - frame.timestamp_ms - this.arrivalOffsetBaselineMs)
      if (drift > this.latencyDriftMs) diagnostics.push({ code: 'latency_drift', value: drift })
    }
    return diagnostics
  }

  private isMotionFrame(value: unknown): value is MotionFrame {
    if (!value || typeof value !== 'object') return false
    const frame = value as Record<string, unknown>
    return isNonNegativeInteger(frame.seq) && isFiniteNumber(frame.timestamp_ms) && frame.timestamp_ms >= 0 &&
      frame.mode === 'upper' && frame.exercise === 'elbow_flexion' && frame.body_mode === 'upper' &&
      ['standard', 'game', 'custom'].includes(String(frame.train_mode)) &&
      isFiniteNumber(frame.left_angle_deg) && isFiniteNumber(frame.right_angle_deg) &&
      isFiniteNumber(frame.left_rom_deg) && frame.left_rom_deg >= 0 &&
      isFiniteNumber(frame.right_rom_deg) && frame.right_rom_deg >= 0 &&
      isFiniteNumber(frame.lr_rom_diff_deg) && frame.lr_rom_diff_deg >= 0 &&
      isNonNegativeInteger(frame.left_count) && isNonNegativeInteger(frame.right_count) &&
      isNonNegativeInteger(frame.target_count) && frame.target_count > 0 &&
      isFiniteNumber(frame.completion_percent) && frame.completion_percent >= 0 && frame.completion_percent <= 100 &&
      TRAINING_STATES.includes(frame.training_state as TrainingState) && REP_EVENTS.includes(frame.rep_event as RepEvent) &&
      isFiniteNumber(frame.left_speed_deg_s) && isFiniteNumber(frame.right_speed_deg_s) &&
      isNonNegativeInteger(frame.set_index) && frame.set_index >= 1 &&
      isNonNegativeInteger(frame.target_sets) && frame.target_sets >= frame.set_index &&
      isNonNegativeInteger(frame.rest_remaining_sec) &&
      isFiniteNumber(frame.target_angle_deg) && isFiniteNumber(frame.valid_angle_deg) && isFiniteNumber(frame.return_angle_deg) &&
      frame.return_angle_deg < frame.valid_angle_deg && frame.valid_angle_deg <= frame.target_angle_deg &&
      isFiniteNumber(frame.overall_completion_percent) && frame.overall_completion_percent >= 0 && frame.overall_completion_percent <= 100 &&
      QUALITY_VALUES.includes(frame.quality as MotionQuality) && WARNING_VALUES.includes(frame.warning as MotionWarning)
  }
}
