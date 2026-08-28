import { loadRecords, persistRecord, type GameTrainingRecord, type SyncStorage } from '../../game-platform/records/record-store'
import type { VitalityTrainingResult } from '../types/result'

export const VITALITY_RECORD_KEY = 'rehabmotion-vitality-park:records:v1'
export type VitalityRecord = GameTrainingRecord<VitalityTrainingResult>
export function normalizeVitalityResult(value: unknown): VitalityTrainingResult | null { if (!value || typeof value !== 'object') return null; const raw = value as VitalityTrainingResult; if (raw.gameId !== 'vitality-park' || !raw.training || !raw.game) return null; raw.game.activatedEventCount = Number(raw.game.activatedEventCount) || 0; raw.game.vitalityValue = Number(raw.game.vitalityValue) || 0; raw.game.combo = Number(raw.game.combo) || 0; raw.game.bestCombo = Number(raw.game.bestCombo) || 0; raw.game.celebration = Boolean(raw.game.celebration); return raw }
export function loadVitalityRecords(storage?: SyncStorage): VitalityRecord[] { return loadRecords({ storageKey: VITALITY_RECORD_KEY, normalizeResult: normalizeVitalityResult, storage }) as VitalityRecord[] }
export function saveVitalityResult(result: VitalityTrainingResult, storage?: SyncStorage) { return persistRecord({ storageKey: VITALITY_RECORD_KEY, idPrefix: 'vitality-park', result, normalizeResult: normalizeVitalityResult, storage }) }
