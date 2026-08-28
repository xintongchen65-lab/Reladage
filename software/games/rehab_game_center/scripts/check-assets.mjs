import fs from 'node:fs'
import path from 'node:path'
import { countOpaqueComponents, opaqueBounds, readRgbaPng } from './png-inspection.mjs'

const root = process.cwd()
const characterPath = path.join(root, 'src/pages-fruit-game/static/character/boy-body.png')
const fruitRoot = path.join(root, 'src/pages-fruit-game/static/fruits')
const armRoot = path.join(root, 'src/pages-fruit-game/static/character')
const effectRoot = path.join(root, 'src/pages-fruit-game/static/effects')
const backgroundRoot = path.join(root, 'src/pages-fruit-game/static/backgrounds')
const penaltyRoot = path.join(root, 'src/pages-penalty-game/static')
const vitalityRoot = path.join(root, 'src/pages-vitality-park/static')
const moleRoot = path.join(root, 'src/pages-mole-game/static')
const errors = []

function inspectTransparentAsset(file, expectedWidth, expectedHeight) {
  const image = readRgbaPng(file)
  if (image.width !== expectedWidth || image.height !== expectedHeight) {
    errors.push(`${path.basename(file)}: expected ${expectedWidth}x${expectedHeight}, got ${image.width}x${image.height}`)
  }
  const bounds = opaqueBounds(image)
  if (!bounds) errors.push(`${path.basename(file)}: no visible subject`)
  else if (bounds.left < 4 || bounds.top < 4 || bounds.right > image.width - 5 || bounds.bottom > image.height - 5) {
    errors.push(`${path.basename(file)}: subject lacks transparent safety padding`)
  }
  return image
}

const character = readRgbaPng(characterPath)
if (character.height !== 760 || character.width < 260 || character.width > 310) {
  errors.push(`boy-body.png: unexpected dimensions ${character.width}x${character.height}`)
}
const characterBounds = opaqueBounds(character)
if (!characterBounds || characterBounds.left < 2 || characterBounds.top < 2) {
  errors.push('boy-body.png: transparent crop margin missing')
}

let visibleCharacterMagenta = 0
for (let index = 0; index < character.width * character.height; index += 1) {
  const offset = index * 4
  const red = character.rgba[offset]
  const green = character.rgba[offset + 1]
  const blue = character.rgba[offset + 2]
  const alpha = character.rgba[offset + 3]
  if (alpha > 32 && red > 140 && blue > 140 && green < 135 && Math.abs(red - blue) < 105) {
    visibleCharacterMagenta += 1
  }
}
if (visibleCharacterMagenta > 2) {
  errors.push(`boy-body.png: visible magenta-key residue ${visibleCharacterMagenta}px`)
}

const grapes = inspectTransparentAsset(path.join(fruitRoot, 'grapes.png'), 256, 256)
const strawberry = inspectTransparentAsset(path.join(fruitRoot, 'strawberry.png'), 256, 256)
const watermelon = inspectTransparentAsset(path.join(fruitRoot, 'watermelon.png'), 256, 256)

for (const [name, image] of [['grapes.png', grapes], ['strawberry.png', strawberry], ['watermelon.png', watermelon]]) {
  const components = countOpaqueComponents(image)
  if (components !== 1) errors.push(`${name}: expected one connected fruit subject, got ${components}`)
}

let opaqueGrapePixels = 0
let purpleGrapePixels = 0
for (let index = 0; index < grapes.width * grapes.height; index += 1) {
  const offset = index * 4
  const red = grapes.rgba[offset]
  const green = grapes.rgba[offset + 1]
  const blue = grapes.rgba[offset + 2]
  const alpha = grapes.rgba[offset + 3]
  if (alpha <= 180) continue
  opaqueGrapePixels += 1
  if (red > 60 && blue > 75 && blue > green * 1.2 && red > green * 1.15) purpleGrapePixels += 1
}
if (opaqueGrapePixels === 0 || purpleGrapePixels / opaqueGrapePixels < 0.3) {
  errors.push('grapes.png: purple color coverage is too low')
}

for (const [name, image] of [['strawberry.png', strawberry], ['watermelon.png', watermelon]]) {
  let visibleMagenta = 0
  for (let index = 0; index < image.width * image.height; index += 1) {
    const offset = index * 4
    const red = image.rgba[offset]
    const green = image.rgba[offset + 1]
    const blue = image.rgba[offset + 2]
    const alpha = image.rgba[offset + 3]
    if (alpha > 32 && red > 140 && blue > 140 && green < 100 && Math.abs(red - blue) < 90) {
      visibleMagenta += 1
    }
  }
  if (visibleMagenta > 2) errors.push(`${name}: visible magenta-key residue ${visibleMagenta}px`)
}

for (const side of ['left', 'right']) {
  for (const pose of ['low', 'mid', 'high']) {
    const name = `${side}-${pose}.png`
    const image = inspectTransparentAsset(path.join(armRoot, name), 320, 320)
    let opaque = 0
    let orange = 0
    for (let index = 0; index < image.width * image.height; index += 1) {
      const offset = index * 4
      const red = image.rgba[offset]
      const green = image.rgba[offset + 1]
      const blue = image.rgba[offset + 2]
      const alpha = image.rgba[offset + 3]
      if (alpha <= 160) continue
      opaque += 1
      if (red > 170 && green > 55 && green < 155 && blue < 65) orange += 1
    }
    const ratio = opaque ? orange / opaque : 0
    if (ratio < 0.01 || ratio > 0.16) errors.push(`${name}: wristband ratio ${ratio.toFixed(3)} suggests missing band or shirt sleeve`)
  }
}

