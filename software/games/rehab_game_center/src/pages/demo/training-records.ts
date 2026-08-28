export const TRAINING_RECORD_STORAGE_KEY = 'rehabmotion-fruit-game:records:v1'

export type DemoRecordStatus = 'COMPLETED' | 'PARTIAL'

export interface DemoTrainingResult {
  endReason: 'FINISHED' | 'STOPPED'
  elapsedMs: number
  activeElapsedMs: number
  completedAtMs: number
  training: {
    left_count: number
    right_count: number
    left_rom_deg: number
    right_rom_deg: number
    lr_rom_diff_deg: number
    target_count: number
    completion_percent: number
    training_state: 'IDLE' | 'RUNNING' | 'PAUSED' | 'REST' | 'FINISHED' | 'STOPPED'
    set_index?: number
    target_sets?: number
    overall_completion_percent?: number
    left_total_count?: number
    right_total_count?: number
    session_left_rom_deg?: number
    session_right_rom_deg?: number
    session_lr_rom_diff_deg?: number
  }
  game: {
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
}

export interface DemoTrainingRecord {
  id: string
  status: DemoRecordStatus
  savedAtMs: number
  result: DemoTrainingResult
}

export interface SyncStorage {
  getStorageSync(key: string): unknown
  setStorageSync(key: string, value: unknown): void
}

export interface PersistRecordResult {
  records: DemoTrainingRecord[]
  saved: boolean
  reason?: 'no_activity' | 'duplicate' | 'storage_error'
  error?: unknown
}

function isFiniteNumber(value: unknown): value is number {
  return typeof value === 'number' && Number.isFinite(value)
}

export function hasTrainingActivity(result: DemoTrainingResult): boolean {
  return (result.training.left_total_count ?? result.training.left_count) +
    (result.training.right_total_count ?? result.training.right_count) > 0
}

export function normalizeDemoTrainingResult(value: unknown): DemoTrainingResult | null {
  if (!value || typeof value !== 'object') return null
  const candidate = value as DemoTrainingResult
  const training = candidate.training
  const game = candidate.game
  const valid = (
    (candidate.endReason === 'FINISHED' || candidate.endReason === 'STOPPED') &&
    isFiniteNumber(candidate.elapsedMs) && candidate.elapsedMs >= 0 &&
    isFiniteNumber(candidate.activeElapsedMs) && candidate.activeElapsedMs >= 0 &&
    isFiniteNumber(candidate.completedAtMs) && candidate.completedAtMs > 0 &&
    !!training &&
    isFiniteNumber(training.left_count) && training.left_count >= 0 &&
    isFiniteNumber(training.right_count) && training.right_count >= 0 &&
    isFiniteNumber(training.left_rom_deg) &&
    isFiniteNumber(training.right_rom_deg) &&
    isFiniteNumber(training.lr_rom_diff_deg) &&
    isFiniteNumber(training.target_count) && training.target_count > 0 &&
    isFiniteNumber(training.completion_percent) &&
    !!game &&
    isFiniteNumber(game.harvestedCount) &&
    isFiniteNumber(game.score) &&
    isFiniteNumber(game.combo) &&
    isFiniteNumber(game.maxCombo) &&
    isFiniteNumber(game.wrongSideCount)
  )
  if (!valid) return null
  return {
    ...candidate,
    training: {
      ...training,
      set_index: isFiniteNumber(training.set_index) ? training.set_index : 1,
      target_sets: isFiniteNumber(training.target_sets) ? training.target_sets : 1,
      overall_completion_percent: isFiniteNumber(training.overall_completion_percent) ? training.overall_completion_percent : training.completion_percent,
      left_total_count: isFiniteNumber(training.left_total_count) ? training.left_total_count : training.left_count,
      right_total_count: isFiniteNumber(training.right_total_count) ? training.right_total_count : training.right_count,
      session_left_rom_deg: isFiniteNumber(training.session_left_rom_deg) ? training.session_left_rom_deg : training.left_rom_deg,
      session_right_rom_deg: isFiniteNumber(training.session_right_rom_deg) ? training.session_right_rom_deg : training.right_rom_deg,
      session_lr_rom_diff_deg: isFiniteNumber(training.session_lr_rom_diff_deg) ? training.session_lr_rom_diff_deg : training.lr_rom_diff_deg
    },
    game: {
      ...game,
      normalFruitCount: isFiniteNumber(game.normalFruitCount) ? game.normalFruitCount : Math.max(0, game.harvestedCount),
      goldenAppleCount: isFiniteNumber(game.goldenAppleCount) ? game.goldenAppleCount : 0,
      rainbowFruitCount: isFiniteNumber(game.rainbowFruitCount) ? game.rainbowFruitCount : 0,
      bothWatermelonCount: isFiniteNumber(game.bothWatermelonCount) ? game.bothWatermelonCount : 0
    }
  }
}

export function isDemoTrainingResult(value: unknown): value is DemoTrainingResult {
  return normalizeDemoTrainingResult(value) !== null
}

function normalizeRecord(value: unknown): DemoTrainingRecord | null {
  if (!value || typeof value !== 'object') return null
  const candidate = value as DemoTrainingRecord
  const result = normalizeDemoTrainingResult(candidate.result)
  if (!(
    typeof candidate.id === 'string' &&
    candidate.id.length > 0 &&
    (candidate.status === 'COMPLETED' || candidate.status === 'PARTIAL') &&
    isFiniteNumber(candidate.savedAtMs) &&
    result &&
    hasTrainingActivity(result)
  )) return null
  return { ...candidate, result }
}

export function normalizeTrainingRecords(value: unknown): DemoTrainingRecord[] {
  if (!Array.isArray(value)) return []
  const byCompletedAt = new Map<number, DemoTrainingRecord>()
  value.forEach((item) => {
    const record = normalizeRecord(item)
    if (!record) return
    if (!byCompletedAt.has(record.result.completedAtMs)) byCompletedAt.set(record.result.completedAtMs, record)
  })
  return [...byCompletedAt.values()].sort((left, right) => right.result.completedAtMs - left.result.completedAtMs)
}

export function selectVisibleTrainingRecords(
  records: DemoTrainingRecord[],
  showAll: boolean
): DemoTrainingRecord[] {
  return showAll ? records : records.slice(0, 3)
}

export function appendTrainingRecord(
  records: DemoTrainingRecord[],
  result: DemoTrainingResult
): PersistRecordResult {
  const normalized = normalizeTrainingRecords(records)
  if (!hasTrainingActivity(result)) return { records: normalized, saved: false, reason: 'no_activity' }
  if (normalized.some((item) => item.result.completedAtMs === result.completedAtMs)) {
    return { records: normalized, saved: false, reason: 'duplicate' }
  }
  const record: DemoTrainingRecord = {
    id: `fruit-${Math.trunc(result.completedAtMs)}`,
    status: result.endReason === 'FINISHED' ? 'COMPLETED' : 'PARTIAL',
    savedAtMs: result.completedAtMs,
    result
  }
  return {
    records: [record, ...normalized].sort((left, right) => right.result.completedAtMs - left.result.completedAtMs),
    saved: true
  }
}

export function loadTrainingRecords(storage: SyncStorage = uni): DemoTrainingRecord[] {
  try {
    return normalizeTrainingRecords(storage.getStorageSync(TRAINING_RECORD_STORAGE_KEY))
  } catch {
    return []
  }
}

export function persistTrainingResult(
  result: DemoTrainingResult,
  storage: SyncStorage = uni
): PersistRecordResult {
  const appended = appendTrainingRecord(loadTrainingRecords(storage), result)
  if (!appended.saved) return appended
  try {
    storage.setStorageSync(TRAINING_RECORD_STORAGE_KEY, appended.records)
    return appended
  } catch (error) {
    return { records: appended.records, saved: false, reason: 'storage_error', error }
  }
}
