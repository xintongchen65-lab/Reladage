import type { VitalityMotionFrame } from '../types/motion'
import type { VitalityGameMetrics, VitalityTrainingResult } from '../types/result'
import { vitalityCount } from '../types/motion'
export interface VitalityTrainingAggregate { totalCount: number; maxRomDeg: number; overallCompletionPercent: number; finalSetIndex: number }
export class VitalityTrainingAccumulator {
  private value: VitalityTrainingAggregate = { totalCount: 0, maxRomDeg: 0, overallCompletionPercent: 0, finalSetIndex: 1 }
  accept(frame: VitalityMotionFrame): VitalityTrainingAggregate {
    this.value.totalCount = Math.max(this.value.totalCount, vitalityCount(frame))
    this.value.maxRomDeg = Math.max(this.value.maxRomDeg, frame.left_rom_deg, frame.right_rom_deg)
    this.value.overallCompletionPercent = Math.max(this.value.overallCompletionPercent, frame.overall_completion_percent)
    this.value.finalSetIndex = Math.max(this.value.finalSetIndex, frame.set_index)
    return this.snapshot()
  }
  reset(): void { this.value = { totalCount: 0, maxRomDeg: 0, overallCompletionPercent: 0, finalSetIndex: 1 } }
  snapshot(): VitalityTrainingAggregate { return { ...this.value } }
}

export interface VitalityRepDecision { accepted: boolean; countJump: boolean }
export class VitalityRepReconciler {
  private setIndex = 1
  private count = 0
  private eventCredit = 0
  private eventWasPrevious = false
  reset(frame?: VitalityMotionFrame): void {
    this.setIndex = frame?.set_index ?? 1
    this.count = frame ? Math.max(frame.left_count, frame.right_count) : 0
    this.eventCredit = 0
    this.eventWasPrevious = false
  }
  accept(frame: VitalityMotionFrame): VitalityRepDecision {
    const count = Math.max(frame.left_count, frame.right_count)
    if (frame.set_index !== this.setIndex) {
      this.setIndex = frame.set_index
      this.count = count
      this.eventCredit = 0
      this.eventWasPrevious = false
      return { accepted: false, countJump: false }
    }
    let delta = count - this.count
    this.count = count
    const countJump = delta > 1
    if (frame.training_state !== 'RUNNING') {
      this.eventWasPrevious = false
      return { accepted: false, countJump }
    }
    if (frame.rep_event === 'sit_to_stand_done') {
      if (this.eventWasPrevious) return { accepted: false, countJump }
      this.eventWasPrevious = true
      if (delta <= 0) {
        if (this.eventCredit > 0) return { accepted: false, countJump }
        this.eventCredit = 1
      }
      return { accepted: true, countJump }
    }
    this.eventWasPrevious = false
    if (delta > 0 && this.eventCredit > 0) {
      const used = Math.min(delta, this.eventCredit)
      delta -= used
      this.eventCredit -= used
    }
    return { accepted: delta === 1, countJump: countJump || delta > 1 }
  }
}

type VitalityAccumulatorLike = Pick<VitalityTrainingAccumulator, 'accept' | 'snapshot'>

export const VITALITY_EVENTS = ['bird', 'flowers', 'lamp', 'butterfly', 'fountain', 'kite', 'dog', 'flags', 'rainbow', 'celebration'] as const
export type VitalityEvent = typeof VITALITY_EVENTS[number]
export interface VitalityGameState extends VitalityGameMetrics { activatedEvents: VitalityEvent[]; feedback: string; effectId: number; activeState: VitalityMotionFrame['training_state']; totalCount: number }
export interface VitalityTransition { state: VitalityGameState; accepted: boolean; event?: VitalityEvent }

export function createInitialVitalityState(): VitalityGameState { return { activatedEventCount: 0, vitalityValue: 0, combo: 0, bestCombo: 0, celebration: false, activatedEvents: [], feedback: '请坐稳，准备开始训练', effectId: 0, activeState: 'RUNNING', totalCount: 0 } }

export function reduceVitalityFrame(state: VitalityGameState, frame: VitalityMotionFrame, accumulator: VitalityAccumulatorLike, eventAccepted: boolean): VitalityTransition {
  const nextState = { ...state, activeState: frame.training_state, totalCount: Math.max(state.totalCount, vitalityCount(frame)) }
  accumulator.accept(frame)
  if (!eventAccepted || frame.training_state !== 'RUNNING' || frame.rep_event !== 'sit_to_stand_done') return { state: nextState, accepted: false }
  const index = Math.min(VITALITY_EVENTS.length - 1, nextState.activatedEventCount)
  if (nextState.activatedEventCount >= VITALITY_EVENTS.length) return { state: { ...nextState, combo: nextState.combo + 1, bestCombo: Math.max(nextState.bestCombo, nextState.combo + 1), feedback: '公园活力满满！继续保持', effectId: nextState.effectId + 1 }, accepted: true }
  const event = VITALITY_EVENTS[index]
  const combo = nextState.combo + 1
  return { state: { ...nextState, activatedEventCount: nextState.activatedEventCount + 1, vitalityValue: Math.min(100, (nextState.activatedEventCount + 1) * 10), combo, bestCombo: Math.max(nextState.bestCombo, combo), celebration: event === 'celebration' || nextState.celebration, activatedEvents: [...nextState.activatedEvents, event], feedback: event === 'celebration' ? '公园庆祝开始！' : '活力提升！', effectId: nextState.effectId + 1 }, accepted: true, event }
}

export function createVitalityResult(endReason: 'FINISHED' | 'STOPPED', frame: VitalityMotionFrame, state: VitalityGameState, accumulator: VitalityAccumulatorLike, elapsedMs: number, activeElapsedMs: number, completedAtMs = Date.now()): VitalityTrainingResult {
  const aggregate = accumulator.snapshot()
  return { gameId: 'vitality-park', endReason, elapsedMs: Math.max(0, elapsedMs), activeElapsedMs: Math.max(0, Math.min(elapsedMs, activeElapsedMs)), completedAtMs, training: { left_count: frame.left_count, right_count: frame.right_count, left_rom_deg: frame.left_rom_deg, right_rom_deg: frame.right_rom_deg, lr_rom_diff_deg: frame.lr_rom_diff_deg, target_count: frame.target_count, completion_percent: frame.completion_percent, training_state: endReason, set_index: aggregate.finalSetIndex, target_sets: frame.target_sets, overall_completion_percent: aggregate.overallCompletionPercent, total_count: aggregate.totalCount, max_rom_deg: aggregate.maxRomDeg, total_sets: frame.target_sets }, game: { activatedEventCount: state.activatedEventCount, vitalityValue: state.vitalityValue, combo: state.combo, bestCombo: state.bestCombo, celebration: state.celebration } }
}
