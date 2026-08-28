<template>
  <view class="rfg-prepare" :style="viewportCssVars">
    <image class="rfg-prepare__background" :src="backgroundSrc" mode="aspectFill" />
    <view class="rfg-prepare__shade" />
    <button class="rfg-prepare__back" @click="exitPrepare">← 返回</button>

    <view class="rfg-prepare__panel">
      <text class="rfg-prepare__eyebrow">RehabMotion 康复游戏</text>
      <text class="rfg-prepare__title">摘水果</text>
      <text class="rfg-prepare__exercise">训练动作：肘关节屈伸</text>
      <view class="rfg-prepare__targets">
        <view class="rfg-prepare__target"><text>训练组数</text><text class="rfg-prepare__number">{{ config.targetSets }} 组</text></view>
        <view class="rfg-prepare__target"><text>每组左右目标</text><text class="rfg-prepare__number">各 {{ config.targetCount }} 次</text></view>
      </view>
      <button class="rfg-prepare__button" :disabled="counting" @click="startCountdown">
        {{ counting ? '准备开始' : '开始训练' }}
      </button>
      <text class="rfg-prepare__tip">请横屏使用，并确保左右手臂有足够活动空间</text>
      <text class="rfg-prepare__parameters">目标 {{ config.targetAngleDeg }}° · 有效 ≥ {{ config.validAngleDeg }}° · 返回 ≤ {{ config.returnAngleDeg }}°</text>
    </view>

    <view v-if="counting" class="rfg-prepare__countdown">
      <text class="rfg-prepare__countdown-number">{{ countdown }}</text>
      <text class="rfg-prepare__countdown-label">保持坐姿，准备训练</text>
    </view>
  </view>
</template>

<script lang="ts">
import { getSessionConfig, prepareReplaySession, registerResultEmitter } from '../runtime/session-runtime'
import { configureSessionFromQuery } from '../runtime/launch-config'
import { FRUIT_GAME_ASSETS } from '../runtime/asset-paths'
import { appendReturnUrl, configureCallerReturnUrl, returnToCaller } from '../runtime/navigation-runtime'
import { getViewportLayout, viewportStyle } from '../runtime/viewport-layout'
import { clearMultiplayerRuntime } from '../runtime/multiplayer-runtime'

export default {
  name: 'RfgPreparePage',
  data() {
    return {
      backgroundSrc: FRUIT_GAME_ASSETS.orchard.day,
      config: getSessionConfig(),
      viewportLayout: getViewportLayout(),
      counting: false,
      returning: false,
      countdown: 3,
      countdownTimer: null as ReturnType<typeof setInterval> | null
    }
  },
  onLoad(query: Record<string, string | undefined>) {
    const launchQuery = query || {}
    if (launchQuery.replay === '1') {
      if (launchQuery.returnUrl) configureCallerReturnUrl(launchQuery.returnUrl)
      this.config = prepareReplaySession()
      return
    }
    clearMultiplayerRuntime()
    configureCallerReturnUrl(launchQuery.returnUrl)
    this.config = configureSessionFromQuery(launchQuery)
    try {
      const eventChannel = (this as any).getOpenerEventChannel()
      registerResultEmitter((result) => eventChannel.emit('fruitGameResult', result))
    } catch {
      registerResultEmitter(null)
    }
  },
  onUnload() {
    this.clearCountdown()
  },
  onBackPress() {
    this.exitPrepare()
    return true
  },
  onResize() {
    this.viewportLayout = getViewportLayout()
  },
  computed: {
    viewportCssVars(): Record<string, string> {
      return viewportStyle(this.viewportLayout)
    }
  },
  methods: {
    startCountdown(): void {
      if (this.counting) return
      this.counting = true
      this.countdown = 3
      this.countdownTimer = setInterval(() => {
        if (this.countdown > 1) {
          this.countdown -= 1
          return
        }
        this.clearCountdown()
        uni.redirectTo({ url: appendReturnUrl('/pages-fruit-game/game/index') })
      }, 1000)
    },
    clearCountdown(): void {
      if (this.countdownTimer) clearInterval(this.countdownTimer)
      this.countdownTimer = null
    },
    exitPrepare(): void {
      if (this.returning) return
      this.returning = true
      this.clearCountdown()
      registerResultEmitter(null)
      returnToCaller()
    }
  }
}
</script>

