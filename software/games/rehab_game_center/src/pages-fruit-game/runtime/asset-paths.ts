export interface FruitGameAssetPaths {
  orchard: { day: string; sunset: string; basketZone: string }
  character: {
    body: string
    left: { low: string; mid: string; high: string }
    right: { low: string; mid: string; high: string }
  }
  fruits: Record<string, string>
  basket: { empty: string; half: string; full: string }
  effects: { combo: string; rewardStar: string; harvestBurst: string }
}

export let FRUIT_GAME_ASSETS: FruitGameAssetPaths

// H5 没有真实分包静态目录，由 Vite 生成可访问的资源 URL。
// #ifdef H5
FRUIT_GAME_ASSETS = {
  orchard: {
    day: new URL('../static/backgrounds/orchard-day.webp', import.meta.url).href,
    sunset: new URL('../static/backgrounds/orchard-sunset.webp', import.meta.url).href,
    basketZone: new URL('../static/backgrounds/basket-zone.webp', import.meta.url).href
  },
  character: {
    body: new URL('../static/character/boy-body.png', import.meta.url).href,
    left: {
      low: new URL('../static/character/left-low.png', import.meta.url).href,
      mid: new URL('../static/character/left-mid.png', import.meta.url).href,
      high: new URL('../static/character/left-high.png', import.meta.url).href
    },
    right: {
      low: new URL('../static/character/right-low.png', import.meta.url).href,
      mid: new URL('../static/character/right-mid.png', import.meta.url).href,
      high: new URL('../static/character/right-high.png', import.meta.url).href
    }
  },
  fruits: {
    apple: new URL('../static/fruits/apple.png', import.meta.url).href,
    orange: new URL('../static/fruits/orange.png', import.meta.url).href,
    banana: new URL('../static/fruits/banana.png', import.meta.url).href,
    grapes: new URL('../static/fruits/grapes.png', import.meta.url).href,
    peach: new URL('../static/fruits/peach.png', import.meta.url).href,
    pear: new URL('../static/fruits/pear.png', import.meta.url).href,
    strawberry: new URL('../static/fruits/strawberry.png', import.meta.url).href,
    watermelon: new URL('../static/fruits/watermelon.png', import.meta.url).href,
    goldenApple: new URL('../static/fruits/golden-apple.png', import.meta.url).href,
    rainbowFruit: new URL('../static/fruits/rainbow-fruit.png', import.meta.url).href,
    bothWatermelon: new URL('../static/fruits/both-watermelon.png', import.meta.url).href
  },
  basket: {
    empty: new URL('../static/basket/empty.webp', import.meta.url).href,
    half: new URL('../static/basket/half.webp', import.meta.url).href,
    full: new URL('../static/basket/full.webp', import.meta.url).href
  },
  effects: {
    combo: new URL('../static/effects/combo.png', import.meta.url).href,
    rewardStar: new URL('../static/effects/reward-star.png', import.meta.url).href,
    harvestBurst: new URL('../static/effects/harvest-burst.png', import.meta.url).href
  }
}
// #endif

// 微信/App 运行时保持分包内绝对路径，不从主包 static 取素材。
// #ifndef H5
const RFG_STATIC_ROOT = '/pages-fruit-game/static'
FRUIT_GAME_ASSETS = {
  orchard: {
    day: `${RFG_STATIC_ROOT}/backgrounds/orchard-day.webp`,
    sunset: `${RFG_STATIC_ROOT}/backgrounds/orchard-sunset.webp`,
    basketZone: `${RFG_STATIC_ROOT}/backgrounds/basket-zone.webp`
  },
  character: {
    body: `${RFG_STATIC_ROOT}/character/boy-body.png`,
    left: {
      low: `${RFG_STATIC_ROOT}/character/left-low.png`,
      mid: `${RFG_STATIC_ROOT}/character/left-mid.png`,
      high: `${RFG_STATIC_ROOT}/character/left-high.png`
    },
    right: {
      low: `${RFG_STATIC_ROOT}/character/right-low.png`,
      mid: `${RFG_STATIC_ROOT}/character/right-mid.png`,
      high: `${RFG_STATIC_ROOT}/character/right-high.png`
    }
  },
  fruits: {
    apple: `${RFG_STATIC_ROOT}/fruits/apple.png`,
    orange: `${RFG_STATIC_ROOT}/fruits/orange.png`,
    banana: `${RFG_STATIC_ROOT}/fruits/banana.png`,
    grapes: `${RFG_STATIC_ROOT}/fruits/grapes.png`,
    peach: `${RFG_STATIC_ROOT}/fruits/peach.png`,
    pear: `${RFG_STATIC_ROOT}/fruits/pear.png`,
    strawberry: `${RFG_STATIC_ROOT}/fruits/strawberry.png`,
    watermelon: `${RFG_STATIC_ROOT}/fruits/watermelon.png`,
    goldenApple: `${RFG_STATIC_ROOT}/fruits/golden-apple.png`,
    rainbowFruit: `${RFG_STATIC_ROOT}/fruits/rainbow-fruit.png`,
    bothWatermelon: `${RFG_STATIC_ROOT}/fruits/both-watermelon.png`
  },
  basket: {
    empty: `${RFG_STATIC_ROOT}/basket/empty.webp`,
    half: `${RFG_STATIC_ROOT}/basket/half.webp`,
    full: `${RFG_STATIC_ROOT}/basket/full.webp`
  },
  effects: {
    combo: `${RFG_STATIC_ROOT}/effects/combo.png`,
    rewardStar: `${RFG_STATIC_ROOT}/effects/reward-star.png`,
    harvestBurst: `${RFG_STATIC_ROOT}/effects/harvest-burst.png`
  }
}
// #endif
