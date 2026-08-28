import { describe, expect, it, vi } from 'vitest'
import { MotionSessionGuard } from '../src/game-platform/motion/session-guard'
import { MoleRepReconciler, MoleTrainingAccumulator } from '../src/pages-mole-game/core/game-engine'
import { MoleMotionFrameAdapter } from '../src/pages-mole-game/core/motion-adapter'
import { MoleRealDataSource } from '../src/pages-mole-game/data-sources/real-data-source'
import { createInitialSquatFrame, type SquatMotionFrame } from '../src/pages-mole-game/types/motion'
import { createInitialVitalityFrame } from '../src/pages-vitality-park/types/motion'

function frame(patch: Partial<SquatMotionFrame> = {}): SquatMotionFrame {
  return { ...createInitialSquatFrame(), seq: 1, timestamp_ms: 40, ...patch }
}

describe('mole protocol alignment', () => {
  it('accepts RUNNING -> REST -> next RUNNING while preserving overall progress', () => {
    const adapter = new MoleMotionFrameAdapter()
    expect(adapter.ingest(frame({ left_count: 10, right_count: 10, completion_percent: 100, overall_completion_percent: 33 })).accepted).toBe(true)
    expect(adapter.ingest(frame({ seq: 2, timestamp_ms: 80, training_state: 'REST', left_count: 10, right_count: 10, completion_percent: 100, overall_completion_percent: 33, rest_remaining_sec: 30, quality: 'REST', warning: 'resting' })).accepted).toBe(true)
    expect(adapter.ingest(frame({ seq: 3, timestamp_ms: 120, set_index: 2, left_count: 0, right_count: 0, completion_percent: 0, overall_completion_percent: 33 })).accepted).toBe(true)
  })

  it('rejects REST frames carrying a medical event', () => {
    const adapter = new MoleMotionFrameAdapter()
    const result = adapter.ingest(frame({ training_state: 'REST', rep_event: 'both_rep_done', rest_remaining_sec: 30 }))
    expect(result).toEqual({ accepted: false, reason: 'invalid_rest_event' })
  })

  it('deduplicates an event followed by its count increment', () => {
    const reconciler = new MoleRepReconciler()
    reconciler.reset(frame({ seq: 0, timestamp_ms: 0 }))
    expect(reconciler.accept(frame({ rep_event: 'both_rep_done' }))).toMatchObject({ accepted: true, source: 'event' })
    expect(reconciler.accept(frame({ seq: 2, left_count: 1, right_count: 1 }))).toMatchObject({ accepted: false })
  })

  it('uses a single count increment as fallback but never expands a count jump', () => {
    const reconciler = new MoleRepReconciler()
    reconciler.reset(frame({ seq: 0, timestamp_ms: 0 }))
    expect(reconciler.accept(frame({ left_count: 1, right_count: 1 }))).toMatchObject({ accepted: true, source: 'count' })
    expect(reconciler.accept(frame({ seq: 2, left_count: 3, right_count: 3 }))).toMatchObject({ accepted: false, countJump: true })
  })

  it('keeps totals, ROM and overall progress across sets', () => {
    const accumulator = new MoleTrainingAccumulator()
    accumulator.accept(frame({ left_count: 10, right_count: 10, left_rom_deg: 72, right_rom_deg: 70, overall_completion_percent: 33 }))
    const aggregate = accumulator.accept(frame({ seq: 2, set_index: 2, left_count: 3, right_count: 3, left_rom_deg: 68, right_rom_deg: 75, overall_completion_percent: 43 }))
    expect(aggregate).toMatchObject({ leftTotalCount: 13, rightTotalCount: 13, leftMaxRomDeg: 72, rightMaxRomDeg: 75, overallCompletionPercent: 43, finalSetIndex: 2 })
  })

  it('filters duplicate frames in the real data source', () => {
    let push: ((payload: unknown) => void) | null = null
    const transport = { subscribe(listener: (payload: unknown) => void) { push = listener; return () => { push = null } }, start: vi.fn(), stop: vi.fn() }
    const rejected = vi.fn()
    const source = new MoleRealDataSource(transport, rejected)
    const received: number[] = []
    source.subscribe((value) => received.push(value.seq))
    source.start()
    push?.(frame())
    push?.(frame())
    expect(received).toEqual([1])
    expect(rejected).toHaveBeenCalledWith('stale_seq')
    source.stop()
  })

  it('requires both sides to return before sit-to-stand rewards resume', () => {
    const guard = new MotionSessionGuard({ dataTimeoutMs: 1000, returnAngleDeg: 20, startedAtMs: 0 })
    const base = createInitialVitalityFrame()
    guard.acceptFrame({ ...base, seq: 1, left_angle_deg: 10, right_angle_deg: 45 }, 0)
    expect(guard.canScore('sit_to_stand_done')).toBe(false)
    guard.acceptFrame({ ...base, seq: 2, left_angle_deg: 10, right_angle_deg: 10 }, 40)
    expect(guard.canScore('sit_to_stand_done')).toBe(true)
  })
})
