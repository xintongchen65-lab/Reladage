import type { SquatMotionFrame } from '../types/motion'
import type { MoleGameMetrics } from '../types/result'

export type MolePhase = 'WARNING' | 'DODGED' | 'HIT' | 'NEXT'

export interface MoleGameState extends MoleGameMetrics {
  phase: MolePhase
  phaseRemainingMs: number
  hammerLane: number
  turnIndex: number
  feedback: string
  effectId: number
  pendingLocalDodge: boolean
}

const LANE_COUNT = 5
const NPC_WARNING_MS = 1150
const RESULT_MS = 760
const NEXT_MS = 320

export interface MoleTrainingAggregate {
  leftTotalCount: number
  rightTotalCount: number
  leftMaxRomDeg: number
  rightMaxRomDeg: number
  overallCompletionPercent: number
  finalSetIndex: number
}

export class MoleTrainingAccumulator {
  private value: MoleTrainingAggregate = emptyAggregate()
  accept(frame: SquatMotionFrame): MoleTrainingAggregate {
    const priorSets = Math.max(0, frame.set_index - 1) * frame.target_count
    this.value.leftTotalCount = Math.max(this.value.leftTotalCount, priorSets + frame.left_count)
    this.value.rightTotalCount = Math.max(this.value.rightTotalCount, priorSets + frame.right_count)
    this.value.leftMaxRomDeg = Math.max(this.value.leftMaxRomDeg, frame.left_rom_deg)
    this.value.rightMaxRomDeg = Math.max(this.value.rightMaxRomDeg, frame.right_rom_deg)
    this.value.overallCompletionPercent = Math.max(this.value.overallCompletionPercent, frame.overall_completion_percent)
    this.value.finalSetIndex = Math.max(this.value.finalSetIndex, frame.set_index)
    return this.snapshot()
  }
  reset(): void { this.value = emptyAggregate() }
  snapshot(): MoleTrainingAggregate { return { ...this.value } }
}

export interface MoleRepDecision { accepted: boolean; source: 'event' | 'count' | null; countJump: boolean }

export class MoleRepReconciler {
  private setIndex = 1
  private leftCount = 0
  private rightCount = 0
  private eventCredit = 0
  private eventWasPrevious = false
  reset(frame?: SquatMotionFrame): void {
    this.setIndex = frame?.set_index ?? 1
    this.leftCount = frame?.left_count ?? 0
    this.rightCount = frame?.right_count ?? 0
    this.eventCredit = 0
    this.eventWasPrevious = false
  }
  accept(frame: SquatMotionFrame): MoleRepDecision {
    if (frame.set_index !== this.setIndex) {
      this.setIndex = frame.set_index
      this.leftCount = frame.left_count
      this.rightCount = frame.right_count
      this.eventCredit = 0
      this.eventWasPrevious = false
      return { accepted: false, source: null, countJump: false }
    }
    let leftDelta = frame.left_count - this.leftCount
    let rightDelta = frame.right_count - this.rightCount
    this.leftCount = frame.left_count
    this.rightCount = frame.right_count
    const countJump = leftDelta > 1 || rightDelta > 1 || leftDelta !== rightDelta
    if (frame.training_state !== 'RUNNING') {
      this.eventWasPrevious = false
      return { accepted: false, source: null, countJump }
    }
    if (frame.rep_event === 'both_rep_done') {
      if (this.eventWasPrevious) return { accepted: false, source: null, countJump }
      this.eventWasPrevious = true
      if (leftDelta <= 0 && rightDelta <= 0) {
        if (this.eventCredit > 0) return { accepted: false, source: null, countJump }
        this.eventCredit = 1
      }
      return { accepted: true, source: 'event', countJump }
    }
    this.eventWasPrevious = false
    if (leftDelta > 0 && rightDelta > 0 && this.eventCredit > 0) {
      const used = Math.min(leftDelta, rightDelta, this.eventCredit)
      leftDelta -= used
      rightDelta -= used
      this.eventCredit -= used
    }
    if (leftDelta === 1 && rightDelta === 1) return { accepted: true, source: 'count', countJump: false }
    return { accepted: false, source: null, countJump: countJump || leftDelta > 1 || rightDelta > 1 }
  }
}

function emptyAggregate(): MoleTrainingAggregate {
  return { leftTotalCount: 0, rightTotalCount: 0, leftMaxRomDeg: 0, rightMaxRomDeg: 0, overallCompletionPercent: 0, finalSetIndex: 1 }
}

export function createInitialMoleState(localLane = 2, warningWindowMs = 3200): MoleGameState {
  return {
    rounds: 0,
    dodges: 0,
    hits: 0,
    score: 0,
    combo: 0,
    bestCombo: 0,
    coins: 0,
    phase: 'WARNING',
    phaseRemainingMs: warningWindowMs,
    hammerLane: clampLane(localLane),
    turnIndex: 0,
    feedback: '锤子锁定你了，完成一次箱式深蹲躲进洞里！',
    effectId: 0,
    pendingLocalDodge: false
  }
}

