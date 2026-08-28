import { describe, expect, it } from 'vitest'
import { SessionGuard, shouldAdvanceGameTimers } from '../src/pages-fruit-game/core/session-guard'
import { createInitialMotionFrame } from '../src/pages-fruit-game/types/motion'
import type { MotionFrame } from '../src/pages-fruit-game/types/motion'

const createGuard = () => new SessionGuard({ dataTimeoutMs: 1000, returnAngleDeg: 20, startedAtMs: 0 })
const frame = (patch: Partial<MotionFrame> = {}): MotionFrame => ({ ...createInitialMotionFrame(), seq: 1, ...patch })

describe('SessionGuard v3', () => {
  it('advances game timers only during active or rearming play outside the collection book', () => {
    expect(shouldAdvanceGameTimers('RUNNING', false)).toBe(true)
    expect(shouldAdvanceGameTimers('REARMING', false)).toBe(true)
    expect(shouldAdvanceGameTimers('RUNNING', true)).toBe(false)
    ;(['USER_PAUSED', 'BACKGROUND_PAUSED', 'SOURCE_PAUSED', 'RESTING', 'DATA_INTERRUPTED', 'TERMINAL'] as const)
      .forEach((state) => expect(shouldAdvanceGameTimers(state, false)).toBe(false))
  })
  it('rearms both sides in the low-angle return region', () => {
    const guard = createGuard(); guard.acceptFrame(frame(), 0)
    expect(guard.getState()).toBe('RUNNING'); expect(guard.canScore('both_rep_done')).toBe(true)
  })

  it('freezes on timeout and requires low-angle rearming after recovery', () => {
    const guard = createGuard(); guard.acceptFrame(frame(), 0)
    expect(guard.checkTimeout(999)).toBe(false); expect(guard.checkTimeout(1000)).toBe(true)
    guard.acceptFrame(frame({ seq: 2, left_angle_deg: 60, right_angle_deg: 60 }), 1100)
    expect(guard.getState()).toBe('REARMING'); expect(guard.canScore('left_rep_done')).toBe(false)
    guard.acceptFrame(frame({ seq: 3 }), 1140); expect(guard.getState()).toBe('RUNNING')
  })

  it('excludes REST, user pause and background pause from active time', () => {
    const guard = createGuard(); guard.acceptFrame(frame(), 0)
    guard.acceptFrame(frame({ seq: 2, training_state: 'REST', rest_remaining_sec: 30 }), 100)
    expect(guard.getState()).toBe('RESTING'); expect(guard.getActiveElapsedMs(900)).toBe(100)
    guard.acceptFrame(frame({ seq: 3 }), 1000); guard.pauseByUser(1100); guard.resume(1500); guard.acceptFrame(frame({ seq: 4 }), 1500)
    guard.pauseForBackground(1600); guard.resume(2000); guard.acceptFrame(frame({ seq: 5 }), 2000); guard.finish(2100)
    expect(guard.getActiveElapsedMs(2100)).toBe(400)
  })
})
