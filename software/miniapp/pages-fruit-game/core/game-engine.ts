import type { MotionFrame, RepEvent } from '../types/motion'
import type { GameMetrics, SessionEndReason, TrainingResult } from '../types/result'
import { totalSideCount } from '../types/motion'

export type FruitSide = 'left' | 'right'
export type OrchardTheme = 'day' | 'sunset'

export const NORMAL_FRUIT_NAMES = [
  'apple',
  'orange',
  'banana',
  'grapes',
  'peach',
  'pear',
  'strawberry',
  'watermelon'
] as const

// 保留原导出名，避免迁移方已有引用失效。
export const FRUIT_NAMES = NORMAL_FRUIT_NAMES

export const MAIN_FRUIT_NAMES = [
  ...NORMAL_FRUIT_NAMES,
  'goldenApple',
  'rainbowFruit'
] as const

export const COLLECTIBLE_FRUIT_NAMES = [
  ...MAIN_FRUIT_NAMES,
  'bothWatermelon'
] as const

export type NormalFruitName = typeof NORMAL_FRUIT_NAMES[number]
export type MainFruitName = typeof MAIN_FRUIT_NAMES[number]
export type CollectibleFruitName = typeof COLLECTIBLE_FRUIT_NAMES[number]
export type FruitInventory = Record<CollectibleFruitName, number>
export type RandomSource = () => number
export const COMBO_TIMEOUT_MS = 8000

export interface TrainingAggregate {
  leftTotalCount: number
  rightTotalCount: number
  leftMaxRomDeg: number
  rightMaxRomDeg: number
  overallCompletionPercent: number
}

export class TrainingAccumulator {
  private aggregate: TrainingAggregate = {
    leftTotalCount: 0,
    rightTotalCount: 0,
    leftMaxRomDeg: 0,
    rightMaxRomDeg: 0,
    overallCompletionPercent: 0
  }

  accept(frame: MotionFrame): TrainingAggregate {
    this.aggregate.leftTotalCount = Math.max(this.aggregate.leftTotalCount, totalSideCount(frame, 'left'))
    this.aggregate.rightTotalCount = Math.max(this.aggregate.rightTotalCount, totalSideCount(frame, 'right'))
    this.aggregate.leftMaxRomDeg = Math.max(this.aggregate.leftMaxRomDeg, frame.left_rom_deg)
    this.aggregate.rightMaxRomDeg = Math.max(this.aggregate.rightMaxRomDeg, frame.right_rom_deg)
    this.aggregate.overallCompletionPercent = Math.max(
      this.aggregate.overallCompletionPercent,
      frame.overall_completion_percent
    )
    return this.snapshot()
  }

  reset(): void {
    this.aggregate = {
      leftTotalCount: 0,
      rightTotalCount: 0,
      leftMaxRomDeg: 0,
      rightMaxRomDeg: 0,
      overallCompletionPercent: 0
    }
  }

  snapshot(): TrainingAggregate {
    return { ...this.aggregate }
  }
}

export interface GameState extends GameMetrics {
  activeSide: FruitSide
  activeFruit: MainFruitName
  activeFruitForced: boolean
  inventory: FruitInventory
  orchardTheme: OrchardTheme
  bonusActive: boolean
  bonusTriggeredMask: number
  feedback: string
  effectId: number
  comboRemainingMs: number
}

export interface GameTransition {
  state: GameState
  harvested: boolean
  ignoredWrongSide: boolean
  harvestedFruit?: CollectibleFruitName
  pointsAwarded: number
}

const BONUS_THRESHOLDS = [35, 70] as const

function emptyInventory(): FruitInventory {
  return COLLECTIBLE_FRUIT_NAMES.reduce((inventory, name) => {
    inventory[name] = 0
    return inventory
  }, {} as FruitInventory)
}

function normalizedRandom(random: RandomSource): number {
  const value = Number(random())
  if (!Number.isFinite(value)) return 0
  return Math.min(0.999999999, Math.max(0, value))
}

export function pickRandomMainFruit(random: RandomSource = Math.random): MainFruitName {
  const value = normalizedRandom(random)
  if (value < 0.82) {
    const index = Math.min(
      NORMAL_FRUIT_NAMES.length - 1,
      Math.floor((value / 0.82) * NORMAL_FRUIT_NAMES.length)
    )
    return NORMAL_FRUIT_NAMES[index]
  }
  if (value < 0.94) return 'goldenApple'
  return 'rainbowFruit'
}

