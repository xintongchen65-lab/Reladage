export type RepCycleState = 'WAIT_RETURN' | 'READY' | 'FLEXING' | 'RETURNING'

export interface RepCycleDetectorOptions {
  validAngleDeg: number
  returnAngleDeg: number
}

export interface RepCycleUpdate {
  state: RepCycleState
  completed: boolean
  needsReturn: boolean
}

// Retained for callers from earlier V1 builds; v3 itself counts on threshold crossing.
export function requiredStableFrames(stableDurationMs: number, frameRateHz: number): number {
  const frameDurationMs = 1000 / Math.max(1, frameRateHz)
  return Math.max(1, Math.ceil(Math.max(0, stableDurationMs) / frameDurationMs))
}

/** Mirrors RehabMotion v3 TrainingLogic: low -> valid/high -> low. */
export class ElbowRepCycleDetector {
  private state: RepCycleState = 'WAIT_RETURN'

  constructor(private readonly options: RepCycleDetectorOptions) {}

  update(angleDeg: number): RepCycleUpdate {
    let completed = false
    if (this.state === 'WAIT_RETURN') {
      if (angleDeg <= this.options.returnAngleDeg) this.state = 'READY'
    } else if (this.state === 'READY') {
      if (angleDeg > this.options.returnAngleDeg) this.state = 'FLEXING'
    } else if (this.state === 'FLEXING') {
      if (angleDeg >= this.options.validAngleDeg) this.state = 'RETURNING'
      else if (angleDeg <= this.options.returnAngleDeg) this.state = 'READY'
    } else if (angleDeg <= this.options.returnAngleDeg) {
      completed = true
      this.state = 'WAIT_RETURN'
    }
    return { state: this.state, completed, needsReturn: this.state === 'WAIT_RETURN' }
  }

  resetToWaitReturn(): void {
    this.state = 'WAIT_RETURN'
  }

  // Backwards-compatible method name for the optional data-source capability.
  resetToWaitExtension(): void {
    this.resetToWaitReturn()
  }

  getState(): RepCycleState {
    return this.state
  }
}
