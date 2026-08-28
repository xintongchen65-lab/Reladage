import type { BaseMotionFrame, MotionQuality, MotionWarning } from '../../game-platform/motion/types'

export type SquatStage = 'STANDING' | 'DESCENDING' | 'BOTTOM' | 'RISING'

export interface SquatMotionFrame extends BaseMotionFrame {
  mode: 'lower'
  body_mode: 'lower'
  exercise: 'box_squat'
  rep_event: 'none' | 'both_rep_done'
  motion_progress: number
  motion_stage: SquatStage
  symmetry_percent: number
  quality: MotionQuality
  warning: MotionWarning
}

export function createInitialSquatFrame(targetCount = 10, targetSets = 3): SquatMotionFrame {
  return {
    seq: 0, timestamp_ms: 0, mode: 'lower', body_mode: 'lower', train_mode: 'game', exercise: 'box_squat',
    left_angle_deg: 0, right_angle_deg: 0, left_rom_deg: 0, right_rom_deg: 0, lr_rom_diff_deg: 0,
    left_count: 0, right_count: 0, target_count: targetCount, completion_percent: 0,
    training_state: 'RUNNING', rep_event: 'none', left_speed_deg_s: 0, right_speed_deg_s: 0,
    set_index: 1, target_sets: targetSets, rest_remaining_sec: 0, target_angle_deg: 90,
    valid_angle_deg: 70, return_angle_deg: 20, overall_completion_percent: 0,
    quality: 'READY', warning: 'none', motion_progress: 0, motion_stage: 'STANDING', symmetry_percent: 100
  }
}
