export type VitalityCycleState = 'WAIT_RETURN' | 'READY' | 'RISING' | 'STANDING' | 'RETURNING'

export class VitalityRepCycleDetector {
  private state: VitalityCycleState = 'WAIT_RETURN'
  constructor(private readonly validProgress = 0.72, private readonly returnProgress = 0.12) {}
  update(progress: number): { completed: boolean; state: VitalityCycleState } {
    const value = Math.max(0, Math.min(1, Number(progress) || 0))
    let completed = false
    if (this.state === 'WAIT_RETURN' && value <= this.returnProgress) this.state = 'READY'
    else if (this.state === 'READY' && value > this.returnProgress) this.state = 'RISING'
    else if (this.state === 'RISING' && value >= this.validProgress) this.state = 'STANDING'
    else if (this.state === 'STANDING' && value < this.validProgress) this.state = 'RETURNING'
    else if (this.state === 'RETURNING' && value <= this.returnProgress) {
      completed = true
      this.state = 'WAIT_RETURN'
    }
    return { completed, state: this.state }
  }
  reset(): void { this.state = 'WAIT_RETURN' }
  getState(): VitalityCycleState { return this.state }
}
