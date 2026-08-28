import { describe, expect, it, vi } from 'vitest'
import {
  TRAINING_RECORD_STORAGE_KEY,
  appendTrainingRecord,
  loadTrainingRecords,
  normalizeTrainingRecords,
  persistTrainingResult,
  selectVisibleTrainingRecords
} from '../src/pages/demo/training-records'
import type { DemoTrainingResult, SyncStorage } from '../src/pages/demo/training-records'

function result(completedAtMs: number, leftCount = 1, endReason: 'FINISHED' | 'STOPPED' = 'STOPPED'): DemoTrainingResult {
  return {
    endReason,
    elapsedMs: 5000,
    activeElapsedMs: 4000,
    completedAtMs,
    training: {
      left_count: leftCount,
      right_count: 0,
      left_rom_deg: 80,
      right_rom_deg: 0,
      lr_rom_diff_deg: 80,
      target_count: 10,
      completion_percent: leftCount * 5,
      training_state: endReason
    },
    game: {
      harvestedCount: leftCount,
      normalFruitCount: leftCount,
      goldenAppleCount: 0,
      rainbowFruitCount: 0,
      bothWatermelonCount: 0,
      score: leftCount * 100,
      combo: leftCount,
      maxCombo: leftCount,
      wrongSideCount: 0
    }
  }
}

describe('demo training records', () => {
  it('does not save a zero-count result', () => {
    const appended = appendTrainingRecord([], result(1000, 0))
    expect(appended.saved).toBe(false)
    expect(appended.reason).toBe('no_activity')
    expect(appended.records).toEqual([])
  })

  it('stores partial and completed sessions newest-first without a record limit', () => {
    let records = appendTrainingRecord([], result(1000, 1)).records
    records = appendTrainingRecord(records, result(2000, 2, 'FINISHED')).records
    for (let index = 3; index <= 105; index += 1) {
      records = appendTrainingRecord(records, result(index * 1000, 1)).records
    }
    expect(records).toHaveLength(105)
    expect(records[0].result.completedAtMs).toBe(105000)
    expect(records.at(-1)?.result.completedAtMs).toBe(1000)
    expect(records.find((item) => item.result.completedAtMs === 2000)?.status).toBe('COMPLETED')
  })

  it('deduplicates completedAtMs and exposes only three records until expanded', () => {
    let records = appendTrainingRecord([], result(1000)).records
    records = appendTrainingRecord(records, result(2000)).records
    records = appendTrainingRecord(records, result(3000)).records
    records = appendTrainingRecord(records, result(4000)).records
    const duplicate = appendTrainingRecord(records, result(4000, 2))
    expect(duplicate.saved).toBe(false)
    expect(duplicate.reason).toBe('duplicate')
    expect(selectVisibleTrainingRecords(records, false)).toHaveLength(3)
    expect(selectVisibleTrainingRecords(records, true)).toHaveLength(4)
  })

  it('filters damaged storage records without crashing', () => {
    const valid = appendTrainingRecord([], result(1000)).records[0]
    const normalized = normalizeTrainingRecords([null, { id: 'broken' }, valid])
    expect(normalized).toHaveLength(1)
    expect(normalized[0]).toMatchObject({ id: valid.id, status: valid.status, savedAtMs: valid.savedAtMs })
    expect(normalized[0].result.training).toMatchObject({
      set_index: 1,
      target_sets: 1,
      overall_completion_percent: 5,
      left_total_count: 1,
      right_total_count: 0
    })
    const storage: SyncStorage = {
      getStorageSync: vi.fn(() => 'damaged'),
      setStorageSync: vi.fn()
    }
    expect(loadTrainingRecords(storage)).toEqual([])
  })

  it('keeps v1 records and fills missing special-fruit metrics with zero', () => {
    const legacy = result(1000) as unknown as Record<string, unknown>
    const legacyGame = { ...(legacy.game as Record<string, unknown>) }
    delete legacyGame.normalFruitCount
    delete legacyGame.goldenAppleCount
    delete legacyGame.rainbowFruitCount
    delete legacyGame.bothWatermelonCount
    legacy.game = legacyGame
    const normalized = normalizeTrainingRecords([{
      id: 'legacy',
      status: 'PARTIAL',
      savedAtMs: 1000,
      result: legacy
    }])
    expect(normalized).toHaveLength(1)
    expect(normalized[0].result.game.normalFruitCount).toBe(1)
    expect(normalized[0].result.game.goldenAppleCount).toBe(0)
    expect(normalized[0].result.game.rainbowFruitCount).toBe(0)
    expect(normalized[0].result.game.bothWatermelonCount).toBe(0)
  })

  it('reports storage quota errors without deleting the current in-memory record', () => {
    const storage: SyncStorage = {
      getStorageSync: vi.fn(() => []),
      setStorageSync: vi.fn(() => { throw new Error('quota') })
    }
    const persisted = persistTrainingResult(result(1000), storage)
    expect(storage.setStorageSync).toHaveBeenCalledWith(TRAINING_RECORD_STORAGE_KEY, persisted.records)
    expect(persisted.saved).toBe(false)
    expect(persisted.reason).toBe('storage_error')
    expect(persisted.records).toHaveLength(1)
  })
})
