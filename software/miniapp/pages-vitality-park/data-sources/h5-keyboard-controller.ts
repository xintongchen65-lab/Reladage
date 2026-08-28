import type { AngleControllableMotionDataSource, HeldControl } from '../../game-platform/motion/data-source'
import type { VitalityMotionFrame } from '../types/motion'

const controls: Record<string, HeldControl> = { w: 'leftFlex', ArrowUp: 'leftFlex', s: 'leftExtend', ArrowDown: 'leftExtend' }
export class VitalityKeyboardController {
  private pressed = new Set<string>()
  constructor(private readonly source: AngleControllableMotionDataSource<VitalityMotionFrame>, private readonly debugEnabled: boolean, private readonly commands: { pause(): void; finish(): void; stop(): void; reset(): void }) {}
  attach(): void { window.addEventListener('keydown', this.onKeyDown); window.addEventListener('keyup', this.onKeyUp) }
  detach(): void { window.removeEventListener('keydown', this.onKeyDown); window.removeEventListener('keyup', this.onKeyUp); this.release() }
  private onKeyDown = (event: KeyboardEvent): void => { const key = event.key.length === 1 ? event.key.toLowerCase() : event.key; const control = controls[key]; if (control) { event.preventDefault(); if (!this.pressed.has(key)) this.source.setHeldControl(control, true); this.pressed.add(key); return }; if (event.repeat) return; if (key === 'p') this.commands.pause(); if (!this.debugEnabled) return; if (key === 'f') this.commands.finish(); else if (key === 'x') this.commands.stop(); else if (key === 'r') this.commands.reset() }
  private onKeyUp = (event: KeyboardEvent): void => { const key = event.key.length === 1 ? event.key.toLowerCase() : event.key; const control = controls[key]; if (!control) return; event.preventDefault(); this.source.setHeldControl(control, false); this.pressed.delete(key) }
  private release(): void { this.pressed.forEach((key) => { const control = controls[key]; if (control) this.source.setHeldControl(control, false) }); this.pressed.clear() }
}
