export type TrainingState = 'IDLE' | 'RUNNING' | 'PAUSED' | 'REST' | 'FINISHED' | 'STOPPED'

export type RepEvent = 'none' | 'left_rep_done' | 'right_rep_done' | 'both_rep_done' | 'sit_to_stand_done'
export type BodyMode = 'upper' | 'lower'
export type TrainMode = 'standard' | 'game' | 'custom'
export type MotionQuality = 'READY' | 'SIGNAL_LOST' | 'REST' | 'ROM_LOW' | 'ASYMMETRY' | 'GOOD'
export type MotionWarning =
  | 'none'
  | 'imu_signal_lost'
  | 'resting'
  | 'range_too_small'
  | 'left_right_asymmetry'

/** Normalized game-facing contract emitted by RehabMotion v3-J. */
export interface MotionFrame {
  seq: number
  timestamp_ms: number
  mode: 'upper'
  exercise: 'elbow_flexion'
  left_angle_deg: number
  right_angle_deg: number
  left_rom_deg: number
  right_rom_deg: number
  lr_rom_diff_deg: number
  left_count: number
  right_count: number
  target_count: number
  completion_percent: number
  training_state: TrainingState
  rep_event: RepEvent
  left_speed_deg_s: number
  right_speed_deg_s: number
  body_mode: BodyMode
  train_mode: TrainMode
  set_index: number
  target_sets: number
  rest_remaining_sec: number
  target_angle_deg: number
  valid_angle_deg: number
  return_angle_deg: number
  overall_completion_percent: number
  quality: MotionQuality
  warning: MotionWarning
}

export const DEFAULT_TARGET_COUNT = 10
export const DEFAULT_TARGET_SETS = 3
export const DEFAULT_TARGET_ANGLE_DEG = 80
export const DEFAULT_VALID_ANGLE_DEG = 60
export const DEFAULT_RETURN_ANGLE_DEG = 20
export const DEFAULT_ANGLE_DEG = 0

export function createInitialMotionFrame(
  targetCount = DEFAULT_TARGET_COUNT,
  targetSets = DEFAULT_TARGET_SETS
): MotionFrame {
  return {
    seq: 0,
    timestamp_ms: 0,
    mode: 'upper',
    exercise: 'elbow_flexion',
    left_angle_deg: DEFAULT_ANGLE_DEG,
    right_angle_deg: DEFAULT_ANGLE_DEG,
    left_rom_deg: 0,
    right_rom_deg: 0,
    lr_rom_diff_deg: 0,
    left_count: 0,
    right_count: 0,
    target_count: targetCount,
    completion_percent: 0,
    training_state: 'RUNNING',
    rep_event: 'none',
    left_speed_deg_s: 0,
    right_speed_deg_s: 0,
    body_mode: 'upper',
    train_mode: 'game',
    set_index: 1,
    target_sets: targetSets,
    rest_remaining_sec: 0,
    target_angle_deg: DEFAULT_TARGET_ANGLE_DEG,
    valid_angle_deg: DEFAULT_VALID_ANGLE_DEG,
    return_angle_deg: DEFAULT_RETURN_ANGLE_DEG,
    overall_completion_percent: 0,
    quality: 'GOOD',
    warning: 'none'
  }
}

export function totalSideCount(frame: MotionFrame, side: 'left' | 'right'): number {
  const current = side === 'left' ? frame.left_count : frame.right_count
  return Math.max(0, (frame.set_index - 1) * frame.target_count + current)
}
