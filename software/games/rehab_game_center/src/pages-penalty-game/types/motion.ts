import type {
  ActiveSide,
  BaseMotionFrame,
  MotionQuality,
  MotionWarning,
  RepEvent,
  TrainingState
} from '../../game-platform/motion/types'

export type { ActiveSide, MotionQuality, MotionWarning, RepEvent, TrainingState }

export interface KneeMotionFrame extends BaseMotionFrame {
  mode: 'lower'
  body_mode: 'lower'
  exercise: 'knee_flexion'
  active_side: ActiveSide
}

export type RawKneeMotionFrame = Omit<KneeMotionFrame, 'active_side'> & { active_side?: ActiveSide }

export const DEFAULT_TARGET_COUNT = 10
export const DEFAULT_TARGET_SETS = 3
export const DEFAULT_TARGET_ANGLE_DEG = 80
export const DEFAULT_VALID_ANGLE_DEG = 60
export const DEFAULT_RETURN_ANGLE_DEG = 20

export function createInitialKneeFrame(targetCount = 10, targetSets = 3): KneeMotionFrame {
  return {
    seq: 0,
    timestamp_ms: 0,
    mode: 'lower',
    body_mode: 'lower',
    exercise: 'knee_flexion',
    active_side: 'left',
    left_angle_deg: 0,
    right_angle_deg: 0,
    left_rom_deg: 0,
    right_rom_deg: 0,
    lr_rom_diff_deg: 0,
    left_count: 0,
    right_count: 0,
    target_count: targetCount,
    completion_percent: 0,
    overall_completion_percent: 0,
    training_state: 'RUNNING',
    rep_event: 'none',
    left_speed_deg_s: 0,
    right_speed_deg_s: 0,
    train_mode: 'game',
    set_index: 1,
    target_sets: targetSets,
    rest_remaining_sec: 0,
    target_angle_deg: DEFAULT_TARGET_ANGLE_DEG,
    valid_angle_deg: DEFAULT_VALID_ANGLE_DEG,
    return_angle_deg: DEFAULT_RETURN_ANGLE_DEG,
    quality: 'READY',
    warning: 'none'
  }
}

export function totalSideCount(frame: KneeMotionFrame, side: ActiveSide): number {
  const count = side === 'left' ? frame.left_count : frame.right_count
  return Math.max(0, (frame.set_index - 1) * frame.target_count + count)
}
