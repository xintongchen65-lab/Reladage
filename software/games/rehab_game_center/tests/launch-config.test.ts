import { describe, expect, it } from 'vitest'
import { isDebugBuild, resolveDebugEnabled } from '../src/pages-fruit-game/runtime/launch-config'

describe('launch debug gate', () => {
  it('requires both an eligible build and an explicit debug query', () => {
    expect(isDebugBuild('development', false)).toBe(true)
    expect(isDebugBuild('production', false)).toBe(false)
    expect(isDebugBuild('production', true)).toBe(true)
    expect(resolveDebugEnabled('1', true)).toBe(true)
    expect(resolveDebugEnabled(undefined, true)).toBe(false)
    expect(resolveDebugEnabled('0', true)).toBe(false)
    expect(resolveDebugEnabled('1', false)).toBe(false)
  })
})
