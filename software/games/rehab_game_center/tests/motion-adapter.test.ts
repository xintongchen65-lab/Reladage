import { describe, expect, it } from 'vitest'
import { MotionFrameAdapter } from '../src/pages-fruit-game/core/motion-adapter'
import { createInitialMotionFrame } from '../src/pages-fruit-game/types/motion'
import type { MotionFrame } from '../src/pages-fruit-game/types/motion'

function frame(patch: Partial<MotionFrame> = {}): MotionFrame {
  return { ...createInitialMotionFrame(), seq: 1, ...patch }
}

describe('MotionFrameAdapter', () => {
  it('accepts a valid object and JSON string', () => {
    const adapter = new MotionFrameAdapter()
    const first = frame()
    expect(adapter.ingest(first, 0)).toEqual({ accepted: true, frame: first, diagnostics: [] })

    const second = { ...first, seq: 2, timestamp_ms: 40 }
    expect(adapter.ingest(JSON.stringify(second), 40)).toEqual({
      accepted: true,
      frame: second,
      diagnostics: []
    })
  })

  it('rejects malformed, duplicate and out-of-order frames', () => {
    const adapter = new MotionFrameAdapter()
    const current = frame({ seq: 8 })
    expect(adapter.ingest('{')).toEqual({ accepted: false, reason: 'invalid_json' })
    expect(adapter.ingest({ seq: 1 })).toEqual({ accepted: false, reason: 'invalid_shape' })
    expect(adapter.ingest(current).accepted).toBe(true)
    expect(adapter.ingest(current)).toEqual({ accepted: false, reason: 'stale_seq' })
    expect(adapter.ingest({ ...current, seq: 7 })).toEqual({ accepted: false, reason: 'stale_seq' })
  })

  it('rejects timestamp, count, target and completion regressions', () => {
    const cases: Array<[Partial<MotionFrame>, string]> = [
      [{ timestamp_ms: 99 }, 'timestamp_regression'],
      [{ left_count: 1, completion_percent: 10 }, 'count_regression'],
      [{ target_count: 11 }, 'target_changed'],
      [{ completion_percent: 9 }, 'completion_regression']
    ]
    cases.forEach(([patch, reason]) => {
      const adapter = new MotionFrameAdapter()
      adapter.ingest(frame({ timestamp_ms: 100, left_count: 2, right_count: 2, completion_percent: 20 }))
      expect(adapter.ingest(frame({ seq: 2, timestamp_ms: 140, left_count: 2, right_count: 2, completion_percent: 20, ...patch }))).toEqual({
        accepted: false,
        reason
      })
    })
  })

  it('rejects all data after a terminal frame', () => {
    const adapter = new MotionFrameAdapter()
    expect(adapter.ingest(frame({ training_state: 'FINISHED' })).accepted).toBe(true)
    expect(adapter.ingest(frame({ seq: 2, timestamp_ms: 40, training_state: 'FINISHED' }))).toEqual({
      accepted: false,
      reason: 'after_terminal'
    })
  })

  it('rejects an illegal direct transition from PAUSED to FINISHED', () => {
    const adapter = new MotionFrameAdapter()
    adapter.ingest(frame({ training_state: 'PAUSED' }))
    expect(adapter.ingest(frame({
      seq: 2,
      timestamp_ms: 40,
      training_state: 'FINISHED'
    }))).toEqual({ accepted: false, reason: 'invalid_state_transition' })
  })

  it('reports sequence, timestamp, count and latency anomalies without rejecting the frame', () => {
    const adapter = new MotionFrameAdapter({ gapDiagnosticMs: 500, latencyDriftMs: 100 })
    adapter.ingest(frame({ timestamp_ms: 100 }), 1000)
    const result = adapter.ingest(frame({
      seq: 4,
      timestamp_ms: 800,
      left_count: 3,
      completion_percent: 15
    }), 2000)
    expect(result.accepted).toBe(true)
    if (!result.accepted) return
    expect(result.diagnostics.map((item) => item.code)).toEqual([
      'seq_gap',
      'timestamp_gap',
      'count_jump',
      'latency_drift'
    ])
  })

  it('can reset all state for a new session', () => {
    const adapter = new MotionFrameAdapter()
    const current = frame()
    expect(adapter.ingest(current).accepted).toBe(true)
    adapter.reset()
    expect(adapter.ingest(current).accepted).toBe(true)
  })

  it('accepts REST and one legal group reset while preserving overall progress', () => {
    const adapter = new MotionFrameAdapter()
    expect(adapter.ingest(frame({ left_count: 10, right_count: 10, completion_percent: 100, overall_completion_percent: 33 })).accepted).toBe(true)
    expect(adapter.ingest(frame({ seq: 2, timestamp_ms: 200, training_state: 'REST', rest_remaining_sec: 30, left_count: 10, right_count: 10, completion_percent: 100, overall_completion_percent: 33, quality: 'REST', warning: 'resting' })).accepted).toBe(true)
    expect(adapter.ingest(frame({ seq: 3, timestamp_ms: 400, set_index: 2, left_count: 0, right_count: 0, completion_percent: 0, overall_completion_percent: 33 })).accepted).toBe(true)
  })

  it('rejects REST events, illegal set jumps and overall completion regression', () => {
    const restAdapter = new MotionFrameAdapter()
    expect(restAdapter.ingest(frame({ training_state: 'REST', rep_event: 'left_rep_done', quality: 'REST', warning: 'resting' }))).toEqual({ accepted: false, reason: 'invalid_rest_event' })
    const jumpAdapter = new MotionFrameAdapter()
    jumpAdapter.ingest(frame({ training_state: 'REST', overall_completion_percent: 33, quality: 'REST', warning: 'resting' }))
    expect(jumpAdapter.ingest(frame({ seq: 2, set_index: 3, overall_completion_percent: 33 }))).toEqual({ accepted: false, reason: 'invalid_set_transition' })
    const regressionAdapter = new MotionFrameAdapter()
    regressionAdapter.ingest(frame({ overall_completion_percent: 50 }))
    expect(regressionAdapter.ingest(frame({ seq: 2, overall_completion_percent: 49 }))).toEqual({ accepted: false, reason: 'overall_completion_regression' })
  })
})
