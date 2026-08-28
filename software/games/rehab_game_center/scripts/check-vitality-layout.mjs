import fs from 'node:fs'
import path from 'node:path'

const root = process.cwd()
const game = fs.readFileSync(path.join(root, 'src/pages-vitality-park/game/index.vue'), 'utf8')
const prepare = fs.readFileSync(path.join(root, 'src/pages-vitality-park/prepare/index.vue'), 'utf8')
const errors = []

for (const layer of ['far', 'mid', 'front']) {
  const rx = new RegExp(`vp-game__bg--${layer}[^>]*mode="aspectFill"`)
  if (!rx.test(game)) errors.push(`background ${layer} must use aspectFill so H5/WeChat crop identically`)
}
if (!game.includes('getMenuButtonBoundingClientRect')) errors.push('WeChat menu capsule safe-right handling missing')
if (!game.includes(':style="{ right: `${hudSafeRight}px` }"')) errors.push('HUD does not consume computed safe-right inset')

const benchBlock = game.match(/\.vp-game__bench \{[\s\S]*?\n\}/)?.[0] ?? ''
const stageBlock = game.match(/\.vp-game__stage \{[\s\S]*?\n\}/)?.[0] ?? ''
for (const [name, block] of [['bench', benchBlock], ['stage', stageBlock]]) {
  if (!/width:\s*[0-9.]+vh/.test(block) || !/height:\s*[0-9.]+vh/.test(block)) errors.push(`${name} must be sized in vh for identical cross-platform proportions`)
  if (/\bvw\b|min-width|max-width/.test(block)) errors.push(`${name} still mixes viewport width/fixed limits and may scale differently in WeChat`)
}
if (!prepare.includes("resolveDebugEnabled(query?.debug)")) errors.push('vitality park must use the shared production debug gate')

if (errors.length) {
  console.error(errors.map((x) => `- ${x}`).join('\n'))
  process.exit(1)
}
console.log('Vitality layout check passed: aligned background crop, viewport-height anchors, WeChat capsule inset and production debug gate.')