for (const name of ['golden-apple.png', 'rainbow-fruit.png', 'both-watermelon.png']) {
  inspectTransparentAsset(path.join(fruitRoot, name), 256, 256)
}
for (const name of ['combo.png', 'reward-star.png', 'harvest-burst.png']) {
  inspectTransparentAsset(path.join(effectRoot, name), 192, 192)
}
for (const name of ['orchard-day.webp', 'orchard-sunset.webp', 'basket-zone.webp']) {
  const file = path.join(backgroundRoot, name)
  if (!fs.existsSync(file) || fs.statSync(file).size < 20000) errors.push(`${name}: missing or unexpectedly small background`)
}

for (const relative of [
  'player/neutral.png','player/left-flex.png','player/left-kick.png','player/right-flex.png','player/right-kick.png',
  'keeper/ready.png','keeper/dive-left.png','keeper/dive-right.png','objects/ball.png',
  'effects/goal.png','effects/save.png','effects/miss.png','effects/combo.png'
]) {
  const file = path.join(penaltyRoot, relative)
  if (!fs.existsSync(file)) { errors.push(`penalty ${relative}: missing`); continue }
  const image = readRgbaPng(file); const bounds = opaqueBounds(image)
  if (!bounds) errors.push(`penalty ${relative}: no visible subject`)
  let magenta = 0
  for (let index = 0; index < image.width * image.height; index += 1) {
    const offset=index*4, red=image.rgba[offset], green=image.rgba[offset+1], blue=image.rgba[offset+2], alpha=image.rgba[offset+3]
    if (alpha > 100 && red > 190 && blue > 145 && green < 115 && red > green * 1.6) magenta += 1
  }
  if (magenta > 24) errors.push(`penalty ${relative}: visible chroma residue ${magenta}px`)
}
const stadium=path.join(penaltyRoot,'backgrounds/stadium.webp')
if(!fs.existsSync(stadium)||fs.statSync(stadium).size<20000)errors.push('penalty stadium.webp: missing or unexpectedly small')

for (const relative of [
  'backgrounds/park-far.webp','backgrounds/park-mid.png','backgrounds/park-foreground.png','props/bench.png',
  'character/sitting.png','character/lean-forward.png','character/lift-off.png','character/half-standing.png','character/standing.png','character/sit-back.png',
  'events/bird.png','events/flowers.png','events/lamp.png','events/butterfly.png','events/fountain.png','events/kite.png','events/dog.png','events/flags.png','events/rainbow.png','events/celebration.png',
  'effects/event-burst.png','effects/star-particles.png','effects/celebration.png','effects/ground-shadow.png'
]) {
  const file = path.join(vitalityRoot, relative)
  if (!fs.existsSync(file) || fs.statSync(file).size < 100) errors.push(`vitality ${relative}: missing or unexpectedly small`)
}


for (const relative of [
  'backgrounds/meadow.webp','objects/hammer.png','objects/hole.png','objects/sign.png','effects/hit-burst.png','effects/warning-ring.png',
  'moles/mole-1.png','moles/mole-2.png','moles/mole-3.png','moles/mole-4.png','moles/mole-5.png'
]) {
  const file = path.join(moleRoot, relative)
  if (!fs.existsSync(file) || fs.statSync(file).size < 100) errors.push(`mole ${relative}: missing or unexpectedly small`)
  else if (relative.endsWith('.png')) {
    const image = readRgbaPng(file)
    const bounds = opaqueBounds(image)
    if (!bounds) errors.push(`mole ${relative}: no visible subject`)
    else if (bounds.left < 2 || bounds.top < 2 || bounds.right > image.width - 3 || bounds.bottom > image.height - 3) errors.push(`mole ${relative}: transparent safety padding missing`)
    let chroma = 0
    for (let index = 0; index < image.width * image.height; index += 1) {
      const offset = index * 4
      const [red, green, blue, alpha] = [image.rgba[offset], image.rgba[offset + 1], image.rgba[offset + 2], image.rgba[offset + 3]]
      if (alpha > 120 && red > 170 && blue > 145 && green < 120 && Math.abs(red - blue) < 95) chroma += 1
    }
    if (chroma > 24) errors.push(`mole ${relative}: visible chroma-key residue ${chroma}px`)
  }
}

for (const name of ['mole-1.png', 'mole-2.png', 'mole-3.png', 'mole-4.png', 'mole-5.png']) {
  const image = readRgbaPng(path.join(moleRoot, 'moles', name))
  if (image.width !== 340 || image.height !== 260) errors.push(`mole ${name}: expected normalized 340x260 canvas, got ${image.width}x${image.height}`)
  const bounds = opaqueBounds(image)
  if (bounds && bounds.bottom < 230) errors.push(`mole ${name}: bottom anchor is too high (${bounds.bottom})`)
}

if (errors.length) {
  console.error(errors.map((error) => `- ${error}`).join('\n'))
  process.exit(1)
}

console.log('Asset check passed: fruit, penalty, vitality park and mole game characters, objects, effects and backgrounds.')
