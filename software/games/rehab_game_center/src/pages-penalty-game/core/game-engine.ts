import type { ActiveSide, KneeMotionFrame, RepEvent } from '../types/motion'
import type { PenaltyGameMetrics, PenaltyTrainingResult } from '../types/result'
import { totalSideCount } from '../types/motion'

export type ShotOutcome = 'GOAL' | 'SAVE' | 'MISS'
export type ShotDirection = 'left' | 'center' | 'right'
export type PenaltyPhase = 'WAITING_TARGET' | 'POWERING' | 'KICKING' | 'BALL_FLIGHT' | 'FEEDBACK' | 'NEXT_SIDE'
export type RandomSource = () => number

export interface PenaltyGameState extends PenaltyGameMetrics {
  activeSide: ActiveSide
  phase: PenaltyPhase
  phaseRemainingMs: number
  direction: ShotDirection
  outcome: ShotOutcome | null
  feedback: string
  effectId: number
}

export interface TrainingAggregate {
  leftTotalCount: number
  rightTotalCount: number
  leftMaxRomDeg: number
  rightMaxRomDeg: number
  overallCompletionPercent: number
}

export class TrainingAccumulator {
  private value: TrainingAggregate = { leftTotalCount: 0, rightTotalCount: 0, leftMaxRomDeg: 0, rightMaxRomDeg: 0, overallCompletionPercent: 0 }
  accept(frame: KneeMotionFrame): TrainingAggregate {
    this.value.leftTotalCount = Math.max(this.value.leftTotalCount, totalSideCount(frame, 'left'))
    this.value.rightTotalCount = Math.max(this.value.rightTotalCount, totalSideCount(frame, 'right'))
    this.value.leftMaxRomDeg = Math.max(this.value.leftMaxRomDeg, frame.left_rom_deg)
    this.value.rightMaxRomDeg = Math.max(this.value.rightMaxRomDeg, frame.right_rom_deg)
    this.value.overallCompletionPercent = Math.max(this.value.overallCompletionPercent, frame.overall_completion_percent)
    return this.snapshot()
  }
  snapshot(): TrainingAggregate { return { ...this.value } }
}

function normalized(random: RandomSource): number {
  const value = Number(random())
  return Number.isFinite(value) ? Math.max(0, Math.min(0.999999999, value)) : 0
}

export function pickShotOutcome(random: RandomSource = Math.random): ShotOutcome {
  const value = normalized(random)
  return value < 0.7 ? 'GOAL' : value < 0.9 ? 'SAVE' : 'MISS'
}

export function pickShotDirection(random: RandomSource = Math.random): ShotDirection {
  const value = normalized(random)
  return value < 1 / 3 ? 'left' : value < 2 / 3 ? 'center' : 'right'
}

export function createInitialPenaltyState(): PenaltyGameState {
  return { activeSide: 'left', phase: 'WAITING_TARGET', phaseRemainingMs: 0, direction: 'center', outcome: null,
    shots: 0, goals: 0, saves: 0, misses: 0, score: 0, combo: 0, bestCombo: 0,
    feedback: '请使用左腿完成屈伸动作', effectId: 0 }
}

export function applyPower(state: PenaltyGameState, progress: number): PenaltyGameState {
  if (state.phase !== 'WAITING_TARGET' && state.phase !== 'POWERING') return state
  const phase = progress > 0.08 ? 'POWERING' : 'WAITING_TARGET'
  return phase === state.phase ? state : { ...state, phase }
}

export function triggerShot(
  state: PenaltyGameState,
  side: ActiveSide,
  outcomeRandom: RandomSource = Math.random,
  directionRandom: RandomSource = Math.random
): { state: PenaltyGameState; accepted: boolean; wrongSide: boolean } {
  if (state.phase === 'KICKING' || state.phase === 'BALL_FLIGHT' || state.phase === 'FEEDBACK') {
    return { state, accepted: false, wrongSide: false }
  }
  if (side !== state.activeSide) {
    return { state: { ...state, combo: 0, feedback: `当前目标是${state.activeSide === 'left' ? '左腿' : '右腿'}` }, accepted: false, wrongSide: true }
  }
  const outcome = pickShotOutcome(outcomeRandom)
  const direction = pickShotDirection(directionRandom)
  const combo = outcome === 'GOAL' ? state.combo + 1 : 0
  const points = outcome === 'GOAL' ? 100 + (combo - 1) * 20 : 0
  const feedback = outcome === 'GOAL' ? `进球！+${points}` : outcome === 'SAVE' ? '被守门员扑出' : '射偏了，继续加油'
  return {
    accepted: true,
    wrongSide: false,
    state: {
      ...state,
      phase: 'KICKING',
      phaseRemainingMs: 220,
      direction,
      outcome,
      shots: state.shots + 1,
      goals: state.goals + (outcome === 'GOAL' ? 1 : 0),
      saves: state.saves + (outcome === 'SAVE' ? 1 : 0),
      misses: state.misses + (outcome === 'MISS' ? 1 : 0),
      score: state.score + points,
      combo,
      bestCombo: Math.max(state.bestCombo, combo),
      feedback,
      effectId: state.effectId + 1
    }
  }
}

