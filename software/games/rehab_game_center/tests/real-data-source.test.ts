import { describe, expect, it, vi } from 'vitest'
import { RealDataSource } from '../src/pages-fruit-game/data-sources/real-data-source'
import { createInitialMotionFrame } from '../src/pages-fruit-game/types/motion'

describe('RealDataSource', () => {
  it('forwards valid 5 Hz v3 frames once and preserves REST without inventing events', () => {
    let push: ((payload: unknown) => void) | null = null
    const transport = {
      subscribe(listener: (payload: unknown) => void) { push = listener; return () => { push = null } },
      start: vi.fn(),
      stop: vi.fn()
    }
    const rejected = vi.fn()
    const source = new RealDataSource(transport, { onRejectedFrame: rejected })
    const received: number[] = []
    source.subscribe((frame) => received.push(frame.seq))
    source.start()
    const running = { ...createInitialMotionFrame(), seq: 1, timestamp_ms: 200 }
    push?.(JSON.stringify(running))
    push?.(JSON.stringify(running))
    push?.({ ...running, seq: 2, timestamp_ms: 400, training_state: 'REST', rest_remaining_sec: 30, quality: 'REST', warning: 'resting', rep_event: 'none' })
    expect(received).toEqual([1, 2])
    expect(rejected).toHaveBeenCalledWith('stale_seq')
    source.stop()
    expect(transport.start).toHaveBeenCalledOnce()
    expect(transport.stop).toHaveBeenCalledOnce()
  })
})
