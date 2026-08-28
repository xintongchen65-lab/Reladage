import { loadRecords, persistRecord, type GameTrainingRecord, type SyncStorage } from '../../game-platform/records/record-store'
import type { PenaltyTrainingResult } from '../types/result'

export const PENALTY_RECORD_KEY = 'rehabmotion-penalty-game:records:v1'
export type PenaltyRecord = GameTrainingRecord<PenaltyTrainingResult>
const finite = (value: unknown): value is number => typeof value === 'number' && Number.isFinite(value)

export function normalizePenaltyResult(value: unknown): PenaltyTrainingResult | null {
  if (!value || typeof value !== 'object') return null
  const result = value as PenaltyTrainingResult
  if (result.gameId !== 'penalty' || !['FINISHED','STOPPED'].includes(result.endReason) || !finite(result.completedAtMs) || !result.training || !result.game) return null
  if (![result.training.left_count, result.training.right_count, result.game.shots, result.game.goals, result.game.saves, result.game.misses, result.game.score].every(finite)) return null
  return result
}
export function loadPenaltyRecords(storage?: SyncStorage): PenaltyRecord[] {
  return loadRecords({ storageKey: PENALTY_RECORD_KEY, normalizeResult: normalizePenaltyResult, storage })
}
export function savePenaltyResult(result: PenaltyTrainingResult, storage: SyncStorage = uni) {
  return persistRecord({ storageKey: PENALTY_RECORD_KEY, idPrefix: 'penalty', result, normalizeResult: normalizePenaltyResult, storage })
}
