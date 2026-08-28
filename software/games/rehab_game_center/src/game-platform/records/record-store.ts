export type RecordStatus = 'COMPLETED' | 'PARTIAL'

export interface PersistableTrainingResult {
  endReason: 'FINISHED' | 'STOPPED'
  elapsedMs: number
  activeElapsedMs: number
  completedAtMs: number
  training: {
    left_count: number
    right_count: number
    left_total_count?: number
    right_total_count?: number
    [key: string]: unknown
  }
  game: object
}

export interface GameTrainingRecord<TResult extends PersistableTrainingResult> {
  id: string
  status: RecordStatus
  savedAtMs: number
  result: TResult
}

export interface SyncStorage {
  getStorageSync(key: string): unknown
  setStorageSync(key: string, value: unknown): void
}

export interface PersistRecordResult<TResult extends PersistableTrainingResult> {
  records: GameTrainingRecord<TResult>[]
  saved: boolean
  reason?: 'no_activity' | 'duplicate' | 'storage_error'
  error?: unknown
}

export function hasTrainingActivity(result: PersistableTrainingResult): boolean {
  return Number(result.training.left_total_count ?? result.training.left_count) +
    Number(result.training.right_total_count ?? result.training.right_count) > 0
}

export function normalizeRecordList<TResult extends PersistableTrainingResult>(
  value: unknown,
  normalizeResult: (value: unknown) => TResult | null
): GameTrainingRecord<TResult>[] {
  if (!Array.isArray(value)) return []
  const unique = new Map<number, GameTrainingRecord<TResult>>()
  value.forEach((item) => {
    if (!item || typeof item !== 'object') return
    const raw = item as Record<string, unknown>
    const result = normalizeResult(raw.result)
    if (!result || !hasTrainingActivity(result)) return
    if (typeof raw.id !== 'string' || !['COMPLETED', 'PARTIAL'].includes(String(raw.status))) return
    if (!unique.has(result.completedAtMs)) {
      unique.set(result.completedAtMs, {
        id: raw.id,
        status: raw.status as RecordStatus,
        savedAtMs: Number(raw.savedAtMs) || result.completedAtMs,
        result
      })
    }
  })
  return [...unique.values()].sort((left, right) => right.result.completedAtMs - left.result.completedAtMs)
}

export function loadRecords<TResult extends PersistableTrainingResult>(options: {
  storageKey: string
  normalizeResult: (value: unknown) => TResult | null
  storage?: SyncStorage
}): GameTrainingRecord<TResult>[] {
  const storage = options.storage ?? uni
  try { return normalizeRecordList(storage.getStorageSync(options.storageKey), options.normalizeResult) }
  catch { return [] }
}

export function persistRecord<TResult extends PersistableTrainingResult>(options: {
  storageKey: string
  idPrefix: string
  result: TResult
  normalizeResult: (value: unknown) => TResult | null
  storage?: SyncStorage
}): PersistRecordResult<TResult> {
  const storage = options.storage ?? uni
  const records = loadRecords({ storageKey: options.storageKey, normalizeResult: options.normalizeResult, storage })
  if (!hasTrainingActivity(options.result)) return { records, saved: false, reason: 'no_activity' }
  if (records.some((item) => item.result.completedAtMs === options.result.completedAtMs)) {
    return { records, saved: false, reason: 'duplicate' }
  }
  const record: GameTrainingRecord<TResult> = {
    id: `${options.idPrefix}-${Math.trunc(options.result.completedAtMs)}`,
    status: options.result.endReason === 'FINISHED' ? 'COMPLETED' : 'PARTIAL',
    savedAtMs: options.result.completedAtMs,
    result: options.result
  }
  const next = [record, ...records].sort((left, right) => right.result.completedAtMs - left.result.completedAtMs)
  try { storage.setStorageSync(options.storageKey, next); return { records: next, saved: true } }
  catch (error) { return { records: next, saved: false, reason: 'storage_error', error } }
}
