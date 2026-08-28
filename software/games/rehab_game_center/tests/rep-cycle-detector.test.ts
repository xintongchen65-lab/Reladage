import { describe, expect, it } from 'vitest'
import { ElbowRepCycleDetector } from '../src/pages-fruit-game/core/rep-cycle-detector'

const detector = () => new ElbowRepCycleDetector({ validAngleDeg: 60, returnAngleDeg: 20 })
const count = (instance: ElbowRepCycleDetector, angles: number[]) => angles.filter((angle) => instance.update(angle).completed).length

describe('ElbowRepCycleDetector v3 cycle', () => {
  it('requires low-angle arming, valid flexion and a return to low angle', () => {
    expect(count(detector(), [40, 60, 80, 20])).toBe(0)
    expect(count(detector(), [0, 20, 40, 59, 20, 0])).toBe(0)
    expect(count(detector(), [0, 30, 60, 80, 50, 20])).toBe(1)
  })

  it('does not repeat while held high or low and requires a second full cycle', () => {
    const instance = detector()
    expect(count(instance, [0, 30, 60, 80, 20])).toBe(1)
    expect(count(instance, Array(50).fill(20))).toBe(0)
    expect(count(instance, [30, 60, 80, 80, 80])).toBe(0)
    expect(count(instance, [50, 20])).toBe(1)
  })

  it('keeps left and right detectors independent', () => {
    const left = detector()
    const right = detector()
    expect(count(left, [0, 30, 60, 20])).toBe(1)
    expect(count(right, [0, 30, 55, 20])).toBe(0)
    expect(right.getState()).toBe('READY')
  })
})
