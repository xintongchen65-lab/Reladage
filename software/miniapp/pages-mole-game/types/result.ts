export interface MoleSessionConfig { targetCount: number; targetSets: number; frameRateHz: number; targetAngleDeg: number; validAngleDeg: number; returnAngleDeg: number; restDurationSec: number; dataTimeoutMs: number; warningWindowMs: number; debugEnabled: boolean; sourceKind: 'fake' | 'real' }
export interface MoleGameMetrics { rounds: number; dodges: number; hits: number; score: number; combo: number; bestCombo: number; coins: number }
export interface MoleTrainingResult {
  gameId: 'mole'
  endReason: 'FINISHED' | 'STOPPED'
  elapsedMs: number
  activeElapsedMs: number
  completedAtMs: number
  training: {
    left_count: number; right_count: number; left_total_count: number; right_total_count: number
    left_rom_deg: number; right_rom_deg: number; max_rom_deg: number
    target_count: number; completion_percent: number; set_index: number; target_sets: number
    overall_completion_percent: number; symmetry_percent: number; quality: string; warning: string
  }
  game: MoleGameMetrics
}
