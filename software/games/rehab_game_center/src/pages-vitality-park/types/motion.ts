import type { BaseMotionFrame, RepEvent, TrainingState, MotionQuality, MotionWarning } from '../../game-platform/motion/types'

export type VitalityMotionStage =
  | 'SITTING'
  | 'LEAN_FORWARD'
  | 'LIFT_OFF'
  | 'HALF_STANDING'
  | 'STANDING'
  | 'SIT_BACK'

export type VitalityRepEvent = Extract<RepEvent, 'none' | 'sit_to_stand_done'>

export interface VitalityMotionFrame extends BaseMotionFrame {
  mode: 'lower'
  body_mode: 'lower'
  exercise: 'sit_to_stand'
  rep_event: VitalityRepEvent
  motion_progress?: number
  motion_stage?: VitalityMotionStage
}

export type RawVitalityMotionFrame = Omit<VitalityMotionFrame, 'motion_progress' | 'motion_stage'> & {
  motion_progress?: number
  motion_stage?: VitalityMotionStage
}

export const DEFAULT_TARGET_COUNT = 10
export const DEFAULT_TARGET_SETS = 1
export const DEFAULT_TARGET_ANGLE_DEG = 80
export const DEFAULT_VALID_ANGLE_DEG = 60
export const DEFAULT_RETURN_ANGLE_DEG = 20

export function createInitialVitalityFrame(
  targetCount = DEFAULT_TARGET_COUNT,
  targetSets = DEFAULT_TARGET_SETS
): VitalityMotionFrame {
  return {
    seq: 0,
    timestamp_ms: 0,
    mode: 'lower',
    body_mode: 'lower',
    exercise: 'sit_to_stand',
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

export function vitalityCount(frame: VitalityMotionFrame): number {
  return Math.max(0, (frame.set_index - 1) * frame.target_count + Math.max(frame.left_count, frame.right_count))
}

export type VitalityTrainingState = TrainingState
export type VitalityMotionQuality = MotionQuality
export type VitalityMotionWarning = MotionWarning
