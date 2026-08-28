import { afterEach, describe, expect, it, vi } from 'vitest'
import { FakeDataSource } from '../src/pages-fruit-game/data-sources/fake-data-source'
import type { MotionFrame } from '../src/pages-fruit-game/types/motion'

function collect(source: FakeDataSource): MotionFrame[] { const frames: MotionFrame[] = []; source.subscribe((frame) => frames.push(frame)); source.start(); return frames }
function fullCycle(source: FakeDataSource, side: 'left' | 'right' | 'both' = 'left'): void {
  if (side !== 'right') source.setHeldControl('leftFlex', true)
  if (side !== 'left') source.setHeldControl('rightFlex', true)
  vi.advanceTimersByTime(1280)
  source.setHeldControl('leftFlex', false); source.setHeldControl('rightFlex', false)
  if (side !== 'right') source.setHeldControl('leftExtend', true)
  if (side !== 'left') source.setHeldControl('rightExtend', true)
  vi.advanceTimersByTime(1280)
  source.setHeldControl('leftExtend', false); source.setHeldControl('rightExtend', false)
}

describe('FakeDataSource v3', () => {
  afterEach(() => vi.useRealTimers())

  it('increases angle during flexion and counts only after returning low', () => {
    vi.useFakeTimers(); const source = new FakeDataSource({ targetCount: 10, frameRateHz: 25 }); const frames = collect(source)
    source.setHeldControl('leftFlex', true); vi.advanceTimersByTime(1280)
    expect(frames.at(-1)?.left_angle_deg).toBeGreaterThanOrEqual(60)
    expect(frames.at(-1)?.left_count).toBe(0)
    source.setHeldControl('leftFlex', false); source.setHeldControl('leftExtend', true); vi.advanceTimersByTime(1000)
    expect(frames.filter((frame) => frame.rep_event === 'left_rep_done')).toHaveLength(1)
    expect(frames.at(-1)?.left_count).toBe(1); source.stop()
  })

  it('requires a complete second cycle', () => {
    vi.useFakeTimers(); const source = new FakeDataSource({ targetCount: 10 }); const frames = collect(source)
    fullCycle(source); vi.advanceTimersByTime(1000); expect(frames.filter((f) => f.rep_event === 'left_rep_done')).toHaveLength(1)
    fullCycle(source); expect(frames.filter((f) => f.rep_event === 'left_rep_done')).toHaveLength(2); source.stop()
  })

  it('emits both and enters REST before resetting the next set', () => {
    vi.useFakeTimers(); const source = new FakeDataSource({ targetCount: 1, targetSets: 2, restDurationSec: 1 }); const frames = collect(source)
    fullCycle(source, 'both')
    expect(frames.some((frame) => frame.rep_event === 'both_rep_done')).toBe(true)
    vi.advanceTimersByTime(80); expect(frames.at(-1)?.training_state).toBe('REST')
    expect(frames.at(-1)?.overall_completion_percent).toBe(50)
    vi.advanceTimersByTime(1100); expect(frames.at(-1)?.set_index).toBe(2); expect(frames.at(-1)?.left_count).toBe(0); source.stop()
  })

  it('keeps sequence frames flowing while paused and starts at zero degrees', () => {
    vi.useFakeTimers(); const source = new FakeDataSource(); const frames = collect(source)
    source.setPaused(true); source.setHeldControl('leftFlex', true); vi.advanceTimersByTime(80); source.stop()
    expect(frames.at(-1)?.training_state).toBe('PAUSED'); expect(frames.at(-1)?.left_angle_deg).toBe(0); expect(frames.at(-1)?.seq).toBeGreaterThan(frames[0].seq)
  })
})