export function createInitialGameState(
  random: RandomSource = Math.random,
  orchardTheme: OrchardTheme = 'day'
): GameState {
  return {
    activeSide: 'left',
    activeFruit: pickRandomMainFruit(random),
    activeFruitForced: false,
    inventory: emptyInventory(),
    orchardTheme,
    bonusActive: false,
    bonusTriggeredMask: 0,
    harvestedCount: 0,
    normalFruitCount: 0,
    goldenAppleCount: 0,
    rainbowFruitCount: 0,
    bothWatermelonCount: 0,
    score: 0,
    combo: 0,
    maxCombo: 0,
    wrongSideCount: 0,
    feedback: '请完成左侧动作',
    effectId: 0,
    comboRemainingMs: 0
  }
}

export function advanceComboTimer(state: GameState, activeDeltaMs: number): GameState {
  if (state.combo <= 0 || state.comboRemainingMs <= 0 || activeDeltaMs <= 0) return state
  const comboRemainingMs = Math.max(0, state.comboRemainingMs - activeDeltaMs)
  if (comboRemainingMs > 0) return { ...state, comboRemainingMs }
  return {
    ...state,
    combo: 0,
    comboRemainingMs: 0,
    feedback: '连击已中断，请继续当前侧动作'
  }
}

export function resolveOrchardTheme(date: Date = new Date()): OrchardTheme {
  const hour = date.getHours()
  return hour >= 18 || hour < 6 ? 'sunset' : 'day'
}

export function withOrchardTheme(state: GameState, theme: OrchardTheme): GameState {
  return state.orchardTheme === theme ? state : { ...state, orchardTheme: theme }
}

export function activateBonusForProgress(state: GameState, completionPercent: number): GameState {
  if (state.bonusActive) return state
  const nextIndex = BONUS_THRESHOLDS.findIndex((threshold, index) => {
    const bit = 1 << index
    return completionPercent >= threshold && (state.bonusTriggeredMask & bit) === 0
  })
  if (nextIndex < 0) return state
  return {
    ...state,
    bonusActive: true,
    bonusTriggeredMask: state.bonusTriggeredMask | (1 << nextIndex),
    feedback: '双手西瓜奖励出现！可同时完成双侧动作'
  }
}

export function expireBonus(state: GameState): GameState {
  if (!state.bonusActive) return state
  return { ...state, bonusActive: false, feedback: '双手奖励已结束，请继续当前目标' }
}

function expectedEvent(side: FruitSide): RepEvent {
  return side === 'left' ? 'left_rep_done' : 'right_rep_done'
}

function oppositeEvent(side: FruitSide): RepEvent {
  return side === 'left' ? 'right_rep_done' : 'left_rep_done'
}

function pointsForFruit(fruit: MainFruitName): number {
  if (fruit === 'goldenApple') return 200
  if (fruit === 'rainbowFruit') return 300
  return 100
}

function incrementInventory(inventory: FruitInventory, fruit: CollectibleFruitName): FruitInventory {
  return { ...inventory, [fruit]: inventory[fruit] + 1 }
}

function mainFruitMetrics(state: GameState, fruit: MainFruitName): Pick<
  GameMetrics,
  'normalFruitCount' | 'goldenAppleCount' | 'rainbowFruitCount'
> {
  return {
    normalFruitCount: state.normalFruitCount + (NORMAL_FRUIT_NAMES.includes(fruit as NormalFruitName) ? 1 : 0),
    goldenAppleCount: state.goldenAppleCount + (fruit === 'goldenApple' ? 1 : 0),
    rainbowFruitCount: state.rainbowFruitCount + (fruit === 'rainbowFruit' ? 1 : 0)
  }
}

