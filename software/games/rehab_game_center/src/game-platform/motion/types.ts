export type TrainingState = 'IDLE' | 'RUNNING' | 'PAUSED' | 'REST' | 'FINISHED' | 'STOPPED'
export type RepEvent = 'none' | 'left_rep_done' | 'right_rep_done' | 'both_rep_done' | 'sit_to_stand_done'
export type BodyMode = 'upper' | 'lower'
export type TrainMode = 'standard' | 'game' | 'custom'
export type MotionQuality = 'READY' | 'SIGNAL_LOST' | 'REST' | 'ROM_LOW' | 'ASYMMETRY' | 'GOOD'
export type MotionWarning = 'none' | 'imu_signal_lost' | 'resting' | 'range_too_small' | 'left_right_asymmetry'
export type ActiveSide = 'left' | 'right'

export interface BaseMotionFrame {
  seq: number
  timestamp_ms: number
  mode: BodyMode
  exercise: string
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
  active_side?: ActiveSide
}

export function totalSideCount(frame: BaseMotionFrame, side: ActiveSide): number {
  const current = side === 'left' ? frame.left_count : frame.right_count
  return Math.max(0, (frame.set_index - 1) * frame.target_count + current)
}
