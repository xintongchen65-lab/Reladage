import type { MotionFrame } from './motion'

export type SessionEndReason = 'FINISHED' | 'STOPPED'

export interface SessionConfig {
  targetCount: number
  targetSets: number
  frameRateHz: number
  targetAngleDeg: number
  validAngleDeg: number
  returnAngleDeg: number
  restDurationSec: number
  dataTimeoutMs: number
  debugEnabled: boolean
}

export interface GameMetrics {
  harvestedCount: number
  normalFruitCount: number
  goldenAppleCount: number
  rainbowFruitCount: number
  bothWatermelonCount: number
  score: number
  combo: number
  maxCombo: number
  wrongSideCount: number
}

export interface TrainingResult {
  endReason: SessionEndReason
  elapsedMs: number
  activeElapsedMs: number
  completedAtMs: number
  training: Pick<MotionFrame,
    | 'left_count' | 'right_count' | 'left_rom_deg' | 'right_rom_deg'
    | 'lr_rom_diff_deg' | 'target_count' | 'completion_percent' | 'training_state'
    | 'set_index' | 'target_sets' | 'overall_completion_percent'
  > & {
    left_total_count: number
    right_total_count: number
    session_left_rom_deg: number
    session_right_rom_deg: number
    session_lr_rom_diff_deg: number
  }
  game: GameMetrics
}
