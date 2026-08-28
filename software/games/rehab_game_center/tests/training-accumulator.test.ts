import { describe, expect, it } from 'vitest'
import { TrainingAccumulator } from '../src/pages-fruit-game/core/game-engine'
import { createInitialMotionFrame } from '../src/pages-fruit-game/types/motion'

describe('TrainingAccumulator', () => {
  it('keeps total counts and maximum ROM across v3 group resets', () => {
    const accumulator = new TrainingAccumulator()
    accumulator.accept({ ...createInitialMotionFrame(10, 3), left_count: 10, right_count: 10, left_rom_deg: 72, right_rom_deg: 68, completion_percent: 100, overall_completion_percent: 33 })
    const aggregate = accumulator.accept({ ...createInitialMotionFrame(10, 3), set_index: 2, left_count: 2, right_count: 1, left_rom_deg: 65, right_rom_deg: 66, overall_completion_percent: 36 })
    expect(aggregate.leftTotalCount).toBe(12); expect(aggregate.rightTotalCount).toBe(11)
    expect(aggregate.leftMaxRomDeg).toBe(72); expect(aggregate.rightMaxRomDeg).toBe(68)
  })
})
