<template>
  <view class="rfg-result" :style="viewportCssVars">
    <image class="rfg-result__background" :src="backgroundSrc" mode="aspectFill" />
    <view class="rfg-result__shade" />

    <view class="rfg-result__card">
      <view class="rfg-result__header">
        <text class="rfg-result__title">{{ completed ? '训练完成' : '训练未完成' }}</text>
        <text class="rfg-result__subtitle">{{ endDescription }}</text>
      </view>

      <view class="rfg-result__columns">
        <view class="rfg-result__section">
          <text class="rfg-result__section-title">康复训练数据</text>
          <view class="rfg-result__grid">
            <view class="rfg-result__metric"><text>左侧总有效次数</text><text class="rfg-result__value">{{ training.left_total_count }}</text></view>
            <view class="rfg-result__metric"><text>右侧总有效次数</text><text class="rfg-result__value">{{ training.right_total_count }}</text></view>
            <view class="rfg-result__metric"><text>左侧最大 ROM</text><text class="rfg-result__value">{{ format(training.session_left_rom_deg) }}°</text></view>
            <view class="rfg-result__metric"><text>右侧最大 ROM</text><text class="rfg-result__value">{{ format(training.session_right_rom_deg) }}°</text></view>
            <view class="rfg-result__metric"><text>训练组次</text><text class="rfg-result__value">{{ training.set_index }}/{{ training.target_sets }}</text></view>
            <view class="rfg-result__metric"><text>整体完成度</text><text class="rfg-result__value">{{ training.overall_completion_percent }}%</text></view>
          </view>
        </view>

        <view class="rfg-result__section rfg-result__section--game">
          <text class="rfg-result__section-title">游戏成绩</text>
          <view class="rfg-result__score"><text class="rfg-result__score-number">{{ game.score }}</text><text>分</text></view>
          <view class="rfg-result__game-row"><text>摘取水果</text><text>{{ game.harvestedCount }} 个</text></view>
          <view class="rfg-result__game-row"><text>普通水果</text><text>{{ game.normalFruitCount }} 个</text></view>
          <view class="rfg-result__game-row"><text>特殊水果</text><text>金 {{ game.goldenAppleCount }} · 彩 {{ game.rainbowFruitCount }} · 双 {{ game.bothWatermelonCount }}</text></view>
          <view class="rfg-result__game-row"><text>最大连击</text><text>×{{ game.maxCombo }}</text></view>
          <view class="rfg-result__game-row"><text>错侧动作</text><text>{{ game.wrongSideCount }} 次</text></view>
          <view class="rfg-result__game-row"><text>总用时</text><text>{{ elapsedSeconds }} 秒</text></view>
          <view class="rfg-result__game-row"><text>有效训练时长</text><text>{{ activeElapsedSeconds }} 秒</text></view>
        </view>
      </view>

      <view class="rfg-result__actions">
        <button class="rfg-result__button rfg-result__button--again" @click="playAgain">↻ 再练一次</button>
        <button class="rfg-result__button rfg-result__button--back" @click="returnToCaller">← 返回</button>
      </view>
    </view>
  </view>
</template>

<script lang="ts">
import { consumeResult } from '../runtime/session-runtime'
import { FRUIT_GAME_ASSETS } from '../runtime/asset-paths'
import { getViewportLayout, viewportStyle } from '../runtime/viewport-layout'
import { appendReturnUrl, configureCallerReturnUrl, getCallerReturnUrl, returnToCaller } from '../runtime/navigation-runtime'
import type { GameMetrics, TrainingResult } from '../types/result'
import type { MotionFrame } from '../types/motion'

type TrainingSnapshot = TrainingResult['training']

const EMPTY_TRAINING: TrainingSnapshot = {
  left_count: 0,
  right_count: 0,
  left_rom_deg: 0,
  right_rom_deg: 0,
  lr_rom_diff_deg: 0,
  target_count: 10,
  completion_percent: 0,
  training_state: 'STOPPED' as MotionFrame['training_state'],
  set_index: 1,
  target_sets: 1,
  overall_completion_percent: 0,
  left_total_count: 0,
  right_total_count: 0,
  session_left_rom_deg: 0,
  session_right_rom_deg: 0,
  session_lr_rom_diff_deg: 0
}

const EMPTY_GAME: GameMetrics = {
  harvestedCount: 0,
  normalFruitCount: 0,
  goldenAppleCount: 0,
  rainbowFruitCount: 0,
  bothWatermelonCount: 0,
  score: 0,
  combo: 0,
  maxCombo: 0,
  wrongSideCount: 0
}

