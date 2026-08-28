import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import {
  DEFAULT_RETURN_URL,
  appendReturnUrl,
  configureCallerReturnUrl,
  normalizeReturnUrl,
  resetNavigationRuntime,
  returnToGameCenter,
  returnToCaller
} from '../src/pages-fruit-game/runtime/navigation-runtime'

describe('navigation runtime', () => {
  const navigateBack = vi.fn()
  const reLaunch = vi.fn()

  beforeEach(() => {
    vi.stubGlobal('uni', { navigateBack, reLaunch })
    vi.stubGlobal('getCurrentPages', () => [{}, {}])
  })

  afterEach(() => {
    resetNavigationRuntime()
    vi.unstubAllGlobals()
    vi.clearAllMocks()
  })

  it('accepts internal caller pages and rejects unsafe return targets', () => {
    expect(normalizeReturnUrl('/pages/member/training-menu')).toBe('/pages/member/training-menu')
    expect(normalizeReturnUrl('%2Fpages%2Fmember%2Ftraining-menu')).toBe('/pages/member/training-menu')
    expect(normalizeReturnUrl('%2Fpages%2Fdemo%2Findex')).toBe('/pages/demo/index')
    expect(normalizeReturnUrl('https://example.com')).toBe(DEFAULT_RETURN_URL)
    expect(normalizeReturnUrl('//evil.example/path')).toBe(DEFAULT_RETURN_URL)
    expect(normalizeReturnUrl('/pages/../secret')).toBe(DEFAULT_RETURN_URL)
  })

  it('keeps the configured return page in replay and result URLs', () => {
    configureCallerReturnUrl('/pages/member/training-menu')
    expect(appendReturnUrl('/pages-fruit-game/prepare/index?replay=1')).toBe(
      '/pages-fruit-game/prepare/index?replay=1&returnUrl=%2Fpages%2Fmember%2Ftraining-menu'
    )
  })

  it('uses navigateBack when the requested page stack exists', () => {
    returnToCaller({ delta: 1, returnUrl: '/pages/member/training-menu' })
    expect(navigateBack).toHaveBeenCalledWith(expect.objectContaining({ delta: 1 }))
    expect(reLaunch).not.toHaveBeenCalled()
  })

  it('falls back to reLaunch for a single-page stack or a navigation failure', () => {
    vi.stubGlobal('getCurrentPages', () => [{}])
    returnToCaller({ returnUrl: '/pages/member/training-menu' })
    expect(reLaunch).toHaveBeenLastCalledWith({ url: '/pages/member/training-menu' })

    vi.stubGlobal('getCurrentPages', () => [{}, {}])
    navigateBack.mockImplementationOnce((options) => options.fail())
    returnToCaller({ returnUrl: '/pages/member/training-menu' })
    expect(reLaunch).toHaveBeenLastCalledWith({ url: '/pages/member/training-menu' })
  })

  it('always relaunches the game center instead of trusting a stale page stack', () => {
    returnToGameCenter()
    expect(navigateBack).not.toHaveBeenCalled()
    expect(reLaunch).toHaveBeenCalledWith({ url: '/pages/game-center/index' })
  })
})