/** The player has physically reached the squat/hide position. This is visual only until both_rep_done confirms the full medical rep. */
export function markLocalHidden(state: MoleGameState, localLane = 2): MoleGameState {
  if (state.phase !== 'WARNING' || state.hammerLane !== clampLane(localLane)) return state
  return {
    ...state,
    phase: 'DODGED',
    phaseRemainingMs: 15000,
    feedback: '已躲进洞里，稳稳起身完成本次动作',
    effectId: state.effectId + 1,
    pendingLocalDodge: true
  }
}

/** A valid both_rep_done confirms the pending dodge and awards game points. */
export function registerDodge(state: MoleGameState, localLane = 2): MoleGameState {
  const local = clampLane(localLane)
  const canConfirmPending = state.pendingLocalDodge && state.hammerLane === local
  const canDirect = state.phase === 'WARNING' && state.hammerLane === local
  if (!canConfirmPending && !canDirect) return state
  const combo = state.combo + 1
  return {
    ...state,
    rounds: state.rounds + 1,
    dodges: state.dodges + 1,
    score: state.score + 100 + Math.min(120, combo * 12),
    combo,
    bestCombo: Math.max(state.bestCombo, combo),
    coins: state.coins + 1,
    phase: 'DODGED',
    phaseRemainingMs: RESULT_MS,
    feedback: combo >= 3 ? `完美闪避 · 连击 x${combo}` : '成功躲开！',
    effectId: state.effectId + 1,
    pendingLocalDodge: false
  }
}

/**
 * Advances the hammer independently of the medical counter.
 * Non-local lanes are visual/NPC turns and resolve automatically; local turns wait for both_rep_done.
 */
export function advanceMole(
  state: MoleGameState,
  deltaMs: number,
  warningWindowMs = 3200,
  localLane = 2
): MoleGameState {
  const local = clampLane(localLane)
  const remaining = state.phaseRemainingMs - Math.max(0, deltaMs)
  if (remaining > 0) return { ...state, phaseRemainingMs: remaining }

  if (state.phase === 'WARNING') {
    if (state.hammerLane === local) {
      return {
        ...state,
        rounds: state.rounds + 1,
        hits: state.hits + 1,
        combo: 0,
        phase: 'HIT',
        phaseRemainingMs: RESULT_MS,
        feedback: '被玩具锤碰到了，下一次看到锤子就稳稳下蹲',
        effectId: state.effectId + 1,
        pendingLocalDodge: false
      }
    }
    return {
      ...state,
      phase: 'DODGED',
      phaseRemainingMs: RESULT_MS * 0.72,
      feedback: `${state.hammerLane + 1}号地鼠躲开了`,
      effectId: state.effectId + 1
    }
  }

  if (state.phase === 'DODGED' && state.pendingLocalDodge) {
    // The user reached the safe squat position but never completed/confirmed the full rep.
    return {
      ...state,
      rounds: state.rounds + 1,
      hits: state.hits + 1,
      combo: 0,
      phase: 'HIT',
      phaseRemainingMs: RESULT_MS,
      feedback: '已下蹲但本次动作未完整确认，请按训练节奏完成起身',
      effectId: state.effectId + 1,
      pendingLocalDodge: false
    }
  }

  if (state.phase === 'DODGED' || state.phase === 'HIT') {
    return { ...state, phase: 'NEXT', phaseRemainingMs: NEXT_MS, feedback: '锤子正在寻找下一个洞口…' }
  }

  const nextTurn = state.turnIndex + 1
  const nextLane = nextHammerLane(state.hammerLane, nextTurn)
  const isLocal = nextLane === local
  return {
    ...state,
    phase: 'WARNING',
    phaseRemainingMs: isLocal ? warningWindowMs : NPC_WARNING_MS,
    hammerLane: nextLane,
    turnIndex: nextTurn,
    feedback: isLocal ? '锤子锁定你了，快下蹲躲避！' : `锤子移向 ${nextLane + 1} 号洞口`,
    pendingLocalDodge: false
  }
}

export function isLocalHammerTurn(state: MoleGameState, localLane: number): boolean {
  return state.phase === 'WARNING' && state.hammerLane === clampLane(localLane)
}

export function npcHideProgress(state: MoleGameState, lane: number, localLane: number): number {
  if (lane === clampLane(localLane) || lane !== state.hammerLane) return 0
  if (state.phase === 'HIT') return 0
  if (state.phase === 'DODGED') return 1
  if (state.phase !== 'WARNING') return 0
  return Math.max(0, Math.min(1, 1 - state.phaseRemainingMs / NPC_WARNING_MS))
}

function nextHammerLane(current: number, _turnIndex: number): number {
  // Step by two around five lanes: 2→4→1→3→0→2. This visits every hole before repeating.
  return (clampLane(current) + 2) % LANE_COUNT
}

function clampLane(lane: number): number {
  return Math.max(0, Math.min(LANE_COUNT - 1, Math.round(lane)))
}
