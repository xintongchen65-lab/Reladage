import type { MotionFrame, RepEvent } from '../types/motion'

export type SessionGuardState =
  | 'RUNNING' | 'USER_PAUSED' | 'BACKGROUND_PAUSED' | 'SOURCE_PAUSED'
  | 'RESTING' | 'DATA_INTERRUPTED' | 'REARMING' | 'TERMINAL'
export type ArmSide = 'left' | 'right'
export interface SessionGuardOptions {
  dataTimeoutMs: number
  returnAngleDeg: number
  startedAtMs?: number
}

export function shouldAdvanceGameTimers(state: SessionGuardState, basketOpen: boolean): boolean {
  return !basketOpen && (state === 'RUNNING' || state === 'REARMING')
}

export class SessionGuard {
  private state: SessionGuardState = 'REARMING'
  private readonly startedAtMs: number
  private activeStartedAtMs: number | null
  private accumulatedActiveMs = 0
  private lastValidFrameAtMs: number | null
  private rearmRequired: Record<ArmSide, boolean> = { left: true, right: true }

  constructor(private readonly options: SessionGuardOptions) {
    this.startedAtMs = options.startedAtMs ?? Date.now()
    this.activeStartedAtMs = this.startedAtMs
    this.lastValidFrameAtMs = this.startedAtMs
  }

  acceptFrame(frame: MotionFrame, nowMs = Date.now()): void {
    if (this.state === 'TERMINAL') return
    const priorState = this.state
    this.lastValidFrameAtMs = nowMs
    if (frame.training_state === 'IDLE' || frame.training_state === 'PAUSED') {
      if (this.state !== 'USER_PAUSED' && this.state !== 'BACKGROUND_PAUSED') this.enterInactive('SOURCE_PAUSED', nowMs)
      return
    }
    if (frame.training_state === 'REST') {
      this.enterInactive('RESTING', nowMs)
      this.requireRearm()
      return
    }
    if (frame.training_state === 'FINISHED' || frame.training_state === 'STOPPED') return
    if (priorState === 'DATA_INTERRUPTED' || priorState === 'SOURCE_PAUSED' || priorState === 'RESTING') {
      this.enterRearming(nowMs)
    }
    if (this.state === 'REARMING' || this.state === 'RUNNING') {
      this.updateRearm('left', frame.left_angle_deg)
      this.updateRearm('right', frame.right_angle_deg)
      if (!this.rearmRequired.left && !this.rearmRequired.right) this.state = 'RUNNING'
    }
  }

  checkTimeout(nowMs = Date.now()): boolean {
    if (['TERMINAL', 'USER_PAUSED', 'BACKGROUND_PAUSED', 'SOURCE_PAUSED', 'RESTING', 'DATA_INTERRUPTED'].includes(this.state) || this.lastValidFrameAtMs === null) return false
    if (nowMs - this.lastValidFrameAtMs < this.options.dataTimeoutMs) return false
    this.enterInactive('DATA_INTERRUPTED', this.lastValidFrameAtMs)
    this.requireRearm()
    return true
  }

  pauseByUser(nowMs = Date.now()): void { if (this.state !== 'TERMINAL') { this.enterInactive('USER_PAUSED', nowMs); this.requireRearm() } }
  pauseForBackground(nowMs = Date.now()): void { if (this.state !== 'TERMINAL') { this.enterInactive('BACKGROUND_PAUSED', nowMs); this.requireRearm() } }
  pauseForSource(nowMs = Date.now()): void { if (this.state !== 'TERMINAL') { this.enterInactive('SOURCE_PAUSED', nowMs); this.requireRearm() } }
  resume(nowMs = Date.now()): void {
    if (this.state === 'TERMINAL' || this.state === 'DATA_INTERRUPTED' || this.state === 'RESTING') return
    this.enterRearming(nowMs)
  }
  finish(nowMs = Date.now()): void { if (this.state !== 'TERMINAL') { this.stopActiveClock(nowMs); this.state = 'TERMINAL' } }

  canScore(event: RepEvent): boolean {
    if (this.state !== 'RUNNING' && this.state !== 'REARMING') return false
    if (event === 'left_rep_done') return !this.rearmRequired.left
    if (event === 'right_rep_done') return !this.rearmRequired.right
    return event === 'both_rep_done' && !this.rearmRequired.left && !this.rearmRequired.right
  }
  getState(): SessionGuardState { return this.state }
  needsRearm(side: ArmSide): boolean { return this.rearmRequired[side] }
  getTotalElapsedMs(nowMs = Date.now()): number { return Math.max(0, nowMs - this.startedAtMs) }
  getActiveElapsedMs(nowMs = Date.now()): number {
    return this.accumulatedActiveMs + (this.activeStartedAtMs === null ? 0 : Math.max(0, nowMs - this.activeStartedAtMs))
  }

  private updateRearm(side: ArmSide, angleDeg: number): void {
    if (this.rearmRequired[side] && angleDeg <= this.options.returnAngleDeg) this.rearmRequired[side] = false
  }
  private requireRearm(): void { this.rearmRequired = { left: true, right: true } }
  private enterRearming(nowMs: number): void { this.requireRearm(); this.state = 'REARMING'; this.lastValidFrameAtMs = nowMs; this.startActiveClock(nowMs) }
  private enterInactive(state: SessionGuardState, nowMs: number): void { this.stopActiveClock(nowMs); this.state = state }
  private startActiveClock(nowMs: number): void { if (this.activeStartedAtMs === null) this.activeStartedAtMs = nowMs }
  private stopActiveClock(nowMs: number): void {
    if (this.activeStartedAtMs === null) return
    this.accumulatedActiveMs += Math.max(0, nowMs - this.activeStartedAtMs)
    this.activeStartedAtMs = null
  }
}
