export interface MoleGameAssetPaths {
  background: string
  moles: readonly [string, string, string, string, string]
  hole: string
  hammer: string
  sign: string
  hitBurst: string
  warningRing: string
  carrot: string
  coin: string
  star: string
}

export let MOLE_GAME_ASSETS: MoleGameAssetPaths

// #ifdef H5
MOLE_GAME_ASSETS = {
  background: new URL('../static/backgrounds/meadow.jpg', import.meta.url).href,
  moles: [
    new URL('../static/moles/mole-1.png', import.meta.url).href,
    new URL('../static/moles/mole-2.png', import.meta.url).href,
    new URL('../static/moles/mole-3.png', import.meta.url).href,
    new URL('../static/moles/mole-4.png', import.meta.url).href,
    new URL('../static/moles/mole-5.png', import.meta.url).href
  ],
  hole: new URL('../static/objects/hole.png', import.meta.url).href,
  hammer: new URL('../static/objects/hammer.png', import.meta.url).href,
  sign: new URL('../static/objects/sign.png', import.meta.url).href,
  hitBurst: new URL('../static/effects/hit-burst.png', import.meta.url).href,
  warningRing: new URL('../static/effects/warning-ring.png', import.meta.url).href,
  carrot: new URL('../static/objects/carrot.png', import.meta.url).href,
  coin: new URL('../static/objects/coin.png', import.meta.url).href,
  star: new URL('../static/objects/star.png', import.meta.url).href
}
// #endif

// #ifndef H5
const ROOT = '/pages-mole-game/static'
MOLE_GAME_ASSETS = {
  background: `${ROOT}/backgrounds/meadow.jpg`,
  moles: [
    `${ROOT}/moles/mole-1.png`,
    `${ROOT}/moles/mole-2.png`,
    `${ROOT}/moles/mole-3.png`,
    `${ROOT}/moles/mole-4.png`,
    `${ROOT}/moles/mole-5.png`
  ],
  hole: `${ROOT}/objects/hole.png`,
  hammer: `${ROOT}/objects/hammer.png`,
  sign: `${ROOT}/objects/sign.png`,
  hitBurst: `${ROOT}/effects/hit-burst.png`,
  warningRing: `${ROOT}/effects/warning-ring.png`,
  carrot: `${ROOT}/objects/carrot.png`,
  coin: `${ROOT}/objects/coin.png`,
  star: `${ROOT}/objects/star.png`
}
// #endif
