import { describe, expect, it } from 'vitest'
import { VitalityRepCycleDetector } from '../src/pages-vitality-park/core/rep-cycle-detector'
import { createInitialVitalityState, reduceVitalityFrame, VITALITY_EVENTS, VitalityTrainingAccumulator } from '../src/pages-vitality-park/core/game-engine'
import { createInitialVitalityFrame } from '../src/pages-vitality-park/types/motion'
import { VitalityMotionFrameAdapter } from '../src/pages-vitality-park/core/motion-adapter'
import { decidePoseFromFrame, poseForProgress } from '../src/pages-vitality-park/core/pose-mapper'

describe('vitality park motion cycle', () => {
  it('requires sitting -> standing -> sitting before one completion', () => {
    const detector = new VitalityRepCycleDetector(.72, .12)
    expect(detector.update(0).completed).toBe(false)
    expect(detector.update(.8).completed).toBe(false)
    expect(detector.update(.9).completed).toBe(false)
    expect(detector.update(.05).completed).toBe(false)
    expect(detector.update(.05).completed).toBe(true)
    expect(detector.update(.05).completed).toBe(false)
  })


  it('replays sit-to-stand poses in reverse while descending', () => {
    expect(poseForProgress(0.9, 'rising')).toBe('standing')
    expect(poseForProgress(0.65, 'descending')).toBe('half-standing')
    expect(poseForProgress(0.42, 'descending')).toBe('lift-off')
    expect(poseForProgress(0.22, 'descending')).toBe('lean-forward')
    expect(poseForProgress(0.08, 'descending')).toBe('sit-back')
    expect(poseForProgress(0, 'descending')).toBe('sitting')

    const frame = { ...createInitialVitalityFrame(), motion_progress: 0.62 }
    const decision = decidePoseFromFrame(frame, 0.8, 'rising')
    expect(decision.direction).toBe('descending')
    expect(decision.pose).toBe('half-standing')
  })

  it('rejects REST events and terminal frames', () => {
    const adapter = new VitalityMotionFrameAdapter()
    const first = createInitialVitalityFrame()
    expect(adapter.ingest(first).accepted).toBe(true)
    const rest = { ...first, seq: 1, training_state: 'REST' as const, rep_event: 'sit_to_stand_done' as const }
    expect(adapter.ingest(rest)).toEqual({ accepted: false, reason: 'invalid_rest_event' })
    const finished = { ...first, seq: 1, training_state: 'FINISHED' as const, rep_event: 'none' as const }
    expect(adapter.ingest(finished).accepted).toBe(true)
    expect(adapter.ingest({ ...finished, seq: 2 })).toEqual({ accepted: false, reason: 'after_terminal' })
  })

  it('unlocks one park event per accepted completion', () => {
    const state = createInitialVitalityState()
    const accumulator = new VitalityTrainingAccumulator()
    let current = state
    let frame = createInitialVitalityFrame()
    for (let i = 0; i < VITALITY_EVENTS.length; i += 1) {
      frame = { ...frame, left_count: i + 1, right_count: i + 1, completion_percent: (i + 1) * 10, overall_completion_percent: (i + 1) * 10, rep_event: 'sit_to_stand_done', seq: i + 1 }
      current = reduceVitalityFrame(current, frame, accumulator, true).state
    }
    expect(current.activatedEventCount).toBe(10)
    expect(current.vitalityValue).toBe(100)
    expect(current.celebration).toBe(true)
    expect(current.activatedEvents).toEqual([...VITALITY_EVENTS])
  })
})
