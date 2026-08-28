import type { HeldControl } from '../../game-platform/motion/data-source'
import type { ActiveSide, KneeMotionFrame } from '../types/motion'
import { createInitialKneeFrame } from '../types/motion'
import { KneeRepCycleDetector } from '../core/rep-cycle-detector'
import type { PenaltyControllableDataSource } from './contracts'

export interface PenaltyFakeOptions {
  targetCount?: number; targetSets?: number; frameRateHz?: number; targetAngleDeg?: number
  validAngleDeg?: number; returnAngleDeg?: number; restDurationSec?: number
}

export class PenaltyFakeDataSource implements PenaltyControllableDataSource {
  private listeners = new Set<(frame: KneeMotionFrame) => void>()
  private timer: ReturnType<typeof setInterval> | null = null
  private cycleTimer: ReturnType<typeof setInterval> | null = null
  private frame: KneeMotionFrame
  private startedAt = 0
  private restAt = 0
  private afterEmit: 'REST' | 'FINISHED' | 'NEXT_SIDE' | null = null
  private readonly intervalMs: number
  private readonly restDurationSec: number
  private readonly leftDetector: KneeRepCycleDetector
  private readonly rightDetector: KneeRepCycleDetector
  private held: Record<HeldControl, boolean> = { leftFlex: false, leftExtend: false, rightFlex: false, rightExtend: false }
  private extrema = { leftMin: 0, leftMax: 0, rightMin: 0, rightMax: 0 }

