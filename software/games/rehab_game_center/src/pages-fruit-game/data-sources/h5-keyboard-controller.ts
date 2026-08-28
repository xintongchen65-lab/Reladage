import type { HeldControl, MotionDataSource } from './contracts'
import { isAngleControllableSource } from './contracts'

export interface KeyboardControllerOptions {
  debugEnabled: boolean
  onPause: () => void
  onFinish: () => void
  onStop: () => void
  onReset: () => void
}

const HELD_KEYS: Record<string, HeldControl> = {
  w: 'leftFlex',
  s: 'leftExtend',
  ArrowUp: 'rightFlex',
  ArrowDown: 'rightExtend'
}

export class H5KeyboardController {
  private attached = false

  constructor(
    private readonly source: MotionDataSource,
    private readonly options: KeyboardControllerOptions
  ) {}

  attach(): void {
    // #ifdef H5
    if (this.attached) return
    window.addEventListener('keydown', this.onKeyDown)
    window.addEventListener('keyup', this.onKeyUp)
    this.attached = true
    // #endif
  }

  detach(): void {
    // #ifdef H5
    if (!this.attached) return
    window.removeEventListener('keydown', this.onKeyDown)
    window.removeEventListener('keyup', this.onKeyUp)
    const source = this.source
    if (isAngleControllableSource(source)) {
      Object.values(HELD_KEYS).forEach((control) => source.setHeldControl(control, false))
    }
    this.attached = false
    // #endif
  }

  private readonly onKeyDown = (event: KeyboardEvent): void => {
    const key = event.key.length === 1 ? event.key.toLowerCase() : event.key
    const heldControl = HELD_KEYS[key]
    if (heldControl && isAngleControllableSource(this.source)) {
      event.preventDefault()
      this.source.setHeldControl(heldControl, true)
      return
    }
    if (event.repeat) return
    if (key === 'p') this.options.onPause()
    if (!this.options.debugEnabled) return
    if (key === 'f') this.options.onFinish()
    if (key === 'x') this.options.onStop()
    if (key === 'r') this.options.onReset()
  }

  private readonly onKeyUp = (event: KeyboardEvent): void => {
    const key = event.key.length === 1 ? event.key.toLowerCase() : event.key
    const heldControl = HELD_KEYS[key]
    if (!heldControl || !isAngleControllableSource(this.source)) return
    event.preventDefault()
    this.source.setHeldControl(heldControl, false)
  }
}
