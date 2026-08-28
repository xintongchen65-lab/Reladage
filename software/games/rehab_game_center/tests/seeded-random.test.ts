import { describe, expect, it } from 'vitest'
import { createInitialGameState, reduceGameState } from '../src/pages-fruit-game/core/game-engine'
import { createSeededRandom } from '../src/pages-fruit-game/core/seeded-random'
import { createInitialMotionFrame } from '../src/pages-fruit-game/types/motion'

function collect(seed: number, injectWrongSide: boolean): { fruits: string[]; draws: number[] } {
  const seeded = createSeededRandom(seed)
  const draws: number[] = []
  const random = () => { const value = seeded(); draws.push(value); return value }
  let state = createInitialGameState(random)
  const fruits = [state.activeFruit]
  for (let index = 0; index < 8; index += 1) {
    if (injectWrongSide && index === 2) {
      const wrong = state.activeSide === 'left' ? 'right_rep_done' : 'left_rep_done'
      state = reduceGameState(state, { ...createInitialMotionFrame(), rep_event: wrong }, random).state
    }
    const expected = state.activeSide === 'left' ? 'left_rep_done' : 'right_rep_done'
    state = reduceGameState(state, { ...createInitialMotionFrame(), rep_event: expected }, random).state
    fruits.push(state.activeFruit)
  }
  return { fruits, draws }
}

describe('multiplayer seeded fruit stream', () => {
  it('is deterministic for a room seed', () => {
    expect(collect(20260817, false)).toEqual(collect(20260817, false))
    expect(collect(20260817, false)).not.toEqual(collect(20260818, false))
  })

  it('keeps the Nth base draw aligned when one player breaks a combo', () => {
    const uninterrupted = collect(991, false)
    const interrupted = collect(991, true)
    expect(interrupted.draws).toEqual(uninterrupted.draws)
    expect(interrupted.draws).toHaveLength(9)
    expect(interrupted.fruits[5]).not.toBe('')
  })
})