export default {
  name: 'RfgResultPage',
  data() {
    return {
      backgroundSrc: FRUIT_GAME_ASSETS.orchard.day,
      result: null as TrainingResult | null,
      viewportLayout: getViewportLayout(),
      training: { ...EMPTY_TRAINING },
      game: { ...EMPTY_GAME },
      elapsedMs: 0,
      activeElapsedMs: 0,
      endReason: 'STOPPED' as 'FINISHED' | 'STOPPED'
    }
  },
  computed: {
    completed(): boolean {
      return this.endReason === 'FINISHED'
    },
    endDescription(): string {
      return this.completed ? '做得很好，今天的训练目标已记录' : '本次提前结束，已保留当前训练数据'
    },
    elapsedSeconds(): number {
      return Math.max(0, Math.round(this.elapsedMs / 1000))
    },
    activeElapsedSeconds(): number {
      return Math.max(0, Math.round(this.activeElapsedMs / 1000))
    },
    viewportCssVars(): Record<string, string> {
      return viewportStyle(this.viewportLayout)
    }
  },
  onLoad(query: Record<string, string | undefined>) {
    if (query?.returnUrl) configureCallerReturnUrl(query.returnUrl)
    const result = consumeResult()
    if (!result) return
    this.result = result
    this.training = { ...result.training }
    this.game = { ...result.game }
    this.elapsedMs = result.elapsedMs
    this.activeElapsedMs = result.activeElapsedMs
    this.endReason = result.endReason
  },
  onResize() {
    this.viewportLayout = getViewportLayout()
  },
  methods: {
    format(value: number): string {
      return Number(value).toFixed(1)
    },
    playAgain(): void {
      uni.redirectTo({ url: appendReturnUrl('/pages-fruit-game/prepare/index?replay=1') })
    },
    returnToCaller(): void {
      returnToCaller({ delta: 1, returnUrl: getCallerReturnUrl() })
    }
  }
}
</script>

