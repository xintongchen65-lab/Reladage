export interface VitalityAssetPaths {
  backgrounds: { far: string; mid: string; foreground: string }
  bench: string
  poses: Record<'sitting' | 'lean-forward' | 'lift-off' | 'half-standing' | 'standing' | 'sit-back', string>
  events: Record<string, string>
  effects: { burst: string; stars: string; celebration: string; groundShadow: string }
}

const EVENT_NAMES = ['bird', 'flowers', 'lamp', 'butterfly', 'fountain', 'kite', 'dog', 'flags', 'rainbow', 'celebration']

export let VITALITY_ASSETS: VitalityAssetPaths
// #ifdef H5
VITALITY_ASSETS = {
  backgrounds: {
    far: new URL('../static/backgrounds/park-far.jpg', import.meta.url).href,
    mid: new URL('../static/backgrounds/park-mid.png', import.meta.url).href,
    foreground: new URL('../static/backgrounds/park-foreground.png', import.meta.url).href
  },
  bench: new URL('../static/props/bench.png', import.meta.url).href,
  poses: {
    sitting: new URL('../static/character/sitting.png', import.meta.url).href,
    'lean-forward': new URL('../static/character/lean-forward.png', import.meta.url).href,
    'lift-off': new URL('../static/character/lift-off.png', import.meta.url).href,
    'half-standing': new URL('../static/character/half-standing.png', import.meta.url).href,
    standing: new URL('../static/character/standing.png', import.meta.url).href,
    'sit-back': new URL('../static/character/sit-back.png', import.meta.url).href
  },
  events: Object.fromEntries(EVENT_NAMES.map((name) => [name, new URL(`../static/events/${name}.png`, import.meta.url).href])),
  effects: {
    burst: new URL('../static/effects/event-burst.png', import.meta.url).href,
    stars: new URL('../static/effects/star-particles.png', import.meta.url).href,
    celebration: new URL('../static/effects/celebration.png', import.meta.url).href,
    groundShadow: new URL('../static/effects/ground-shadow.png', import.meta.url).href
  }
}
// #endif
// #ifndef H5
const ROOT = '/pages-vitality-park/static'
VITALITY_ASSETS = {
  backgrounds: {
    far: `${ROOT}/backgrounds/park-far.jpg`,
    mid: `${ROOT}/backgrounds/park-mid.png`,
    foreground: `${ROOT}/backgrounds/park-foreground.png`
  },
  bench: `${ROOT}/props/bench.png`,
  poses: {
    sitting: `${ROOT}/character/sitting.png`,
    'lean-forward': `${ROOT}/character/lean-forward.png`,
    'lift-off': `${ROOT}/character/lift-off.png`,
    'half-standing': `${ROOT}/character/half-standing.png`,
    standing: `${ROOT}/character/standing.png`,
    'sit-back': `${ROOT}/character/sit-back.png`
  },
  events: Object.fromEntries(EVENT_NAMES.map((name) => [name, `${ROOT}/events/${name}.png`])),
  effects: {
    burst: `${ROOT}/effects/event-burst.png`,
    stars: `${ROOT}/effects/star-particles.png`,
    celebration: `${ROOT}/effects/celebration.png`,
    groundShadow: `${ROOT}/effects/ground-shadow.png`
  }
}
// #endif
