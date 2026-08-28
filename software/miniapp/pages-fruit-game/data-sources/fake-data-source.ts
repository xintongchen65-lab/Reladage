import type {
  ControllableMotionDataSource,
  HeldControl,
  MotionFrameListener
} from './contracts'
import type { MotionFrame, TrainingState } from '../types/motion'
import {
  DEFAULT_RETURN_ANGLE_DEG,
  DEFAULT_TARGET_ANGLE_DEG,
  DEFAULT_TARGET_COUNT,
  DEFAULT_TARGET_SETS,
  DEFAULT_VALID_ANGLE_DEG,
  createInitialMotionFrame
} from '../types/motion'
import { ElbowRepCycleDetector } from '../core/rep-cycle-detector'

export interface FakeDataSourceOptions {
  targetCount?: number
  targetSets?: number
  frameRateHz?: number
  targetAngleDeg?: number
  validAngleDeg?: number
  returnAngleDeg?: number
  restDurationSec?: number
}

const ANGLE_MIN = 0
const ANGLE_MAX = 120
const ANGLE_STEP = 2

type AfterRepTransition = 'REST' | 'FINISHED' | null

export class FakeDataSource implements ControllableMotionDataSource {
  private readonly listeners = new Set<MotionFrameListener>()
  private readonly targetCount: number
  private readonly targetSets: number
  private readonly restDurationSec: number
  private readonly intervalMs: number
  private readonly leftDetector: ElbowRepCycleDetector
  private readonly rightDetector: ElbowRepCycleDetector
  private timer: ReturnType<typeof setInterval> | null = null
  private frame: MotionFrame
  private startedAtMs = 0
  private restStartedAtMs = 0
  private afterRepTransition: AfterRepTransition = null
  private leftMin = ANGLE_MIN
  private leftMax = ANGLE_MIN
  private rightMin = ANGLE_MIN
  private rightMax = ANGLE_MIN
  private held: Record<HeldControl, boolean> = {
    leftFlex: false,
    leftExtend: false,
    rightFlex: false,
    rightExtend: false
  }

  constructor(options: FakeDataSourceOptions = {}) {
    this.targetCount = Math.max(1, Math.round(options.targetCount ?? DEFAULT_TARGET_COUNT))
    this.targetSets = Math.max(1, Math.round(options.targetSets ?? DEFAULT_TARGET_SETS))
    const frameRateHz = Math.min(50, Math.max(5, options.frameRateHz ?? 25))
    this.intervalMs = Math.round(1000 / frameRateHz)
    this.restDurationSec = Math.max(0, Math.round(options.restDurationSec ?? 30))
    this.frame = createInitialMotionFrame(this.targetCount, this.targetSets)
    this.frame.target_angle_deg = options.targetAngleDeg ?? DEFAULT_TARGET_ANGLE_DEG
    this.frame.valid_angle_deg = options.validAngleDeg ?? DEFAULT_VALID_ANGLE_DEG
    this.frame.return_angle_deg = options.returnAngleDeg ?? DEFAULT_RETURN_ANGLE_DEG
    const detectorOptions = {
      validAngleDeg: this.frame.valid_angle_deg,
      returnAngleDeg: this.frame.return_angle_deg
    }
    this.leftDetector = new ElbowRepCycleDetector(detectorOptions)
    this.rightDetector = new ElbowRepCycleDetector(detectorOptions)
  }

  start(): void {
    if (this.timer) return
    if (!this.startedAtMs) this.startedAtMs = Date.now()
    this.timer = setInterval(() => this.tick(), this.intervalMs)
    this.tick()
  }

  stop(): void {
    if (this.timer) clearInterval(this.timer)
    this.timer = null
    this.clearHeld()
  }