export function reduceGameState(
  state: GameState,
  frame: MotionFrame,
  random: RandomSource = Math.random
): GameTransition {
  const unchanged = { state, harvested: false, ignoredWrongSide: false, pointsAwarded: 0 }
  if (frame.training_state !== 'RUNNING' || frame.rep_event === 'none') return unchanged

  if (frame.rep_event === 'both_rep_done') {
    if (!state.bonusActive) {
      return {
        ...unchanged,
        state: { ...state, feedback: '当前没有双手奖励任务，请继续当前侧动作' }
      }
    }
    return {
      state: {
        ...state,
        bonusActive: false,
        inventory: incrementInventory(state.inventory, 'bothWatermelon'),
        harvestedCount: state.harvestedCount + 1,
        bothWatermelonCount: state.bothWatermelonCount + 1,
        score: state.score + 500,
        feedback: '双手西瓜收入篮中！+500',
        effectId: state.effectId + 1
      },
      harvested: true,
      ignoredWrongSide: false,
      harvestedFruit: 'bothWatermelon',
      pointsAwarded: 500
    }
  }

  if (frame.rep_event === oppositeEvent(state.activeSide)) {
    return {
      state: {
        ...state,
        combo: 0,
        comboRemainingMs: 0,
        wrongSideCount: state.wrongSideCount + 1,
        feedback: `当前目标是${state.activeSide === 'left' ? '左侧' : '右侧'}，请按提示完成`
      },
      harvested: false,
      ignoredWrongSide: true,
      pointsAwarded: 0
    }
  }

  if (frame.rep_event !== expectedEvent(state.activeSide)) return unchanged

  const harvestedFruit = state.activeFruit
  const pointsAwarded = pointsForFruit(harvestedFruit)
  const combo = state.combo + 1
  const forceGoldenApple = combo % 5 === 0
  // 即使本次被连击金苹果覆盖，也消费一次基础随机数。这样同一房间中
  // 第 N 颗基础水果不会因某位玩家触发强制奖励而与其他玩家错位。
  const randomFruit = pickRandomMainFruit(random)
  const nextFruit = forceGoldenApple ? 'goldenApple' : randomFruit
  const nextSide: FruitSide = state.activeSide === 'left' ? 'right' : 'left'
  const fruitMetrics = mainFruitMetrics(state, harvestedFruit)

  return {
    state: {
      ...state,
      activeSide: nextSide,
      activeFruit: nextFruit,
      activeFruitForced: forceGoldenApple,
      inventory: incrementInventory(state.inventory, harvestedFruit),
      harvestedCount: state.harvestedCount + 1,
      ...fruitMetrics,
      score: state.score + pointsAwarded,
      combo,
      comboRemainingMs: COMBO_TIMEOUT_MS,
      maxCombo: Math.max(state.maxCombo, combo),
      feedback: `摘取成功 +${pointsAwarded}！请准备${nextSide === 'left' ? '左侧' : '右侧'}`,
      effectId: state.effectId + 1
    },
    harvested: true,
    ignoredWrongSide: false,
    harvestedFruit,
    pointsAwarded
  }
}

export function basketStage(metrics: GameMetrics, targetCount: number): 'empty' | 'half' | 'full' {
  const totalTarget = Math.max(1, targetCount * 2)
  const ratio = metrics.harvestedCount / totalTarget
  if (ratio >= 0.85) return 'full'
  if (ratio >= 0.35) return 'half'
  return 'empty'
}

export function createTrainingResult(
  endReason: SessionEndReason,
  frame: MotionFrame,
  game: GameState,
  elapsedMs: number,
  activeElapsedMs: number,
  completedAtMs = Date.now(),
  aggregate?: TrainingAggregate
): TrainingResult {
  return {
    endReason,
    elapsedMs: Math.max(0, elapsedMs),
    activeElapsedMs: Math.max(0, Math.min(activeElapsedMs, elapsedMs)),
    completedAtMs,
    training: {
      left_count: frame.left_count,
      right_count: frame.right_count,
      left_rom_deg: frame.left_rom_deg,
      right_rom_deg: frame.right_rom_deg,
      lr_rom_diff_deg: frame.lr_rom_diff_deg,
      target_count: frame.target_count,
      completion_percent: frame.completion_percent,
      training_state: frame.training_state,
      set_index: frame.set_index,
      target_sets: frame.target_sets,
      overall_completion_percent: frame.overall_completion_percent,
      left_total_count: aggregate?.leftTotalCount ?? totalSideCount(frame, 'left'),
      right_total_count: aggregate?.rightTotalCount ?? totalSideCount(frame, 'right'),
      session_left_rom_deg: aggregate?.leftMaxRomDeg ?? frame.left_rom_deg,
      session_right_rom_deg: aggregate?.rightMaxRomDeg ?? frame.right_rom_deg,
      session_lr_rom_diff_deg: Math.abs(
        (aggregate?.leftMaxRomDeg ?? frame.left_rom_deg) -
        (aggregate?.rightMaxRomDeg ?? frame.right_rom_deg)
      )
    },
    game: {
      harvestedCount: game.harvestedCount,
      normalFruitCount: game.normalFruitCount,
      goldenAppleCount: game.goldenAppleCount,
      rainbowFruitCount: game.rainbowFruitCount,
      bothWatermelonCount: game.bothWatermelonCount,
      score: game.score,
      combo: game.combo,
      maxCombo: game.maxCombo,
      wrongSideCount: game.wrongSideCount
    }
  }
}