  constructor(options: PenaltyFakeOptions = {}) {
    const targetCount = Math.max(1, Math.round(options.targetCount ?? 10))
    const targetSets = Math.max(1, Math.round(options.targetSets ?? 3))
    this.intervalMs = Math.round(1000 / Math.min(50, Math.max(5, options.frameRateHz ?? 25)))
    this.restDurationSec = Math.max(0, Math.round(options.restDurationSec ?? 30))
    this.frame = createInitialKneeFrame(targetCount, targetSets)
    this.frame.target_angle_deg = options.targetAngleDeg ?? 80
    this.frame.valid_angle_deg = options.validAngleDeg ?? 60
    this.frame.return_angle_deg = options.returnAngleDeg ?? 20
    this.leftDetector = new KneeRepCycleDetector(this.frame.valid_angle_deg, this.frame.return_angle_deg)
    this.rightDetector = new KneeRepCycleDetector(this.frame.valid_angle_deg, this.frame.return_angle_deg)
  }
  start(): void { if (!this.timer) { if (!this.startedAt) this.startedAt = Date.now(); this.timer = setInterval(() => this.tick(), this.intervalMs); this.tick() } }
  stop(): void { if (this.timer) clearInterval(this.timer); if (this.cycleTimer) clearInterval(this.cycleTimer); this.timer = null; this.cycleTimer = null; this.clearHeld() }
  reset(): void {
    const { target_count, target_sets, target_angle_deg, valid_angle_deg, return_angle_deg } = this.frame
    this.stop(); this.frame = createInitialKneeFrame(target_count, target_sets)
    Object.assign(this.frame, { target_angle_deg, valid_angle_deg, return_angle_deg }); this.startedAt = 0; this.afterEmit = null; this.resetGroup()
  }
  subscribe(listener: (frame: KneeMotionFrame) => void): () => void { this.listeners.add(listener); return () => this.listeners.delete(listener) }
  setHeldControl(control: HeldControl, pressed: boolean): void { this.held[control] = pressed }
  setPaused(paused: boolean): void {
    if (paused && this.frame.training_state === 'RUNNING') { this.frame.training_state = 'PAUSED'; this.clearHeld(); this.resetRepCycleDetectors() }
    else if (!paused && this.frame.training_state === 'PAUSED') { this.frame.training_state = 'RUNNING'; this.resetRepCycleDetectors() }
  }
  resetRepCycleDetectors(): void { this.leftDetector.reset(); this.rightDetector.reset() }
  simulateCompleteCycle(side: ActiveSide): boolean {
    if (this.cycleTimer || this.frame.training_state !== 'RUNNING' || side !== this.frame.active_side) return false
    const flex = side === 'left' ? 'leftFlex' : 'rightFlex'; const extend = side === 'left' ? 'leftExtend' : 'rightExtend'
    let returning = false
    this.setHeldControl(flex, true)
    this.cycleTimer = setInterval(() => {
      const angle = side === 'left' ? this.frame.left_angle_deg : this.frame.right_angle_deg
      if (!returning && angle >= this.frame.valid_angle_deg + 4) { returning = true; this.setHeldControl(flex, false); this.setHeldControl(extend, true) }
      if (returning && angle <= this.frame.return_angle_deg) {
        this.setHeldControl(extend, false); if (this.cycleTimer) clearInterval(this.cycleTimer); this.cycleTimer = null
      }
    }, this.intervalMs)
    return true
  }
  finishTraining(reason: 'FINISHED' | 'STOPPED'): void {
    if (this.frame.training_state === 'FINISHED' || this.frame.training_state === 'STOPPED') return
    this.clearHeld(); this.frame.training_state = reason
  }
  private tick(): void {
    const startedState = this.frame.training_state
    if (startedState === 'RUNNING') this.updateRunning()
    else if (startedState === 'REST') this.updateRest()
    else { this.frame.rep_event = 'none'; this.frame.left_speed_deg_s = 0; this.frame.right_speed_deg_s = 0 }
    this.frame.seq += 1; this.frame.timestamp_ms = Math.max(0, Date.now() - this.startedAt)
    this.listeners.forEach((listener) => listener({ ...this.frame }))
    const transition = this.afterEmit; this.afterEmit = null
    if (transition === 'NEXT_SIDE') this.frame.active_side = this.frame.active_side === 'left' ? 'right' : 'left'
    else if (transition === 'REST') this.enterRest()
    else if (transition === 'FINISHED') this.frame.training_state = 'FINISHED'
    else if (startedState === 'FINISHED' || startedState === 'STOPPED') this.stop()
    this.frame.rep_event = 'none'
  }
  private updateRunning(): void {
    const priorLeft = this.frame.left_angle_deg; const priorRight = this.frame.right_angle_deg
    this.frame.left_angle_deg = clamp(priorLeft + (Number(this.held.leftFlex) - Number(this.held.leftExtend)) * 2, 0, 120)
    this.frame.right_angle_deg = clamp(priorRight + (Number(this.held.rightFlex) - Number(this.held.rightExtend)) * 2, 0, 120)
    this.frame.left_speed_deg_s = (this.frame.left_angle_deg - priorLeft) * 1000 / this.intervalMs
    this.frame.right_speed_deg_s = (this.frame.right_angle_deg - priorRight) * 1000 / this.intervalMs
    this.updateRom()
    const side = this.frame.active_side
    const detector = side === 'left' ? this.leftDetector : this.rightDetector
    const angle = side === 'left' ? this.frame.left_angle_deg : this.frame.right_angle_deg
    if (!detector.update(angle).completed) { this.frame.rep_event = 'none'; return }
    if (side === 'left') this.frame.left_count = Math.min(this.frame.target_count, this.frame.left_count + 1)
    else this.frame.right_count = Math.min(this.frame.target_count, this.frame.right_count + 1)
    this.frame.rep_event = side === 'left' ? 'left_rep_done' : 'right_rep_done'
    this.updateCompletion()
    if (this.frame.left_count >= this.frame.target_count && this.frame.right_count >= this.frame.target_count) {
      this.afterEmit = this.frame.set_index < this.frame.target_sets ? 'REST' : 'FINISHED'
    } else this.afterEmit = 'NEXT_SIDE'
  }
  private updateCompletion(): void {
    const completed = Math.min(this.frame.left_count, this.frame.right_count)
    this.frame.completion_percent = Math.floor(completed * 100 / this.frame.target_count)
    this.frame.overall_completion_percent = Math.floor(((this.frame.set_index - 1) * this.frame.target_count + completed) * 100 / (this.frame.target_sets * this.frame.target_count))
  }
  private updateRom(): void {
    this.extrema.leftMin = Math.min(this.extrema.leftMin, this.frame.left_angle_deg); this.extrema.leftMax = Math.max(this.extrema.leftMax, this.frame.left_angle_deg)
    this.extrema.rightMin = Math.min(this.extrema.rightMin, this.frame.right_angle_deg); this.extrema.rightMax = Math.max(this.extrema.rightMax, this.frame.right_angle_deg)
    this.frame.left_rom_deg = this.extrema.leftMax - this.extrema.leftMin; this.frame.right_rom_deg = this.extrema.rightMax - this.extrema.rightMin
    this.frame.lr_rom_diff_deg = Math.abs(this.frame.left_rom_deg - this.frame.right_rom_deg)
  }
  private enterRest(): void { this.clearHeld(); this.restAt = Date.now(); this.frame.training_state = 'REST'; this.frame.rest_remaining_sec = this.restDurationSec; this.frame.quality = 'REST'; this.frame.warning = 'resting' }
  private updateRest(): void {
    this.frame.rest_remaining_sec = Math.max(0, this.restDurationSec - Math.floor((Date.now() - this.restAt) / 1000))
    this.frame.rep_event = 'none'; this.frame.left_speed_deg_s = 0; this.frame.right_speed_deg_s = 0
    if (this.frame.rest_remaining_sec === 0) { this.frame.set_index += 1; this.frame.training_state = 'RUNNING'; this.frame.left_count = 0; this.frame.right_count = 0; this.frame.completion_percent = 0; this.frame.active_side = 'left'; this.resetGroup() }
  }
  private resetGroup(): void { this.frame.left_angle_deg = 0; this.frame.right_angle_deg = 0; this.frame.left_rom_deg = 0; this.frame.right_rom_deg = 0; this.frame.lr_rom_diff_deg = 0; this.extrema = { leftMin: 0, leftMax: 0, rightMin: 0, rightMax: 0 }; this.resetRepCycleDetectors(); this.clearHeld() }
  private clearHeld(): void { (Object.keys(this.held) as HeldControl[]).forEach((key) => { this.held[key] = false }) }
}

function clamp(value: number, min: number, max: number): number { return Math.min(max, Math.max(min, value)) }