  reset(): void {
    this.stop()
    const targetAngleDeg = this.frame.target_angle_deg
    const validAngleDeg = this.frame.valid_angle_deg
    const returnAngleDeg = this.frame.return_angle_deg
    this.frame = createInitialMotionFrame(this.targetCount, this.targetSets)
    Object.assign(this.frame, { target_angle_deg: targetAngleDeg, valid_angle_deg: validAngleDeg, return_angle_deg: returnAngleDeg })
    this.startedAtMs = 0
    this.restStartedAtMs = 0
    this.afterRepTransition = null
    this.resetGroupMotion()
  }

  subscribe(listener: MotionFrameListener): () => void {
    this.listeners.add(listener)
    return () => this.listeners.delete(listener)
  }

  setHeldControl(control: HeldControl, pressed: boolean): void {
    this.held[control] = pressed
  }

  setPaused(paused: boolean): void {
    if (paused && this.frame.training_state === 'RUNNING') {
      this.frame.training_state = 'PAUSED'
      this.clearHeld()
      this.resetRepCycleDetectors()
    } else if (!paused && this.frame.training_state === 'PAUSED') {
      this.frame.training_state = 'RUNNING'
      this.resetRepCycleDetectors()
    }
  }

  resetRepCycleDetectors(): void {
    this.leftDetector.resetToWaitReturn()
    this.rightDetector.resetToWaitReturn()
  }

  private tick(): void {
    const stateAtStart = this.frame.training_state
    if (stateAtStart === 'RUNNING') this.updateRunningFrame()
    else if (stateAtStart === 'REST') this.updateRestFrame()
    else {
      this.frame.left_speed_deg_s = 0
      this.frame.right_speed_deg_s = 0
      this.frame.rep_event = 'none'
    }

    this.frame.seq += 1
    this.frame.timestamp_ms = Math.max(0, Date.now() - this.startedAtMs)
    this.emit({ ...this.frame })

    if (this.afterRepTransition) {
      const transition = this.afterRepTransition
      this.afterRepTransition = null
      if (transition === 'REST') this.enterRest()
      else this.frame.training_state = 'FINISHED'
    } else if (stateAtStart === 'FINISHED' || stateAtStart === 'STOPPED') {
      this.stop()
    }
    this.frame.rep_event = 'none'
  }

  private updateRunningFrame(): void {
    const previousLeft = this.frame.left_angle_deg
    const previousRight = this.frame.right_angle_deg
    const leftDirection = Number(this.held.leftFlex) - Number(this.held.leftExtend)
    const rightDirection = Number(this.held.rightFlex) - Number(this.held.rightExtend)
    this.frame.left_angle_deg = this.clampAngle(previousLeft + leftDirection * ANGLE_STEP)
    this.frame.right_angle_deg = this.clampAngle(previousRight + rightDirection * ANGLE_STEP)
    this.frame.left_speed_deg_s = (this.frame.left_angle_deg - previousLeft) * (1000 / this.intervalMs)
    this.frame.right_speed_deg_s = (this.frame.right_angle_deg - previousRight) * (1000 / this.intervalMs)
    this.updateRom()
    this.applyAutomaticEvents()
    this.frame.quality = this.frame.left_rom_deg < this.frame.valid_angle_deg || this.frame.right_rom_deg < this.frame.valid_angle_deg
      ? 'ROM_LOW'
      : Math.abs(this.frame.left_count - this.frame.right_count) >= 2 ? 'ASYMMETRY' : 'GOOD'
    this.frame.warning = this.frame.quality === 'ROM_LOW'
      ? 'range_too_small'
      : this.frame.quality === 'ASYMMETRY' ? 'left_right_asymmetry' : 'none'
  }

  private updateRestFrame(): void {
    const elapsedSec = Math.floor((Date.now() - this.restStartedAtMs) / 1000)
    this.frame.rest_remaining_sec = Math.max(0, this.restDurationSec - elapsedSec)
    this.frame.left_speed_deg_s = 0
    this.frame.right_speed_deg_s = 0
    this.frame.rep_event = 'none'
    this.frame.quality = 'REST'
    this.frame.warning = 'resting'
    if (this.frame.rest_remaining_sec === 0) this.startNextSet()
  }

