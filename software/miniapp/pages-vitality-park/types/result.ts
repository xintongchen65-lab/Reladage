import type { VitalityMotionFrame } from './motion'

export interface VitalitySessionConfig {
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

export interface VitalityGameMetrics {
  activatedEventCount: number
  vitalityValue: number
  combo: number
  bestCombo: number
  celebration: boolean
}

export interface VitalityTrainingResult {
  gameId: 'vitality-park'
  endReason: 'FINISHED' | 'STOPPED'
  elapsedMs: number
  activeElapsedMs: number
  completedAtMs: number
  training: Pick<VitalityMotionFrame, 'left_count' | 'right_count' | 'left_rom_deg' | 'right_rom_deg' | 'lr_rom_diff_deg' | 'target_count' | 'completion_percent' | 'training_state' | 'set_index' | 'target_sets' | 'overall_completion_percent'> & {
    total_count: number
    max_rom_deg: number
    total_sets: number
  }
  game: VitalityGameMetrics
}
