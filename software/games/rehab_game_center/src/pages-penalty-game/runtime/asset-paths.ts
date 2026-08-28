export interface PenaltyAssetPaths {
  stadium: string
  player: { neutral: string; leftFlex: string; leftKick: string; rightFlex: string; rightKick: string }
  keeper: { ready: string; left: string; right: string }
  ball: string
  effects: { goal: string; save: string; miss: string; combo: string }
}
export let PENALTY_ASSETS: PenaltyAssetPaths
// #ifdef H5
PENALTY_ASSETS = {
  stadium:new URL('../static/backgrounds/stadium.webp',import.meta.url).href,
  player:{neutral:new URL('../static/player/neutral.png',import.meta.url).href,leftFlex:new URL('../static/player/left-flex.png',import.meta.url).href,leftKick:new URL('../static/player/left-kick.png',import.meta.url).href,rightFlex:new URL('../static/player/right-flex.png',import.meta.url).href,rightKick:new URL('../static/player/right-kick.png',import.meta.url).href},
  keeper:{ready:new URL('../static/keeper/ready.png',import.meta.url).href,left:new URL('../static/keeper/dive-left.png',import.meta.url).href,right:new URL('../static/keeper/dive-right.png',import.meta.url).href},
  ball:new URL('../static/objects/ball.png',import.meta.url).href,
  effects:{goal:new URL('../static/effects/goal.png',import.meta.url).href,save:new URL('../static/effects/save.png',import.meta.url).href,miss:new URL('../static/effects/miss.png',import.meta.url).href,combo:new URL('../static/effects/combo.png',import.meta.url).href}
}
// #endif
// #ifndef H5
const root='/pages-penalty-game/static'
PENALTY_ASSETS={stadium:`${root}/backgrounds/stadium.webp`,player:{neutral:`${root}/player/neutral.png`,leftFlex:`${root}/player/left-flex.png`,leftKick:`${root}/player/left-kick.png`,rightFlex:`${root}/player/right-flex.png`,rightKick:`${root}/player/right-kick.png`},keeper:{ready:`${root}/keeper/ready.png`,left:`${root}/keeper/dive-left.png`,right:`${root}/keeper/dive-right.png`},ball:`${root}/objects/ball.png`,effects:{goal:`${root}/effects/goal.png`,save:`${root}/effects/save.png`,miss:`${root}/effects/miss.png`,combo:`${root}/effects/combo.png`}}
// #endif
