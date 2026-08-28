import type { HeldControl } from '../../game-platform/motion/data-source'
import type { VitalityMotionFrame } from '../types/motion'
import { createInitialVitalityFrame } from '../types/motion'
import { VitalityRepCycleDetector } from '../core/rep-cycle-detector'
import type { VitalityControllableDataSource, CycleSimulatableSource } from './contracts'

export interface VitalityFakeOptions { targetCount?: number; targetSets?: number; frameRateHz?: number; targetAngleDeg?: number; validAngleDeg?: number; returnAngleDeg?: number; restDurationSec?: number }

export class VitalityFakeDataSource implements VitalityControllableDataSource, CycleSimulatableSource {
  private listeners = new Set<(frame: VitalityMotionFrame) => void>()
  private timer: ReturnType<typeof setInterval> | null = null
  private frame: VitalityMotionFrame
  private startedAt = 0
  private restStartedAt = 0
  private cycleTimer: ReturnType<typeof setInterval> | null = null
  private pendingTransition: 'REST' | 'FINISHED' | null = null
  private held: Record<HeldControl, boolean> = { leftFlex: false, leftExtend: false, rightFlex: false, rightExtend: false }
  private readonly intervalMs: number
  private readonly restDurationSec: number
  private readonly detector: VitalityRepCycleDetector