<style scoped>
.rfg-prepare {
  position: relative;
  box-sizing: border-box;
  display: flex;
  align-items: center;
  justify-content: center;
  width: 100vw;
  height: 100vh;
  height: 100dvh;
  padding: var(--rfg-safe-top, 8px) 12px 12px;
  overflow: hidden;
  color: #57300e;
  font-family: "Microsoft YaHei", sans-serif;
}

.rfg-prepare__background,
.rfg-prepare__shade { position: absolute; inset: 0; width: 100%; height: 100%; }
.rfg-prepare__shade { background: rgba(42, 75, 24, 0.18); }
.rfg-prepare__back { position:absolute; top:calc(var(--rfg-safe-top,8px) + 6px); left:12px; z-index:12; display:flex; align-items:center; justify-content:center; width:80px; height:36px; margin:0; padding:0 8px; border-radius:999px; background:rgba(255,249,220,.96); color:#397527; font-size:14px; font-weight:900; line-height:1; white-space:nowrap; box-shadow:0 3px 0 rgba(66,101,33,.25); }
.rfg-prepare__back::after { border:0; }

.rfg-prepare__panel {
  position: relative;
  z-index: 2;
  box-sizing: border-box;
  display: flex;
  flex-direction: column;
  align-items: center;
  width: min(560px, 88vw);
  max-height: calc(100vh - var(--rfg-safe-top, 8px) - 18px);
  max-height: calc(100dvh - var(--rfg-safe-top, 8px) - 18px);
  padding: clamp(12px, 2.4vh, 22px) clamp(22px, 4vw, 42px);
  overflow-y: auto;
  border: 4px solid #b56d1a;
  border-radius: 24px;
  background: rgba(255, 249, 220, 0.95);
  box-shadow: 0 10px 0 rgba(90, 48, 8, 0.25), 0 18px 48px rgba(27, 61, 18, 0.28);
}

.rfg-prepare__eyebrow { color: #559025; font-size: clamp(12px, 2vh, 16px); font-weight: 800; }
.rfg-prepare__title { margin-top: 2px; color: #287329; font-size: clamp(34px, 7vh, 56px); font-weight: 900; line-height: 1.05; text-shadow: 0 2px 0 #d8ed9b; }
.rfg-prepare__exercise { margin-top: 5px; font-size: clamp(14px, 2.4vh, 20px); font-weight: 700; }
.rfg-prepare__targets { display: flex; gap: 16px; margin: clamp(10px, 2.5vh, 20px) 0; }
.rfg-prepare__target { display: flex; flex: 1; flex-direction: column; align-items: center; min-width: 124px; padding: 7px 14px; border: 2px solid #e3be66; border-radius: 13px; background: #fffdf0; font-size: clamp(12px, 2vh, 16px); }
.rfg-prepare__number { margin-top: 2px; color: #d25d10; font-size: clamp(21px, 3.7vh, 30px); font-weight: 900; }

.rfg-prepare__button {
  width: min(250px, 70%);
  height: clamp(42px, 8vh, 50px);
  margin: 0;
  padding: 0 14px;
  border-radius: 999px;
  background: linear-gradient(#92df3d, #4ca414);
  color: #fff;
  font-size: clamp(19px, 3.5vh, 27px);
  font-weight: 900;
  box-shadow: 0 5px 0 #347812;
}

.rfg-prepare__button::after { border: 0; }
.rfg-prepare__tip { margin-top: clamp(8px, 2vh, 16px); color: #786648; font-size: clamp(11px, 1.8vh, 14px); text-align: center; }
.rfg-prepare__parameters { margin-top: 4px; color: #92733c; font-size: clamp(10px, 1.7vh, 13px); }

.rfg-prepare__countdown {
  position: absolute;
  inset: 0;
  z-index: 10;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  background: rgba(21, 56, 16, 0.68);
}

.rfg-prepare__countdown-number { color: #fff; font-size: clamp(100px, 30vh, 190px); font-weight: 900; line-height: 1; text-shadow: 0 7px 0 #4f7d20; }
.rfg-prepare__countdown-label { margin-top: 10px; color: #fffbe1; font-size: clamp(18px, 4vh, 28px); font-weight: 800; }

@media (max-height: 390px) {
  .rfg-prepare__panel { width: min(520px, 86vw); padding: 9px 28px 10px; }
  .rfg-prepare__title { font-size: 34px; }
  .rfg-prepare__exercise { margin-top: 2px; font-size: 14px; }
  .rfg-prepare__targets { margin: 8px 0 10px; }
  .rfg-prepare__target { padding: 5px 12px; }
  .rfg-prepare__button { height: 40px; line-height: 1; }
  .rfg-prepare__tip { margin-top: 5px; }
}
</style>
