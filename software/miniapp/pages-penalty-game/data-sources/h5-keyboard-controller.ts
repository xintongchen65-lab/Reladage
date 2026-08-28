import type { AngleControllableMotionDataSource, HeldControl } from '../../game-platform/motion/data-source'
import type { KneeMotionFrame } from '../types/motion'
import type { CycleSimulatableSource } from './contracts'

const heldKeys: Record<string, HeldControl | undefined> = {
  w: 'leftFlex', s: 'leftExtend', arrowup: 'rightFlex', arrowdown: 'rightExtend'
}

export class PenaltyKeyboardController {
  private pressed = new Set<string>()
  constructor(
    private readonly source: AngleControllableMotionDataSource<KneeMotionFrame>,
    private readonly cycleSource: CycleSimulatableSource | null,
    private readonly debugEnabled: boolean,
    private readonly onCommand: (command: 'pause' | 'finish' | 'stop' | 'reset') => void
  ) {}
  attach(): void { window.addEventListener('keydown', this.onKeyDown); window.addEventListener('keyup', this.onKeyUp) }
  detach(): void { window.removeEventListener('keydown', this.onKeyDown); window.removeEventListener('keyup', this.onKeyUp); this.releaseAll() }
  private onKeyDown = (event: KeyboardEvent): void => {
    const key = event.key.toLowerCase(); const control = heldKeys[key]
    if (control) { event.preventDefault(); if (!this.pressed.has(key)) this.source.setHeldControl(control, true); this.pressed.add(key); return }
    if (event.repeat) return
    if (key === 'p') this.onCommand('pause')
    if (!this.debugEnabled) return
    if (key === 'q') this.cycleSource?.simulateCompleteCycle('left')
    else if (key === 'e') this.cycleSource?.simulateCompleteCycle('right')
    else if (key === 'f') this.onCommand('finish')
    else if (key === 'x') this.onCommand('stop')
    else if (key === 'r') this.onCommand('reset')
  }
  private onKeyUp = (event: KeyboardEvent): void => {
    const key = event.key.toLowerCase(); const control = heldKeys[key]
    if (control) { event.preventDefault(); this.source.setHeldControl(control, false); this.pressed.delete(key) }
  }
  private releaseAll(): void { this.pressed.forEach((key) => { const control = heldKeys[key]; if (control) this.source.setHeldControl(control, false) }); this.pressed.clear() }
}