export function advancePenaltyPhase(state: PenaltyGameState, deltaMs: number): PenaltyGameState {
  if (deltaMs <= 0 || state.phase === 'WAITING_TARGET' || state.phase === 'POWERING') return state
  let remaining = state.phaseRemainingMs - deltaMs
  let next = state
  while (remaining <= 0) {
    if (next.phase === 'KICKING') { next = { ...next, phase: 'BALL_FLIGHT', phaseRemainingMs: 480 }; remaining += 480 }
    else if (next.phase === 'BALL_FLIGHT') { next = { ...next, phase: 'FEEDBACK', phaseRemainingMs: 650 }; remaining += 650 }
    else if (next.phase === 'FEEDBACK') { next = { ...next, phase: 'NEXT_SIDE', phaseRemainingMs: 180 }; remaining += 180 }
    else if (next.phase === 'NEXT_SIDE') {
      const activeSide: ActiveSide = next.activeSide === 'left' ? 'right' : 'left'
      return { ...next, activeSide, phase: 'WAITING_TARGET', phaseRemainingMs: 0, outcome: null,
        feedback: `请使用${activeSide === 'left' ? '左腿' : '右腿'}完成屈伸动作` }
    } else return next
  }
  return { ...next, phaseRemainingMs: remaining }
}

export class ShotEventReconciler {
  private counts = { left: 0, right: 0 }
  private credits = { left: 0, right: 0 }
  reset(frame?: KneeMotionFrame): void {
    this.counts.left = frame?.left_count ?? 0; this.counts.right = frame?.right_count ?? 0
    this.credits.left = 0; this.credits.right = 0
  }
  accept(frame: KneeMotionFrame): { side: ActiveSide | null; source: 'event' | 'count' | null; countJump: boolean } {
    const deltas = { left: frame.left_count - this.counts.left, right: frame.right_count - this.counts.right }
    this.counts = { left: frame.left_count, right: frame.right_count }
    if (frame.training_state !== 'RUNNING') return { side: null, source: null, countJump: false }
    const eventSide = eventToSide(frame.rep_event)
    if (eventSide) {
      if (deltas[eventSide] <= 0) this.credits[eventSide] += 1
      return { side: eventSide, source: 'event', countJump: deltas.left > 1 || deltas.right > 1 }
    }
    ;(['left', 'right'] as ActiveSide[]).forEach((candidate) => {
      if (deltas[candidate] > 0 && this.credits[candidate] > 0) {
        const used = Math.min(deltas[candidate], this.credits[candidate])
        deltas[candidate] -= used
        this.credits[candidate] -= used
      }
    })
    const side = frame.active_side
    const delta = deltas[side]
    if (delta <= 0) return { side: null, source: null, countJump: deltas.left > 1 || deltas.right > 1 }
    return delta === 1
      ? { side, source: 'count', countJump: false }
      : { side: null, source: null, countJump: true }
  }
}

function eventToSide(event: RepEvent): ActiveSide | null {
  return event === 'left_rep_done' ? 'left' : event === 'right_rep_done' ? 'right' : null
}

export function createPenaltyResult(
  endReason: 'FINISHED' | 'STOPPED', frame: KneeMotionFrame, state: PenaltyGameState,
  elapsedMs: number, activeElapsedMs: number, aggregate: TrainingAggregate, completedAtMs = Date.now()
): PenaltyTrainingResult {
  return {
    gameId: 'penalty', endReason, elapsedMs: Math.max(0, elapsedMs), activeElapsedMs: Math.max(0, Math.min(activeElapsedMs, elapsedMs)), completedAtMs,
    training: {
      left_count: frame.left_count, right_count: frame.right_count, left_rom_deg: frame.left_rom_deg, right_rom_deg: frame.right_rom_deg,
      lr_rom_diff_deg: frame.lr_rom_diff_deg, target_count: frame.target_count, completion_percent: frame.completion_percent,
      training_state: endReason, set_index: frame.set_index, target_sets: frame.target_sets,
      overall_completion_percent: aggregate.overallCompletionPercent,
      left_total_count: aggregate.leftTotalCount, right_total_count: aggregate.rightTotalCount,
      session_left_rom_deg: aggregate.leftMaxRomDeg, session_right_rom_deg: aggregate.rightMaxRomDeg,
      session_lr_rom_diff_deg: Math.abs(aggregate.leftMaxRomDeg - aggregate.rightMaxRomDeg)
    },
    game: { shots: state.shots, goals: state.goals, saves: state.saves, misses: state.misses, score: state.score, combo: state.combo, bestCombo: state.bestCombo }
  }
}
