import type { ActiveSide, BaseMotionFrame, RepEvent } from './types'

export type SessionGuardState = 'RUNNING' | 'USER_PAUSED' | 'BACKGROUND_PAUSED' | 'SOURCE_PAUSED' |
  'RESTING' | 'DATA_INTERRUPTED' | 'REARMING' | 'TERMINAL'

export class MotionSessionGuard<TFrame extends BaseMotionFrame = BaseMotionFrame> {
  private state: SessionGuardState = 'REARMING'
  private readonly startedAt: number
  private activeAt: number | null
  private accumulatedActiveMs = 0
  private lastFrameAt: number | null
  private rearm: Record<ActiveSide, boolean> = { left: true, right: true }
  constructor(private readonly options: { dataTimeoutMs: number; returnAngleDeg: number; startedAtMs?: number }) {
    this.startedAt = options.startedAtMs ?? Date.now(); this.activeAt = this.startedAt; this.lastFrameAt = this.startedAt
  }
  acceptFrame(frame: TFrame, now = Date.now()): void {
    if (this.state === 'TERMINAL') return
    const prior = this.state; this.lastFrameAt = now
    if (frame.training_state === 'IDLE' || frame.training_state === 'PAUSED') { if (!['USER_PAUSED', 'BACKGROUND_PAUSED'].includes(this.state)) this.inactive('SOURCE_PAUSED', now); return }
    if (frame.training_state === 'REST') { this.inactive('RESTING', now); this.requireRearm(); return }
    if (frame.training_state === 'FINISHED' || frame.training_state === 'STOPPED') return
    if (['DATA_INTERRUPTED', 'SOURCE_PAUSED', 'RESTING'].includes(prior)) this.enterRearming(now)
    if (this.state === 'REARMING' || this.state === 'RUNNING') {
      if (this.rearm.left && frame.left_angle_deg <= this.options.returnAngleDeg) this.rearm.left = false
      if (this.rearm.right && frame.right_angle_deg <= this.options.returnAngleDeg) this.rearm.right = false
      if (!this.rearm.left && !this.rearm.right) this.state = 'RUNNING'
    }
  }
  canScore(event: RepEvent): boolean {
    if (!['RUNNING', 'REARMING'].includes(this.state)) return false
    if (event === 'left_rep_done') return !this.rearm.left
    if (event === 'right_rep_done') return !this.rearm.right
    return (event === 'both_rep_done' || event === 'sit_to_stand_done') && !this.rearm.left && !this.rearm.right
  }
  checkTimeout(now = Date.now()): boolean {
    if (['TERMINAL', 'USER_PAUSED', 'BACKGROUND_PAUSED', 'SOURCE_PAUSED', 'RESTING', 'DATA_INTERRUPTED'].includes(this.state) || this.lastFrameAt === null) return false
    if (now - this.lastFrameAt < this.options.dataTimeoutMs) return false
    this.inactive('DATA_INTERRUPTED', this.lastFrameAt); this.requireRearm(); return true
  }
  pauseByUser(now = Date.now()): void { if (this.state !== 'TERMINAL') { this.inactive('USER_PAUSED', now); this.requireRearm() } }
  pauseForBackground(now = Date.now()): void { if (this.state !== 'TERMINAL') { this.inactive('BACKGROUND_PAUSED', now); this.requireRearm() } }
  pauseForSource(now = Date.now()): void { if (this.state !== 'TERMINAL') { this.inactive('SOURCE_PAUSED', now); this.requireRearm() } }
  resume(now = Date.now()): void { if (!['TERMINAL', 'DATA_INTERRUPTED', 'RESTING'].includes(this.state)) this.enterRearming(now) }
  finish(now = Date.now()): void { if (this.state !== 'TERMINAL') { this.stopClock(now); this.state = 'TERMINAL' } }
  getState(): SessionGuardState { return this.state }
  needsRearm(side: ActiveSide): boolean { return this.rearm[side] }
  getTotalElapsedMs(now = Date.now()): number { return Math.max(0, now - this.startedAt) }
  getActiveElapsedMs(now = Date.now()): number { return this.accumulatedActiveMs + (this.activeAt === null ? 0 : Math.max(0, now - this.activeAt)) }
  private requireRearm(): void { this.rearm = { left: true, right: true } }
  private enterRearming(now: number): void { this.requireRearm(); this.state = 'REARMING'; this.lastFrameAt = now; if (this.activeAt === null) this.activeAt = now }
  private inactive(state: SessionGuardState, now: number): void { this.stopClock(now); this.state = state }
  private stopClock(now: number): void { if (this.activeAt !== null) { this.accumulatedActiveMs += Math.max(0, now - this.activeAt); this.activeAt = null } }
}
