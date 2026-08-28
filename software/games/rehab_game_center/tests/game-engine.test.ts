import { describe, expect, it } from 'vitest'
import {
  activateBonusForProgress,
  advanceComboTimer,
  COMBO_TIMEOUT_MS,
  createInitialGameState,
  createTrainingResult,
  expireBonus,
  pickRandomMainFruit,
  reduceGameState,
  resolveOrchardTheme
} from '../src/pages-fruit-game/core/game-engine'
import { createInitialMotionFrame } from '../src/pages-fruit-game/types/motion'

describe('game engine', () => {
  it('resolves the local day and sunset boundaries', () => {
    expect(resolveOrchardTheme(new Date(2026, 7, 17, 5, 59))).toBe('sunset')
    expect(resolveOrchardTheme(new Date(2026, 7, 17, 6, 0))).toBe('day')
    expect(resolveOrchardTheme(new Date(2026, 7, 17, 17, 59))).toBe('day')
    expect(resolveOrchardTheme(new Date(2026, 7, 17, 18, 0))).toBe('sunset')
  })

  it('maps 82/12/6 random ranges and allows repeated ordinary fruits', () => {
    expect(pickRandomMainFruit(() => 0)).toBe('apple')
    expect(pickRandomMainFruit(() => 0.819999)).toBe('watermelon')
    expect(pickRandomMainFruit(() => 0.82)).toBe('goldenApple')
    expect(pickRandomMainFruit(() => 0.939999)).toBe('goldenApple')
    expect(pickRandomMainFruit(() => 0.94)).toBe('rainbowFruit')

    const firstFrame = { ...createInitialMotionFrame(), seq: 1, rep_event: 'left_rep_done' as const }
    const first = reduceGameState(createInitialGameState(() => 0), firstFrame, () => 0)
    const second = reduceGameState(
      first.state,
      { ...firstFrame, seq: 2, rep_event: 'right_rep_done' as const },
      () => 0
    )
    expect(first.harvestedFruit).toBe('apple')
    expect(first.state.activeFruit).toBe('apple')
    expect(second.harvestedFruit).toBe('apple')
    expect(second.state.inventory.apple).toBe(2)
  })

  it('scores special fruits and forces the next golden apple at each five-combo boundary', () => {
    const frame = { ...createInitialMotionFrame(), seq: 1, rep_event: 'left_rep_done' as const }
    const golden = reduceGameState(
      { ...createInitialGameState(() => 0), activeFruit: 'goldenApple' },
      frame,
      () => 0
    )
    expect(golden.pointsAwarded).toBe(200)
    expect(golden.state.goldenAppleCount).toBe(1)

    const rainbow = reduceGameState(
      { ...createInitialGameState(() => 0), activeFruit: 'rainbowFruit' },
      frame,
      () => 0
    )
    expect(rainbow.pointsAwarded).toBe(300)
    expect(rainbow.state.rainbowFruitCount).toBe(1)

    const comboReward = reduceGameState(
      { ...createInitialGameState(() => 0), combo: 4, maxCombo: 4 },
      frame,
      () => 0.99
    )
    expect(comboReward.state.combo).toBe(5)
    expect(comboReward.state.activeFruit).toBe('goldenApple')
    expect(comboReward.state.activeFruitForced).toBe(true)
  })

  it('keeps rehabilitation counts but rejects a wrong-side reward and breaks combo', () => {
    const state = { ...createInitialGameState(() => 0), combo: 3, maxCombo: 3, comboRemainingMs: 4000 }
    const frame = {
      ...createInitialMotionFrame(),
      seq: 1,
      right_count: 1,
      rep_event: 'right_rep_done' as const
    }
    const transition = reduceGameState(state, frame)
    expect(frame.right_count).toBe(1)
    expect(transition.ignoredWrongSide).toBe(true)
    expect(transition.state.harvestedCount).toBe(0)
    expect(transition.state.combo).toBe(0)
    expect(transition.state.comboRemainingMs).toBe(0)
    expect(transition.state.wrongSideCount).toBe(1)
    expect(transition.state.activeSide).toBe('left')
  })

  it('expires a main-fruit combo after eight seconds of active game time', () => {
    const frame = { ...createInitialMotionFrame(), seq: 1, rep_event: 'left_rep_done' as const }
    const harvested = reduceGameState(createInitialGameState(() => 0), frame, () => 0)
    expect(harvested.state.combo).toBe(1)
    expect(harvested.state.comboRemainingMs).toBe(COMBO_TIMEOUT_MS)

    const beforeDeadline = advanceComboTimer(harvested.state, 7999)
    expect(beforeDeadline.combo).toBe(1)
    expect(beforeDeadline.comboRemainingMs).toBe(1)

    const expired = advanceComboTimer(beforeDeadline, 1)
    expect(expired.combo).toBe(0)
    expect(expired.comboRemainingMs).toBe(0)
    expect(expired.maxCombo).toBe(1)
    expect(expired.score).toBe(100)
    expect(expired.wrongSideCount).toBe(0)
    expect(expired.feedback).toBe('连击已中断，请继续当前侧动作')
  })

  it('does not refresh the main combo timer for a both-hand reward', () => {
    const state = {
      ...activateBonusForProgress(createInitialGameState(() => 0), 35),
      combo: 2,
      maxCombo: 2,
      comboRemainingMs: 3500
    }
    const transition = reduceGameState(state, {
      ...createInitialMotionFrame(),
      seq: 1,
      rep_event: 'both_rep_done' as const
    })
    expect(transition.state.combo).toBe(2)
    expect(transition.state.comboRemainingMs).toBe(3500)
  })

  it('opens non-blocking bonus windows and scores both-hand watermelon without changing side or combo', () => {
    const base = createInitialGameState(() => 0)
    expect(activateBonusForProgress(base, 34)).toBe(base)
    const active = activateBonusForProgress(base, 35)
    expect(active.bonusActive).toBe(true)

    const transition = reduceGameState(active, {
      ...createInitialMotionFrame(),
      seq: 1,
      rep_event: 'both_rep_done' as const
    })
    expect(transition.pointsAwarded).toBe(500)
    expect(transition.state.bothWatermelonCount).toBe(1)
    expect(transition.state.inventory.bothWatermelon).toBe(1)
    expect(transition.state.activeSide).toBe('left')
    expect(transition.state.combo).toBe(0)

    const outside = reduceGameState(base, {
      ...createInitialMotionFrame(),
      seq: 2,
      rep_event: 'both_rep_done' as const
    })
    expect(outside.harvested).toBe(false)
    expect(outside.state.wrongSideCount).toBe(0)

    const expired = expireBonus(active)
    expect(expired.bonusActive).toBe(false)
    expect(activateBonusForProgress(expired, 70).bonusActive).toBe(true)
  })

  it('freezes rules while paused and builds results from JSON training facts', () => {
    const paused = {
      ...createInitialMotionFrame(),
      seq: 1,
      training_state: 'PAUSED' as const,
      rep_event: 'left_rep_done' as const,
      left_count: 4,
      right_count: 3,
      left_rom_deg: 91,
      right_rom_deg: 82,
      lr_rom_diff_deg: 9,
      completion_percent: 35
    }
    const state = createInitialGameState(() => 0)
    expect(reduceGameState(state, paused).state).toBe(state)

    const result = createTrainingResult('STOPPED', paused, state, 5500, 4500, 6000)
    expect(result.training.left_count).toBe(4)
    expect(result.training.completion_percent).toBe(35)
    expect(result.game.normalFruitCount).toBe(0)
    expect(result.activeElapsedMs).toBe(4500)
  })
})
