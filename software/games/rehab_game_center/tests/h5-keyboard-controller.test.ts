import { describe, expect, it, vi } from 'vitest'
import { H5KeyboardController } from '../src/pages-fruit-game/data-sources/h5-keyboard-controller'
import type { HeldControl, MotionDataSource } from '../src/pages-fruit-game/data-sources/contracts'

function createSource() {
  const setHeldControl = vi.fn<(control: HeldControl, pressed: boolean) => void>()
  const source: MotionDataSource & { setHeldControl: typeof setHeldControl } = {
    start: vi.fn(),
    stop: vi.fn(),
    reset: vi.fn(),
    subscribe: vi.fn(() => () => {}),
    setHeldControl
  }
  return { source, setHeldControl }
}

function keyboardEvent(key: string, repeat = false) {
  return { key, repeat, preventDefault: vi.fn() }
}

describe('H5KeyboardController', () => {
  it('maps W/S and arrow keys to flexion-decrease and extension-increase controls', () => {
    const { source, setHeldControl } = createSource()
    const controller = new H5KeyboardController(source, {
      debugEnabled: false,
      onPause: vi.fn(),
      onFinish: vi.fn(),
      onStop: vi.fn(),
      onReset: vi.fn()
    })

    ;(controller as any).onKeyDown(keyboardEvent('w'))
    ;(controller as any).onKeyUp(keyboardEvent('w'))
    ;(controller as any).onKeyDown(keyboardEvent('s'))
    ;(controller as any).onKeyDown(keyboardEvent('ArrowUp'))
    ;(controller as any).onKeyDown(keyboardEvent('ArrowDown'))

    expect(setHeldControl.mock.calls).toEqual([
      ['leftFlex', true],
      ['leftFlex', false],
      ['leftExtend', true],
      ['rightFlex', true],
      ['rightExtend', true]
    ])
  })

  it('keeps P available and ignores Q/E/B even in debug mode', () => {
    const { source } = createSource()
    const onPause = vi.fn()
    const onFinish = vi.fn()
    const onStop = vi.fn()
    const onReset = vi.fn()
    const controller = new H5KeyboardController(source, {
      debugEnabled: false,
      onPause,
      onFinish,
      onStop,
      onReset
    })
    ;(controller as any).onKeyDown(keyboardEvent('p'))
    ;(controller as any).onKeyDown(keyboardEvent('q'))
    expect(onPause).toHaveBeenCalledTimes(1)

    const debugController = new H5KeyboardController(source, {
      debugEnabled: true,
      onPause: vi.fn(),
      onFinish,
      onStop,
      onReset
    })
    ;(debugController as any).onKeyDown(keyboardEvent('q'))
    ;(debugController as any).onKeyDown(keyboardEvent('e'))
    ;(debugController as any).onKeyDown(keyboardEvent('b'))
    expect(onFinish).not.toHaveBeenCalled()
    expect(onStop).not.toHaveBeenCalled()
    expect(onReset).not.toHaveBeenCalled()
  })
})
