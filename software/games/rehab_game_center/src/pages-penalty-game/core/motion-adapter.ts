import type { ActiveSide, KneeMotionFrame, RawKneeMotionFrame, RepEvent, TrainingState } from '../types/motion'

export type PenaltyRejectReason = 'invalid_shape' | 'stale_seq' | 'timestamp_regression' | 'count_regression' |
  'target_changed' | 'parameter_changed' | 'overall_completion_regression' | 'invalid_set_transition' |
  'invalid_state_transition' | 'invalid_rest_event' | 'after_terminal'

export type PenaltyAdaptResult = { accepted: true; frame: KneeMotionFrame; diagnostics: string[] } |
  { accepted: false; reason: PenaltyRejectReason }

const states: TrainingState[] = ['IDLE', 'RUNNING', 'PAUSED', 'REST', 'FINISHED', 'STOPPED']
const events: RepEvent[] = ['none', 'left_rep_done', 'right_rep_done', 'both_rep_done']
const numberFields = ['seq', 'timestamp_ms', 'left_angle_deg', 'right_angle_deg', 'left_rom_deg', 'right_rom_deg',
  'lr_rom_diff_deg', 'left_count', 'right_count', 'target_count', 'completion_percent', 'left_speed_deg_s',
  'right_speed_deg_s', 'set_index', 'target_sets', 'rest_remaining_sec', 'target_angle_deg', 'valid_angle_deg',
  'return_angle_deg', 'overall_completion_percent'] as const

export class KneeMotionFrameAdapter {
  private last: KneeMotionFrame | null = null
  private fallbackSide: ActiveSide = 'left'
  reset(): void { this.last = null; this.fallbackSide = 'left' }
  ingest(raw: unknown): PenaltyAdaptResult {
    let value = raw
    if (typeof raw === 'string') { try { value = JSON.parse(raw) } catch { return { accepted: false, reason: 'invalid_shape' } } }
    if (!isRawFrame(value)) return { accepted: false, reason: 'invalid_shape' }
    const candidate = value as RawKneeMotionFrame
    const frame: KneeMotionFrame = { ...candidate, active_side: candidate.active_side ?? this.fallbackSide }
    const previous = this.last
    if (previous) {
      if (frame.seq <= previous.seq) return { accepted: false, reason: 'stale_seq' }
      if (['FINISHED', 'STOPPED'].includes(previous.training_state)) return { accepted: false, reason: 'after_terminal' }
      if (frame.timestamp_ms < previous.timestamp_ms) return { accepted: false, reason: 'timestamp_regression' }
      if (!allowed(previous.training_state, frame.training_state)) return { accepted: false, reason: 'invalid_state_transition' }
      if (frame.target_count !== previous.target_count || frame.target_sets !== previous.target_sets) return { accepted: false, reason: 'target_changed' }
      if (frame.target_angle_deg !== previous.target_angle_deg || frame.valid_angle_deg !== previous.valid_angle_deg || frame.return_angle_deg !== previous.return_angle_deg) return { accepted: false, reason: 'parameter_changed' }
      if (frame.overall_completion_percent < previous.overall_completion_percent) return { accepted: false, reason: 'overall_completion_regression' }
      if (frame.set_index === previous.set_index) {
        if (frame.left_count < previous.left_count || frame.right_count < previous.right_count) return { accepted: false, reason: 'count_regression' }
      } else if (!(previous.training_state === 'REST' && frame.training_state === 'RUNNING' && frame.set_index === previous.set_index + 1 && frame.left_count === 0 && frame.right_count === 0)) {
        return { accepted: false, reason: 'invalid_set_transition' }
      }
    }
    if (frame.training_state === 'REST' && frame.rep_event !== 'none') return { accepted: false, reason: 'invalid_rest_event' }
    const diagnostics: string[] = []
    if (previous && frame.seq - previous.seq > 1) diagnostics.push('seq_gap')
    if (previous && Math.max(frame.left_count - previous.left_count, frame.right_count - previous.right_count) > 1) diagnostics.push('count_jump')
    this.last = { ...frame }
    if (frame.rep_event === `${frame.active_side}_rep_done`) this.fallbackSide = frame.active_side === 'left' ? 'right' : 'left'
    else if (frame.rep_event === 'none' && previous) {
      const activeCount = frame.active_side === 'left' ? frame.left_count : frame.right_count
      const priorCount = frame.active_side === 'left' ? previous.left_count : previous.right_count
      if (activeCount === priorCount + 1) this.fallbackSide = frame.active_side === 'left' ? 'right' : 'left'
    }
    return { accepted: true, frame, diagnostics }
  }
}

function isRawFrame(value: unknown): value is RawKneeMotionFrame {
  if (!value || typeof value !== 'object') return false
  const frame = value as Record<string, unknown>
  if (frame.mode !== 'lower' || frame.body_mode !== 'lower' || frame.exercise !== 'knee_flexion') return false
  if (frame.active_side !== undefined && frame.active_side !== 'left' && frame.active_side !== 'right') return false
  if (!states.includes(frame.training_state as TrainingState) || !events.includes(frame.rep_event as RepEvent)) return false
  if (!['standard', 'game', 'custom'].includes(String(frame.train_mode))) return false
  if (!['READY', 'SIGNAL_LOST', 'REST', 'ROM_LOW', 'ASYMMETRY', 'GOOD'].includes(String(frame.quality))) return false
  if (!['none', 'imu_signal_lost', 'resting', 'range_too_small', 'left_right_asymmetry'].includes(String(frame.warning))) return false
  if (numberFields.some((key) => typeof frame[key] !== 'number' || !Number.isFinite(frame[key]))) return false
  return Number(frame.seq) >= 0 && Number(frame.timestamp_ms) >= 0 && Number(frame.target_count) > 0 &&
    Number(frame.set_index) >= 1 && Number(frame.target_sets) >= Number(frame.set_index) &&
    Number(frame.return_angle_deg) < Number(frame.valid_angle_deg) && Number(frame.valid_angle_deg) <= Number(frame.target_angle_deg) &&
    Number(frame.completion_percent) >= 0 && Number(frame.completion_percent) <= 100 &&
    Number(frame.overall_completion_percent) >= 0 && Number(frame.overall_completion_percent) <= 100
}

function allowed(previous: TrainingState, next: TrainingState): boolean {
  if (previous === 'IDLE') return ['IDLE', 'RUNNING', 'STOPPED'].includes(next)
  if (previous === 'RUNNING') return ['RUNNING', 'PAUSED', 'REST', 'FINISHED', 'STOPPED'].includes(next)
  if (previous === 'PAUSED') return ['PAUSED', 'RUNNING', 'STOPPED'].includes(next)
  if (previous === 'REST') return ['REST', 'RUNNING', 'FINISHED', 'STOPPED'].includes(next)
  return false
}
