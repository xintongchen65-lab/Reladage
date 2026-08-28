import fs from 'node:fs'
import path from 'node:path'
import { describe, expect, it } from 'vitest'

const componentPath = path.join(process.cwd(), 'src/pages-fruit-game/components/CharacterRig.vue')
const debugPath = path.join(process.cwd(), 'src/pages-fruit-game/components/DebugControls.vue')

describe('character rig rendering safety', () => {
  it('keeps all six arm pose images mounted and switches only active opacity', () => {
    const source = fs.readFileSync(componentPath, 'utf8')
    expect(source.match(/class="rfg-rig__arm-layer"/g)).toHaveLength(6)
    expect(source.match(/rfg-rig__arm-layer--active/g)?.length).toBeGreaterThanOrEqual(7)
    expect(source).toContain('leftArmSources.low')
    expect(source).toContain('leftArmSources.mid')
    expect(source).toContain('leftArmSources.high')
    expect(source).toContain('rightArmSources.low')
    expect(source).toContain('rightArmSources.mid')
    expect(source).toContain('rightArmSources.high')
    expect(source).not.toContain(':src="leftArmSrc"')
    expect(source).not.toContain('translateY(')
  })

  it('deduplicates held controls and separates H5 mouse input from mini-program touch input', () => {
    const source = fs.readFileSync(debugPath, 'utf8')
    expect(source).toContain("if (this.heldState[control] === pressed) return")
    expect(source).toContain('<!-- #ifdef H5 -->')
    expect(source).toContain('<!-- #ifndef H5 -->')
  })
})