  constructor(options: VitalityFakeOptions = {}) {
    this.intervalMs = Math.round(1000 / Math.min(50, Math.max(5, options.frameRateHz ?? 25)))
    this.restDurationSec = Math.max(0, Math.round(options.restDurationSec ?? 30))
    this.frame = createInitialVitalityFrame(Math.max(1, Math.round(options.targetCount ?? 10)), Math.max(1, Math.round(options.targetSets ?? 1)))
    this.frame.target_angle_deg = options.targetAngleDeg ?? 80
    this.frame.valid_angle_deg = options.validAngleDeg ?? 60
    this.frame.return_angle_deg = options.returnAngleDeg ?? 20
    this.detector = new VitalityRepCycleDetector(0.72, 0.12)
  }
  start(): void { if (!this.timer) { if (!this.startedAt) this.startedAt = Date.now(); this.timer = setInterval(() => this.tick(), this.intervalMs); this.tick() } }
  stop(): void { if (this.timer) clearInterval(this.timer); if (this.cycleTimer) clearInterval(this.cycleTimer); this.timer = null; this.cycleTimer = null; this.clearHeld() }
  reset(): void { const { target_count, target_sets, target_angle_deg, valid_angle_deg, return_angle_deg } = this.frame; this.stop(); this.frame = createInitialVitalityFrame(target_count, target_sets); Object.assign(this.frame, { target_angle_deg, valid_angle_deg, return_angle_deg }); this.startedAt = 0; this.pendingTransition = null; this.detector.reset() }
  subscribe(listener: (frame: VitalityMotionFrame) => void): () => void { this.listeners.add(listener); return () => this.listeners.delete(listener) }
  setHeldControl(control: HeldControl, pressed: boolean): void { this.held[control] = pressed }
  setPaused(paused: boolean): void { if (paused && this.frame.training_state === 'RUNNING') { this.frame.training_state = 'PAUSED'; this.clearHeld(); this.detector.reset() } else if (!paused && this.frame.training_state === 'PAUSED') { this.frame.training_state = 'RUNNING'; this.detector.reset() } }
  resetRepCycleDetectors(): void { this.detector.reset() }
  simulateCompleteCycle(): boolean {
    if (this.cycleTimer || this.frame.training_state !== 'RUNNING') return false
    let rising = true
    this.setHeldControl('leftFlex', true)
    this.cycleTimer = setInterval(() => {
      const progress = this.frame.motion_progress ?? 0
      if (rising && progress >= 0.86) { rising = false; this.setHeldControl('leftFlex', false); this.setHeldControl('leftExtend', true) }
      if (!rising && progress <= 0.02) { this.setHeldControl('leftExtend', false); if (this.cycleTimer) clearInterval(this.cycleTimer); this.cycleTimer = null }
    }, this.intervalMs)
    return true
  }
  finishTraining(reason: 'FINISHED' | 'STOPPED'): void { if (this.frame.training_state === 'FINISHED' || this.frame.training_state === 'STOPPED') return; this.clearHeld(); this.frame.training_state = reason }
  private tick(): void {
    const state = this.frame.training_state
    if (state === 'RUNNING') this.updateRunning()
    else if (state === 'REST') this.updateRest()
    else { this.frame.rep_event = 'none'; this.frame.left_speed_deg_s = 0; this.frame.right_speed_deg_s = 0 }
    this.frame.seq += 1; this.frame.timestamp_ms = Math.max(0, Date.now() - this.startedAt)
    this.listeners.forEach((listener) => listener({ ...this.frame }))
    this.frame.rep_event = 'none'
    if (this.pendingTransition === 'REST') { this.pendingTransition = null; this.frame.training_state = 'REST'; this.restStartedAt = 0 }
    else if (this.pendingTransition === 'FINISHED') { this.pendingTransition = null; this.frame.training_state = 'FINISHED' }
    if (state === 'FINISHED' || state === 'STOPPED') this.stop()
  }
  private updateRunning(): void {
    const previous = this.frame.motion_progress ?? 0
    const flex = this.held.leftFlex || this.held.rightFlex
    const extend = this.held.leftExtend || this.held.rightExtend
    const next = Math.max(0, Math.min(1, previous + (flex ? 0.045 : extend ? -0.06 : 0)))
    const speed = (next - previous) * 1000 / this.intervalMs
    this.frame.motion_progress = next
    this.frame.motion_stage = stageForProgress(next, next - previous, this.frame.motion_stage)
    this.frame.left_angle_deg = next * this.frame.target_angle_deg
    this.frame.right_angle_deg = this.frame.left_angle_deg
    this.frame.left_speed_deg_s = speed
    this.frame.right_speed_deg_s = speed
    this.frame.left_rom_deg = Math.max(this.frame.left_rom_deg, this.frame.left_angle_deg)
    this.frame.right_rom_deg = this.frame.left_rom_deg
    this.frame.lr_rom_diff_deg = 0
    const transition = this.detector.update(next)
    if (transition.completed) {
      this.frame.rep_event = 'sit_to_stand_done'
      this.frame.left_count += 1
      this.frame.right_count = this.frame.left_count
      this.frame.completion_percent = Math.min(100, this.frame.left_count / this.frame.target_count * 100)
      this.frame.overall_completion_percent = Math.min(100, ((this.frame.set_index - 1) * this.frame.target_count + this.frame.left_count) / (this.frame.target_count * this.frame.target_sets) * 100)
      if (this.frame.left_count >= this.frame.target_count) {
        if (this.frame.set_index < this.frame.target_sets) this.pendingTransition = 'REST'
        else this.pendingTransition = 'FINISHED'
      }
    }
  }
  private updateRest(): void {
    if (!this.restStartedAt) this.restStartedAt = Date.now()
    const remaining = Math.max(0, this.restDurationSec - Math.floor((Date.now() - this.restStartedAt) / 1000))
    this.frame.rest_remaining_sec = remaining
    this.frame.motion_progress = 0
    this.frame.motion_stage = 'SITTING'
    this.frame.rep_event = 'none'
    if (remaining <= 0) { this.frame.set_index += 1; this.frame.left_count = 0; this.frame.right_count = 0; this.frame.completion_percent = 0; this.frame.rest_remaining_sec = 0; this.frame.training_state = 'RUNNING'; this.restStartedAt = 0; this.detector.reset() }
  }
  private clearHeld(): void { Object.keys(this.held).forEach((key) => { this.held[key as HeldControl] = false }) }
}

function stageForProgress(
  progress: number,
  delta: number,
  previous?: VitalityMotionFrame['motion_stage']
): VitalityMotionFrame['motion_stage'] {
  if (Math.abs(delta) < 0.002 && previous) return previous
  if (delta < 0) {
    if (progress >= 0.82) return 'STANDING'
    if (progress >= 0.56) return 'HALF_STANDING'
    if (progress >= 0.36) return 'LIFT_OFF'
    if (progress >= 0.16) return 'LEAN_FORWARD'
    if (progress > 0.035) return 'SIT_BACK'
    return 'SITTING'
  }
  if (progress <= 0.1) return 'SITTING'
  if (progress <= 0.3) return 'LEAN_FORWARD'
  if (progress <= 0.5) return 'LIFT_OFF'
  if (progress <= 0.8) return 'HALF_STANDING'
  return 'STANDING'
}
