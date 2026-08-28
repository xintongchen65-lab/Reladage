export type KneeCycleState = 'WAIT_RETURN' | 'READY' | 'FLEXING' | 'RETURNING'

export class KneeRepCycleDetector {
  private state: KneeCycleState = 'WAIT_RETURN'
  constructor(private readonly validAngleDeg: number, private readonly returnAngleDeg: number) {}
  update(angleDeg: number): { completed: boolean; state: KneeCycleState } {
    let completed = false
    if (this.state === 'WAIT_RETURN' && angleDeg <= this.returnAngleDeg) this.state = 'READY'
    else if (this.state === 'READY' && angleDeg > this.returnAngleDeg) this.state = 'FLEXING'
    else if (this.state === 'FLEXING') {
      if (angleDeg >= this.validAngleDeg) this.state = 'RETURNING'
      else if (angleDeg <= this.returnAngleDeg) this.state = 'READY'
    } else if (this.state === 'RETURNING' && angleDeg <= this.returnAngleDeg) {
      completed = true
      this.state = 'WAIT_RETURN'
    }
    return { completed, state: this.state }
  }
  reset(): void { this.state = 'WAIT_RETURN' }
  getState(): KneeCycleState { return this.state }
}