<style scoped>
.rfg-result { position: relative; box-sizing: border-box; display: flex; align-items: center; justify-content: center; width: 100vw; height: 100vh; height: 100dvh; padding: var(--rfg-safe-top, 8px) 12px 12px; overflow: hidden; color: #58350e; font-family: "Microsoft YaHei", sans-serif; }
.rfg-result__background, .rfg-result__shade { position: absolute; inset: 0; width: 100%; height: 100%; }
.rfg-result__shade { background: rgba(31, 68, 19, 0.26); }
.rfg-result__card { position: relative; z-index: 2; box-sizing: border-box; width: min(900px, 94vw); max-height: calc(100vh - var(--rfg-safe-top, 8px) - 18px); max-height: calc(100dvh - var(--rfg-safe-top, 8px) - 18px); padding: clamp(12px, 2.5vh, 22px) clamp(18px, 3vw, 34px) clamp(14px, 2.8vh, 24px); overflow-x: hidden; overflow-y: auto; border: 4px solid #b06a17; border-radius: 22px; background: rgba(255, 250, 225, 0.96); box-shadow: 0 9px 0 rgba(83, 44, 8, 0.26), 0 18px 48px rgba(25, 58, 16, 0.3); }
.rfg-result__header { display: flex; align-items: baseline; justify-content: space-between; gap: 18px; border-bottom: 2px solid #e7c77a; padding-bottom: 8px; }
.rfg-result__title { flex: none; color: #2c7829; font-size: clamp(26px, 5vh, 40px); font-weight: 900; line-height: 1.1; }
.rfg-result__subtitle { max-width: 58%; color: #74664f; font-size: clamp(11px, 1.8vh, 14px); text-align: right; }
.rfg-result__columns { display: grid; grid-template-columns: 1.7fr 1fr; gap: 16px; margin-top: clamp(9px, 2vh, 16px); }
.rfg-result__section { padding: 10px 14px; border-radius: 14px; background: #fffdf2; }
.rfg-result__section--game { background: #eff9dc; }
.rfg-result__section-title { color: #39772a; font-size: clamp(14px, 2.4vh, 19px); font-weight: 900; }
.rfg-result__grid { display: grid; grid-template-columns: repeat(3, 1fr); gap: 7px; margin-top: 8px; }
.rfg-result__metric { display: flex; flex-direction: column; min-width: 0; padding: 6px 8px; border-radius: 9px; background: #fff4cd; color: #816d4e; font-size: clamp(10px, 1.7vh, 13px); white-space: nowrap; }
.rfg-result__value { margin-top: 2px; color: #6a3a09; font-size: clamp(18px, 3.3vh, 25px); font-weight: 900; }
.rfg-result__score { display: flex; align-items: baseline; justify-content: center; color: #d45a09; font-size: clamp(16px, 2.7vh, 22px); font-weight: 900; line-height: 1; }
.rfg-result__score-number { font-size: clamp(34px, 6vh, 48px); }
.rfg-result__game-row { display: flex; justify-content: space-between; gap: 12px; padding: 4px 6px; border-bottom: 1px solid #d8e9bc; font-size: clamp(11px, 1.9vh, 15px); font-weight: 700; white-space: nowrap; }
.rfg-result__actions { display: flex; justify-content: center; gap: 18px; margin-top: clamp(10px, 2.3vh, 18px); }
.rfg-result__button { width: 170px; margin: 0; border-radius: 999px; color: #fff; font-size: clamp(16px, 2.8vh, 21px); font-weight: 900; line-height: 2; }
.rfg-result__button--again { background: linear-gradient(#91df3a, #4ca216); box-shadow: 0 4px 0 #347510; }
.rfg-result__button--back { background: linear-gradient(#64bdf5, #2788cf); box-shadow: 0 4px 0 #1f6598; }
.rfg-result__button::after { border: 0; }

/* 微信开发者工具偶尔不会在页面切换后立即应用横屏方向。
   竖屏时使用紧凑单列兜底，避免双栏和三列指标按最小内容宽度互相挤压。 */
@media (orientation: portrait), (max-width: 520px) {
  .rfg-result {
    align-items: flex-start;
    padding: calc(var(--rfg-safe-top, 8px) + 8px) 10px 10px;
  }

  .rfg-result__card {
    width: calc(100vw - 20px);
    max-height: calc(100vh - var(--rfg-safe-top, 8px) - 18px);
    max-height: calc(100dvh - var(--rfg-safe-top, 8px) - 18px);
    padding: 14px 16px 16px;
    overflow-y: auto;
    border-width: 3px;
    border-radius: 18px;
  }

  .rfg-result__header {
    align-items: flex-start;
    gap: 10px;
    padding-bottom: 7px;
  }

  .rfg-result__title {
    font-size: 30px;
    line-height: 1.12;
  }

  .rfg-result__subtitle {
    max-width: 48%;
    font-size: 12px;
    line-height: 1.35;
  }

  .rfg-result__columns {
    display: flex;
    flex-direction: column;
    gap: 8px;
    margin-top: 8px;
  }

  .rfg-result__section {
    box-sizing: border-box;
    width: 100%;
    padding: 9px 10px;
  }

  .rfg-result__section-title { font-size: 15px; }
  .rfg-result__grid {
    grid-template-columns: repeat(2, minmax(0, 1fr));
    gap: 6px;
    margin-top: 6px;
  }

  .rfg-result__metric {
    min-width: 0;
    min-height: 48px;
    padding: 5px 7px;
    font-size: 11px;
    line-height: 1.25;
    white-space: normal;
  }

  .rfg-result__value { font-size: 19px; }
  .rfg-result__score-number { font-size: 38px; }
  .rfg-result__game-row {
    gap: 8px;
    padding: 4px 5px;
    font-size: 12px;
    line-height: 1.3;
    white-space: normal;
  }

  .rfg-result__game-row text:last-child {
    flex: none;
    text-align: right;
  }

  .rfg-result__actions {
    gap: 10px;
    margin-top: 10px;
  }

  .rfg-result__button {
    flex: 1;
    width: auto;
    height: 46px;
    padding: 0 8px;
    font-size: 16px;
    line-height: 40px;
    white-space: nowrap;
  }
}

@media (max-height: 390px) {
  .rfg-result__card { width: min(860px, 94vw); padding: 10px 24px 12px; }
  .rfg-result__header { padding-bottom: 5px; }
  .rfg-result__columns { gap: 10px; margin-top: 7px; }
  .rfg-result__section { padding: 7px 10px; }
  .rfg-result__grid { gap: 5px; margin-top: 5px; }
  .rfg-result__metric { padding: 4px 6px; }
  .rfg-result__game-row { padding: 2px 5px; }
  .rfg-result__actions { margin-top: 8px; }
  .rfg-result__button { width: 150px; line-height: 1.8; }
}
</style>