  private applyAutomaticEvents(): void {
    const leftCompleted = this.leftDetector.update(this.frame.left_angle_deg).completed
    const rightCompleted = this.rightDetector.update(this.frame.right_angle_deg).completed
    if (!leftCompleted && !rightCompleted) {
      this.frame.rep_event = 'none'
      return
    }
    if (leftCompleted) this.frame.left_count = Math.min(this.targetCount, this.frame.left_count + 1)
    if (rightCompleted) this.frame.right_count = Math.min(this.targetCount, this.frame.right_count + 1)
    this.frame.rep_event = leftCompleted && rightCompleted
      ? 'both_rep_done'
      : leftCompleted ? 'left_rep_done' : 'right_rep_done'
    this.updateCompletion()
    if (this.frame.left_count >= this.targetCount && this.frame.right_count >= this.targetCount) {
      this.afterRepTransition = this.frame.set_index < this.targetSets ? 'REST' : 'FINISHED'
    }
  }

  private updateCompletion(): void {
    const slowerCount = Math.min(this.frame.left_count, this.frame.right_count)
    this.frame.completion_percent = Math.min(100, Math.floor((slowerCount * 100) / this.targetCount))
    const completedBefore = (this.frame.set_index - 1) * this.targetCount
    this.frame.overall_completion_percent = Math.min(
      100,
      Math.floor(((completedBefore + slowerCount) * 100) / (this.targetSets * this.targetCount))
    )
  }

  private enterRest(): void {
    this.clearHeld()
    this.restStartedAtMs = Date.now()
    this.frame.training_state = 'REST'
    this.frame.rest_remaining_sec = this.restDurationSec
    this.frame.quality = 'REST'
    this.frame.warning = 'resting'
  }

  private startNextSet(): void {
    this.frame.set_index += 1
    this.frame.training_state = 'RUNNING'
    this.frame.rest_remaining_sec = 0
    this.frame.left_count = 0
    this.frame.right_count = 0
    this.frame.completion_percent = 0
    this.frame.quality = 'ROM_LOW'
    this.frame.warning = 'range_too_small'
    this.resetGroupMotion()
  }

  private resetGroupMotion(): void {
    this.frame.left_angle_deg = ANGLE_MIN
    this.frame.right_angle_deg = ANGLE_MIN
    this.frame.left_rom_deg = 0
    this.frame.right_rom_deg = 0
    this.frame.lr_rom_diff_deg = 0
    this.frame.left_speed_deg_s = 0
    this.frame.right_speed_deg_s = 0
    this.leftMin = this.leftMax = ANGLE_MIN
    this.rightMin = this.rightMax = ANGLE_MIN
    this.resetRepCycleDetectors()
    this.clearHeld()
  }

  private updateRom(): void {
    this.leftMin = Math.min(this.leftMin, this.frame.left_angle_deg)
    this.leftMax = Math.max(this.leftMax, this.frame.left_angle_deg)
    this.rightMin = Math.min(this.rightMin, this.frame.right_angle_deg)
    this.rightMax = Math.max(this.rightMax, this.frame.right_angle_deg)
    this.frame.left_rom_deg = this.leftMax - this.leftMin
    this.frame.right_rom_deg = this.rightMax - this.rightMin
    this.frame.lr_rom_diff_deg = Math.abs(this.frame.left_rom_deg - this.frame.right_rom_deg)
  }

  private clampAngle(value: number): number {
    return Math.min(ANGLE_MAX, Math.max(ANGLE_MIN, value))
  }

  private emit(frame: MotionFrame): void {
    this.listeners.forEach((listener) => listener(frame))
  }

  private clearHeld(): void {
    Object.keys(this.held).forEach((key) => { this.held[key as HeldControl] = false })
  }
}
