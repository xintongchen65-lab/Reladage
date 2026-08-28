import { describe, expect, it } from 'vitest'
import { advanceMole, createInitialMoleState, isLocalHammerTurn, registerDodge } from '../src/pages-mole-game/core/game-engine'

describe('mole game hammer / squat mapping', () => {
  it('starts on the local mole and only a local warning can score a dodge', () => {
    let state = createInitialMoleState(2, 3000)
    expect(state.hammerLane).toBe(2)
    expect(isLocalHammerTurn(state, 2)).toBe(true)
    state = registerDodge(state, 2)
    expect(state.dodges).toBe(1)
    expect(state.rounds).toBe(1)
  })

  it('does not score a local dodge while the hammer is on another hole', () => {
    let state = createInitialMoleState(2, 10)
    state = advanceMole(state, 20, 3000, 2) // local miss -> HIT
    state = advanceMole(state, 1000, 3000, 2) // -> NEXT
    state = advanceMole(state, 500, 3000, 2) // -> next lane
    expect(state.hammerLane).not.toBe(2)
    const ignored = registerDodge(state, 2)
    expect(ignored.dodges).toBe(state.dodges)
  })

  it('walks deterministically across five lanes', () => {
    let state = createInitialMoleState(2, 1)
    const lanes = new Set<number>([state.hammerLane])
    for (let i = 0; i < 24; i += 1) {
      state = advanceMole(state, 5000, 1, 2)
      lanes.add(state.hammerLane)
    }
    expect([...lanes].sort()).toEqual([0, 1, 2, 3, 4])
  })
})
