import { describe, expect, it } from 'vitest'
import { mapAngleToProgress, progressToPose } from '../src/pages-fruit-game/core/motion-mapper'

describe('motion mapper', () => {
  it('maps increasing v3 flexion angle to a higher arm pose and clamps boundaries', () => {
    expect(mapAngleToProgress(-10)).toBe(0)
    expect(mapAngleToProgress(0)).toBe(0)
    expect(mapAngleToProgress(40)).toBe(0.5)
    expect(mapAngleToProgress(80)).toBe(1)
    expect(mapAngleToProgress(180)).toBe(1)
  })

  it('uses the previous pose as hysteresis around pose thresholds', () => {
    expect(progressToPose(0.1)).toBe('low')
    expect(progressToPose(0.5)).toBe('mid')
    expect(progressToPose(0.9)).toBe('high')
    expect(progressToPose(0.37, 'low')).toBe('low')
    expect(progressToPose(0.38, 'mid')).toBe('mid')
    expect(progressToPose(0.67, 'mid')).toBe('mid')
    expect(progressToPose(0.67, 'high')).toBe('high')
  })

  it('does not oscillate when progress jitters around the four hysteresis boundaries', () => {
    let pose = 'low' as const | 'mid' | 'high'
    for (const progress of [0.39, 0.4, 0.38, 0.41, 0.39]) pose = progressToPose(progress, pose)
    expect(pose).toBe('mid')

    for (const progress of [0.27, 0.26, 0.25]) pose = progressToPose(progress, pose)
    expect(pose).toBe('low')

    for (const progress of [0.4, 0.73, 0.75, 0.72, 0.61]) pose = progressToPose(progress, pose)
    expect(pose).toBe('high')

    for (const progress of [0.62, 0.6]) pose = progressToPose(progress, pose)
    expect(pose).toBe('mid')
  })

  it('changes pose only twice during one monotonic flexion', () => {
    let pose = 'low' as const | 'mid' | 'high'
    let changes = 0
    for (let progress = 0; progress <= 1; progress += 0.02) {
      const next = progressToPose(progress, pose)
      if (next !== pose) changes += 1
      pose = next
    }
    expect(changes).toBe(2)
    expect(pose).toBe('high')
  })
})
