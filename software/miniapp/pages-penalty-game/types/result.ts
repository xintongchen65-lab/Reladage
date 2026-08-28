import type { KneeMotionFrame } from './motion'

export interface PenaltySessionConfig {
  targetCount: number
  targetSets: number
  frameRateHz: number
  targetAngleDeg: number
  validAngleDeg: number
  returnAngleDeg: number
  restDurationSec: number
  dataTimeoutMs: number
  debugEnabled: boolean
  sourceKind: 'fake' | 'real'
}

export interface PenaltyGameMetrics {
  shots: number
  goals: number
  saves: number
  misses: number
  score: number
  combo: number
  bestCombo: number
}

export interface PenaltyTrainingResult {
  gameId: 'penalty'
  endReason: 'FINISHED' | 'STOPPED'
  elapsedMs: number
  activeElapsedMs: number
  completedAtMs: number
  training: Pick<KneeMotionFrame,
    'left_count' | 'right_count' | 'left_rom_deg' | 'right_rom_deg' | 'lr_rom_diff_deg' |
    'target_count' | 'completion_percent' | 'training_state' | 'set_index' | 'target_sets' |
    'overall_completion_percent'
  > & {
    left_total_count: number
    right_total_count: number
    session_left_rom_deg: number
    session_right_rom_deg: number
    session_lr_rom_diff_deg: number
  }
  game: PenaltyGameMetrics
}
