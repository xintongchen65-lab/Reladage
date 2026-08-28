import { afterEach, describe, expect, it, vi } from 'vitest'
import {
  clearSessionRuntime,
  configureSession,
  consumeResult,
  prepareReplaySession,
  publishResult,
  registerResultEmitter
} from '../src/pages-fruit-game/runtime/session-runtime'
import { createTrainingResult, createInitialGameState } from '../src/pages-fruit-game/core/game-engine'
import { createInitialMotionFrame } from '../src/pages-fruit-game/types/motion'

describe('session runtime', () => {
  afterEach(() => clearSessionRuntime())

  it('sanitizes incoming launch configuration', () => {
    expect(configureSession({
      targetCount: 500,
      targetSets: 9,
      frameRateHz: 2,
      targetAngleDeg: 50,
      validAngleDeg: 55,
      returnAngleDeg: 56,
      restDurationSec: 500,
      dataTimeoutMs: 99999,
      debugEnabled: true
    })).toEqual({
      targetCount: 10,
      targetSets: 3,
      frameRateHz: 25,
      targetAngleDeg: 80,
      validAngleDeg: 60,
      returnAngleDeg: 20,
      restDurationSec: 30,
      dataTimeoutMs: 1000,
      debugEnabled: true
    })
  })

  it('publishes an EventChannel result and stores it only once', () => {
    const emitter = vi.fn()
    registerResultEmitter(emitter)
    const result = createTrainingResult(
      'STOPPED',
      { ...createInitialMotionFrame(), training_state: 'STOPPED' },
      createInitialGameState(),
      1000,
      800,
      1000
    )
    expect(publishResult(result)).toBe(true)
    expect(publishResult({ ...result, elapsedMs: 2000 })).toBe(false)
    expect(emitter).toHaveBeenCalledTimes(1)
    expect(consumeResult()).toEqual(result)
    expect(consumeResult()).toBeNull()
  })

  it('starts a replay without replacing configuration or the EventChannel emitter', () => {
    const emitter = vi.fn()
    registerResultEmitter(emitter)
    configureSession({ targetCount: 6, frameRateHz: 50, debugEnabled: true })
    const first = createTrainingResult(
      'STOPPED',
      { ...createInitialMotionFrame(6), training_state: 'STOPPED', left_count: 1 },
      createInitialGameState(),
      1000,
      900,
      1000
    )
    expect(publishResult(first)).toBe(true)
    expect(prepareReplaySession()).toMatchObject({ targetCount: 6, frameRateHz: 50, debugEnabled: true })
    const second = { ...first, completedAtMs: 2000 }
    expect(publishResult(second)).toBe(true)
    expect(emitter).toHaveBeenCalledTimes(2)
    expect(emitter).toHaveBeenLastCalledWith(second)
  })
})
