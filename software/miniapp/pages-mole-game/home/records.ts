import { loadRecords, persistRecord, type GameTrainingRecord, type SyncStorage } from '../../game-platform/records/record-store'
import type { MoleTrainingResult } from '../types/result'

export const MOLE_RECORD_KEY = 'rehabmotion-mole-game:records:v1'
export type MoleRecord = GameTrainingRecord<MoleTrainingResult>

const finite = (value: unknown, fallback = 0): number => typeof value === 'number' && Number.isFinite(value) ? value : fallback

export function normalizeMoleResult(value: unknown): MoleTrainingResult | null {
  if (!value || typeof value !== 'object') return null
  const raw = value as Partial<MoleTrainingResult>
  if (raw.gameId !== 'mole' || !['FINISHED', 'STOPPED'].includes(String(raw.endReason)) || !raw.training || !raw.game || !Number.isFinite(raw.completedAtMs)) return null
  const training = raw.training as Partial<MoleTrainingResult['training']>
  const game = raw.game as Partial<MoleTrainingResult['game']>
  const leftCount = finite(training.left_count)
  const rightCount = finite(training.right_count)
  const leftRom = finite(training.left_rom_deg)
  const rightRom = finite(training.right_rom_deg)
  return {
    gameId: 'mole',
    endReason: raw.endReason as 'FINISHED' | 'STOPPED',
    elapsedMs: finite(raw.elapsedMs),
    activeElapsedMs: finite(raw.activeElapsedMs),
    completedAtMs: raw.completedAtMs as number,
    training: {
      left_count: leftCount,
      right_count: rightCount,
      left_total_count: finite(training.left_total_count, leftCount),
      right_total_count: finite(training.right_total_count, rightCount),
      left_rom_deg: leftRom,
      right_rom_deg: rightRom,
      max_rom_deg: finite(training.max_rom_deg, Math.max(leftRom, rightRom)),
      target_count: finite(training.target_count, Math.max(1, leftCount, rightCount)),
      completion_percent: finite(training.completion_percent),
      set_index: finite(training.set_index, 1),
      target_sets: finite(training.target_sets, 1),
      overall_completion_percent: finite(training.overall_completion_percent, finite(training.completion_percent)),
      symmetry_percent: finite(training.symmetry_percent, 100),
      quality: typeof training.quality === 'string' ? training.quality : 'READY',
      warning: typeof training.warning === 'string' ? training.warning : 'none'
    },
    game: {
      rounds: finite(game.rounds), dodges: finite(game.dodges), hits: finite(game.hits), score: finite(game.score),
      combo: finite(game.combo), bestCombo: finite(game.bestCombo), coins: finite(game.coins)
    }
  }
}

export function loadMoleRecords(storage?: SyncStorage): MoleRecord[] {
  return loadRecords({ storageKey: MOLE_RECORD_KEY, normalizeResult: normalizeMoleResult, storage })
}

export function saveMoleResult(result: MoleTrainingResult, storage: SyncStorage = uni) {
  return persistRecord({ storageKey: MOLE_RECORD_KEY, idPrefix: 'mole', result, normalizeResult: normalizeMoleResult, storage })
}
