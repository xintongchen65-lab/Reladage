<template>
  <view class="vp-game">
    <view class="vp-game__viewport" :style="stageCss">
    <!-- Three visual depth layers: far / mid / foreground. -->
    <image class="vp-game__bg vp-game__bg--far" :src="assets.backgrounds.far" mode="aspectFill" />
    <image class="vp-game__bg vp-game__bg--mid" :src="assets.backgrounds.mid" mode="aspectFill" />

    <!-- Activated park events live inside the scene, never as floating icon cards. -->
    <view class="vp-game__events">
      <image
        v-for="event in eventNames"
        :key="event"
        class="vp-game__event"
        :class="[`vp-game__event--${event}`, { 'vp-game__event--active': state.activatedEvents.includes(event as any) }]"
        :src="assets.events[event]"
        mode="aspectFit"
      />
    </view>

    <image class="vp-game__bg vp-game__bg--front" :src="assets.backgrounds.foreground" mode="aspectFill" />

    <!-- Bench is an independent fixed layer for the whole training session. -->
    <image class="vp-game__bench" :src="assets.bench" mode="aspectFit" />
    <image class="vp-game__ground-shadow" :src="assets.effects.groundShadow" mode="aspectFit" />

    <!-- All six poses stay mounted and preloaded; only opacity changes. -->
    <view class="vp-game__stage">
      <view class="vp-game__pose-stack">
        <image
          v-for="pose in poseNames"
          :key="pose"
          class="vp-game__pose"
          :class="{ 'vp-game__pose--active': pose === currentPose }"
          :src="assets.poses[pose]"
          mode="aspectFit"
        />
      </view>
    </view>

    <view class="vp-game__hud">
      <view class="vp-game__progress-block">
        <view class="vp-game__progress-title">
          <text>总进度</text>
          <text>{{ Math.round(frame.overall_completion_percent) }}%</text>
        </view>
        <view class="vp-game__track">
          <view :style="{ width: `${Math.max(0, Math.min(100, frame.overall_completion_percent))}%` }" />
        </view>
      </view>
      <view class="vp-game__stat">
        <text class="vp-game__stat-label">本组次数</text>
        <text>{{ currentCount }}/{{ frame.target_count }}</text>
      </view>
      <view class="vp-game__stat">
        <text class="vp-game__stat-label">组数</text>
        <text>{{ frame.set_index }}/{{ frame.target_sets }}</text>
      </view>
      <button class="vp-game__pause" @click="togglePause">{{ paused ? '继续' : '暂停' }}</button>
      <button class="vp-game__exit" @click="confirmStop">退出</button>
    </view>

    <view class="vp-game__action-prompt">
      <text>{{ actionPrompt }}</text>
    </view>

    <view v-if="feedbackVisible" class="vp-game__feedback" :key="state.effectId">
      <text>{{ state.feedback }}</text>
    </view>

    <view v-if="dataInterrupted" class="vp-game__overlay">
      <view class="vp-game__overlay-card">
        <text class="vp-game__overlay-title">动作数据中断</text>
        <text>请检查设备连接，恢复后重新坐稳再继续</text>
        <view class="vp-game__overlay-actions"><button class="vp-game__overlay-exit" @click="confirmStop">退出训练</button></view>
      </view>
    </view>
    <view v-else-if="frame.training_state === 'REST'" class="vp-game__overlay vp-game__overlay--rest">
      <view class="vp-game__overlay-card">
        <text class="vp-game__overlay-title">本组完成</text>
        <text>休息 {{ frame.rest_remaining_sec }} 秒</text>
        <text class="vp-game__overlay-sub">长椅与已激活的公园状态会保持不变</text>
        <view class="vp-game__overlay-actions"><button class="vp-game__overlay-exit" @click="confirmStop">退出训练</button></view>
      </view>
    </view>
    <view v-else-if="paused" class="vp-game__overlay">
      <view class="vp-game__overlay-card">
        <text class="vp-game__overlay-title">训练已暂停</text>
        <view class="vp-game__overlay-actions"><button @click="togglePause">继续训练</button><button class="vp-game__overlay-exit" @click="confirmStop">退出训练</button></view>
      </view>
    </view>

    <view v-if="debugEnabled" class="vp-game__debug">
      <button @touchstart="hold('leftFlex', true)" @touchend="hold('leftFlex', false)">起立</button>
      <button @touchstart="hold('leftExtend', true)" @touchend="hold('leftExtend', false)">坐下</button>
      <button @click="finish('FINISHED')">完成</button>
      <button @click="finish('STOPPED')">结束</button>
      <button @click="resetSource">重置</button>
    </view>
    </view>
  </view>
</template>

<script lang="ts">
import { isAngleControllableSource, isSessionAwareSource, type MotionDataSource } from '../../game-platform/motion/data-source'
import { MotionSessionGuard, type SessionGuardState } from '../../game-platform/motion/session-guard'
import { appendReturnUrl, configureCallerReturnUrl } from '../../game-platform/runtime/navigation'
import { createVitalityDataSource } from '../data-sources/source-factory'
import { VitalityKeyboardController } from '../data-sources/h5-keyboard-controller'
import { createInitialVitalityFrame, type VitalityMotionFrame } from '../types/motion'
import { getVitalityConfig, publishVitalityResult } from '../runtime/session-runtime'
import { VITALITY_ASSETS } from '../runtime/asset-paths'
import { gameStageStyle, getViewportLayout } from '../../game-platform/runtime/viewport'
import { createInitialVitalityState, createVitalityResult, reduceVitalityFrame, VITALITY_EVENTS, VitalityRepReconciler, VitalityTrainingAccumulator, type VitalityGameState } from '../core/game-engine'
import { decidePoseFromFrame, type VitalityMotionDirection, type VitalityPose } from '../core/pose-mapper'

export default {
  name: 'VitalityParkGame',
  data() {
    const config = getVitalityConfig()
    return {
      assets: VITALITY_ASSETS,
      poseNames: ['sitting', 'lean-forward', 'lift-off', 'half-standing', 'standing', 'sit-back'] as VitalityPose[],
      eventNames: [...VITALITY_EVENTS] as string[],
      frame: createInitialVitalityFrame(),
      state: createInitialVitalityState() as VitalityGameState,
      currentPose: 'sitting' as VitalityPose,
      previousProgress: 0,
      motionDirection: 'idle' as VitalityMotionDirection,
      debugEnabled: false,
      viewport: getViewportLayout(),
      guard: new MotionSessionGuard<VitalityMotionFrame>({ dataTimeoutMs: config.dataTimeoutMs, returnAngleDeg: config.returnAngleDeg }),
      guardState: 'REARMING' as SessionGuardState,
      feedbackVisible: false,
      ending: false,
      lastTick: Date.now(),
      timer: null as ReturnType<typeof setInterval> | null,
      feedbackTimer: null as ReturnType<typeof setTimeout> | null,
      source: null as MotionDataSource<VitalityMotionFrame> | null,
      unsubscribe: null as (() => void) | null,
      keyboard: null as VitalityKeyboardController | null,
      accumulator: new VitalityTrainingAccumulator(),
      reconciler: new VitalityRepReconciler()
    }
  },
  computed: {
    stageCss(): Record<string, string> {
      return gameStageStyle(this.viewport, '--vp-safe-top')
    },
    paused(): boolean {
      return ['USER_PAUSED', 'BACKGROUND_PAUSED', 'SOURCE_PAUSED'].includes(this.guardState)
    },
    dataInterrupted(): boolean {
      return this.guardState === 'DATA_INTERRUPTED'
    },
    currentCount(): number {
      return Math.max(this.frame.left_count, this.frame.right_count)
    },
    actionPrompt(): string {
      if (this.dataInterrupted) return '等待动作数据恢复'
      if (this.frame.training_state === 'REST') return '请坐稳休息'
      if (this.paused || this.frame.training_state === 'PAUSED') return '训练已暂停'
      if (this.motionDirection === 'descending') {
        if (this.currentPose === 'standing') return '站稳后缓慢坐回'
        if (this.currentPose === 'half-standing') return '控制速度，继续下坐'
        if (this.currentPose === 'lift-off') return '缓慢接近椅面'
        return '坐稳后准备下一次'
      }
      if (this.currentPose === 'sitting') return '请坐稳，准备起身'
      if (this.currentPose === 'lean-forward') return '身体前倾，准备离椅'
      if (this.currentPose === 'lift-off') return '离开椅面，保持稳定'
      if (this.currentPose === 'half-standing') return '继续起身'
      return '保持站稳，然后缓慢坐回'
    }
  },
  onLoad(query: Record<string, string | undefined>) {
    configureCallerReturnUrl(query?.returnUrl, '/pages-vitality-park/home/index')
    const cfg = getVitalityConfig()
    this.debugEnabled = cfg.debugEnabled
    this.viewport = getViewportLayout()
    this.source = createVitalityDataSource(cfg)
    this.unsubscribe = this.source.subscribe((frame) => this.acceptFrame(frame))
    this.source.start()
    this.lastTick = Date.now()
    this.timer = setInterval(() => this.tick(), 40)

    /* #ifdef H5 */
    if (isAngleControllableSource(this.source)) {
      this.keyboard = new VitalityKeyboardController(this.source, this.debugEnabled, {
        pause: () => this.togglePause(),
        finish: () => this.finish('FINISHED'),
        stop: () => this.finish('STOPPED'),
        reset: () => this.resetSource()
      })
      this.keyboard.attach()
    }
    /* #endif */
  },
  onShow() {
    this.viewport = getViewportLayout()
    if (this.guardState === 'BACKGROUND_PAUSED') {
      if (this.source && isSessionAwareSource(this.source)) this.source.setPaused(false)
      this.guard.resume()
      this.guardState = this.guard.getState()
    }
  },
  onHide() {
    if (this.ending || ['USER_PAUSED', 'RESTING', 'SOURCE_PAUSED', 'DATA_INTERRUPTED'].includes(this.guardState)) return
    this.guard.pauseForBackground()
    if (this.source && isSessionAwareSource(this.source)) this.source.setPaused(true)
    this.guardState = this.guard.getState()
  },
  onResize() {
    this.viewport = getViewportLayout()
  },
  onBackPress() {
    this.confirmStop()
    return true
  },
  onUnload() {
    if (!this.ending) this.finish('STOPPED', false)
    this.cleanup()
  },
  methods: {


    acceptFrame(frame: VitalityMotionFrame) {
      this.guard.acceptFrame(frame)
      this.guardState = this.guard.getState()
      const repDecision = this.reconciler.accept(frame)
      const acceptedEvent = repDecision.accepted && this.guard.canScore('sit_to_stand_done')
      const effective = acceptedEvent && frame.rep_event === 'none' ? { ...frame, rep_event: 'sit_to_stand_done' as const } : frame

      const transition = reduceVitalityFrame(this.state, effective, this.accumulator, acceptedEvent)
      this.state = transition.state
      this.frame = frame

      const decision = decidePoseFromFrame(frame, this.previousProgress, this.motionDirection)
      this.previousProgress = decision.progress
      this.motionDirection = decision.direction
      this.currentPose = decision.pose

      if (transition.accepted) this.showFeedback()

      if (frame.training_state === 'FINISHED') this.finish('FINISHED')
      else if (frame.training_state === 'STOPPED') this.finish('STOPPED')
    },
    showFeedback() {
      this.feedbackVisible = true
      if (this.feedbackTimer) clearTimeout(this.feedbackTimer)
      this.feedbackTimer = setTimeout(() => {
        this.feedbackVisible = false
      }, 1350)
    },
    tick() {
      const now = Date.now()
      this.lastTick = now
      if (this.guard.checkTimeout(now)) this.guardState = this.guard.getState()
    },
    togglePause() {
      if (this.ending || !this.source) return
      if (this.guardState === 'USER_PAUSED') {
        if (isSessionAwareSource(this.source)) this.source.setPaused(false)
        this.guard.resume()
      } else {
        this.guard.pauseByUser()
        if (isSessionAwareSource(this.source)) this.source.setPaused(true)
        this.state = { ...this.state, activeState: 'PAUSED', feedback: '训练已暂停' }
      }
      this.guardState = this.guard.getState()
    },
    confirmStop() {
      if (this.ending) return
      uni.showModal({
        title: '退出训练',
        content: '是否提前退出并保留本次训练数据？',
        confirmText: '退出',
        cancelText: '继续训练',
        success: (result) => { if (result.confirm) this.finish('STOPPED') }
      })
    },
    hold(control: 'leftFlex' | 'leftExtend', pressed: boolean) {
      if (this.source && isAngleControllableSource(this.source)) this.source.setHeldControl(control, pressed)
    },
    resetSource() {
      this.source?.reset()
      this.accumulator.reset()
      this.reconciler.reset()
      this.state = createInitialVitalityState()
      this.frame = createInitialVitalityFrame()
      this.currentPose = 'sitting'
      this.previousProgress = 0
      this.motionDirection = 'idle'
      const config = getVitalityConfig()
      this.guard = new MotionSessionGuard<VitalityMotionFrame>({ dataTimeoutMs: config.dataTimeoutMs, returnAngleDeg: config.returnAngleDeg })
      this.guardState = this.guard.getState()
      this.feedbackVisible = false
    },
    finish(reason: 'FINISHED' | 'STOPPED', navigate = true) {
      if (this.ending) return
      this.ending = true
      if (this.source && isSessionAwareSource(this.source)) this.source.setPaused(true)
      this.guard.finish()
      const result = createVitalityResult(reason, this.frame, this.state, this.accumulator, this.guard.getTotalElapsedMs(), this.guard.getActiveElapsedMs())
      publishVitalityResult(result)
      if (navigate) uni.redirectTo({ url: appendReturnUrl('/pages-vitality-park/result/index') })
    },
    cleanup() {
      if (this.timer) clearInterval(this.timer)
      if (this.feedbackTimer) clearTimeout(this.feedbackTimer)
      this.timer = null
      this.feedbackTimer = null
      this.keyboard?.detach()
      this.unsubscribe?.()
      this.unsubscribe = null
      this.source?.stop()
      this.source = null
    }
  }
}
</script>

<style scoped>
.vp-game {
  position: relative;
  display: flex;
  align-items: center;
  justify-content: center;
  width: 100vw;
  height: 100vh;
  height: 100dvh;
  overflow: hidden;
  background: #315b42;
  color: #fff;
  font-family: "Microsoft YaHei", "PingFang SC", sans-serif;
}
.vp-game__viewport {
  position: relative;
  flex: 0 0 auto;
  width: 1280px;
  height: 720px;
  max-width: none;
  max-height: none;
  overflow: hidden;
  background: #8fcf68;
}

.vp-game__bg {
  position: absolute;
  inset: 0;
  width: 100%;
  height: 100%;
  pointer-events: none;
}
.vp-game__bg--far { z-index: 0; }
.vp-game__bg--mid { z-index: 1; }
.vp-game__bg--front { z-index: 4; }

.vp-game__events {
  position: absolute;
  z-index: 3;
  inset: 0;
  pointer-events: none;
}
.vp-game__event {
  position: absolute;
  opacity: 0;
  transform: translateY(7px) scale(.97);
  transition: opacity 420ms ease, transform 420ms ease;
  object-fit: contain;
}
.vp-game__event--active { opacity: 1; transform: translateY(0) scale(1); }
.vp-game__event--bird { left: 16%; top: 23%; width: 8.4vh; height: 7vh; }
.vp-game__event--flowers { left: 17%; bottom: 10%; width: 26vh; height: 9.5vh; }
.vp-game__event--lamp { right: 8%; bottom: 14%; width: 7.4vh; height: 28vh; }
.vp-game__event--butterfly { left: 28%; top: 47%; width: 7.3vh; height: 7vh; }
.vp-game__event--fountain { left: 59%; bottom: 16%; width: 30vh; height: 18vh; }
.vp-game__event--kite { right: 20%; top: 15%; width: 13.3vh; height: 15vh; }
.vp-game__event--dog { right: 5%; bottom: 9%; width: 14vh; height: 11vh; }
.vp-game__event--flags { left: 13%; top: 12%; width: 23vh; height: 8vh; }
.vp-game__event--rainbow { right: 8%; top: 8%; width: 25vh; height: 13vh; }
.vp-game__event--celebration { left: 50%; top: 12%; width: 46vh; height: 25vh; transform: translateX(-50%) translateY(7px) scale(.97); }
.vp-game__event--celebration.vp-game__event--active { transform: translateX(-50%) translateY(0) scale(1); }

.vp-game__bench {
  position: absolute;
  z-index: 6;
  left: 50%;
  bottom: 9vh;
  width: 55vh;
  height: 30vh;
  transform: translateX(-50%);
  object-fit: contain;
  pointer-events: none;
}
.vp-game__ground-shadow {
  position: absolute;
  z-index: 6;
  left: 50%;
  bottom: 3.2vh;
  width: 37vh;
  height: 8vh;
  transform: translateX(-50%);
  opacity: .62;
  pointer-events: none;
}
.vp-game__stage {
  position: absolute;
  z-index: 7;
  left: 50%;
  bottom: 9vh;
  width: 37.5vh;
  height: 62vh;
  transform: translateX(-50%);
  pointer-events: none;
}
.vp-game__pose-stack { position: relative; width: 100%; height: 100%; }
.vp-game__pose {
  position: absolute;
  inset: 0;
  width: 100%;
  height: 100%;
  opacity: 0;
  transition: opacity 105ms linear;
  will-change: opacity;
}
.vp-game__pose--active { opacity: 1; }

.vp-game__hud {
  position: absolute;
  z-index: 20;
  top: 8px;
  left: 12px;
  right: 12px;
  display: flex;
  align-items: center;
  gap: 12px;
  min-height: 44px;
  padding: 5px 9px;
  border: 1px solid rgba(255,255,255,.3);
  border-radius: 13px;
  background: rgba(25, 82, 42, .82);
  box-shadow: 0 4px 16px rgba(18, 54, 28, .18);
  backdrop-filter: blur(5px);
  font-size: 13px;
  font-weight: 800;
}
.vp-game__progress-block { flex: 1; min-width: 150px; }
.vp-game__progress-title { display: flex; justify-content: space-between; gap: 12px; }
.vp-game__track { height: 8px; margin-top: 4px; overflow: hidden; border-radius: 999px; background: rgba(255,255,255,.3); }
.vp-game__track view { height: 100%; border-radius: inherit; background: linear-gradient(90deg,#83e15c,#ffd54d); }
.vp-game__stat { display: flex; flex-direction: column; min-width: 68px; text-align: center; line-height: 1.15; }
.vp-game__stat-label { margin-bottom: 2px; color: rgba(255,255,255,.78); font-size: 10px; font-weight: 600; }
.vp-game__pause,
.vp-game__exit {
  display: flex;
  align-items: center;
  justify-content: center;
  width: 66px;
  height: 34px;
  margin: 0;
  padding: 0;
  border: 0;
  border-radius: 999px;
  background: #ffd24a;
  color: #62480a;
  font-size: 13px;
  font-weight: 900;
}
.vp-game__exit { background: rgba(255,255,255,.94); color:#315f45; }
.vp-game__pause::after,.vp-game__exit::after { border:0; }

.vp-game__action-prompt {
  position: absolute;
  z-index: 20;
  left: 50%;
  bottom: 12px;
  width: 46vw; max-width: 340px;
  padding: 7px 13px;
  border: 1px solid rgba(255,255,255,.32);
  border-radius: 999px;
  background: rgba(25, 74, 40, .75);
  box-shadow: 0 3px 12px rgba(25,64,33,.18);
  font-size: 12px;
  font-weight: 800;
  text-align: center;
  transform: translateX(-50%);
}
.vp-game__feedback {
  position: absolute;
  z-index: 22;
  left: 50%;
  top: 18%;
  transform: translateX(-50%);
  padding: 5px 12px;
  border-radius: 999px;
  background: rgba(255,250,219,.92);
  color: #4c7a34;
  box-shadow: 0 3px 12px rgba(62,88,37,.16);
  font-size: 12px;
  font-weight: 900;
  animation: vp-feedback 1.35s ease both;
}
@keyframes vp-feedback {
  0% { opacity: 0; transform: translate(-50%, 6px) scale(.96); }
  16%,72% { opacity: 1; transform: translate(-50%, 0) scale(1); }
  100% { opacity: 0; transform: translate(-50%, -5px) scale(.98); }
}

.vp-game__overlay {
  position: absolute;
  z-index: 30;
  inset: 0;
  display: flex;
  align-items: center;
  justify-content: center;
  background: rgba(24, 65, 33, .43);
}
.vp-game__overlay--rest { background: rgba(31, 64, 48, .32); }
.vp-game__overlay-card {
  display: flex;
  min-width: 240px;
  max-width: 70vw;
  padding: 20px 24px;
  flex-direction: column;
  align-items: center;
  gap: 7px;
  border: 1px solid rgba(255,255,255,.5);
  border-radius: 16px;
  background: rgba(38, 84, 55, .88);
  box-shadow: 0 12px 34px rgba(15,42,23,.2);
  text-align: center;
}
.vp-game__overlay-title { font-size: 22px; font-weight: 900; }
.vp-game__overlay-sub { color: rgba(255,255,255,.72); font-size: 11px; }
.vp-game__overlay button { height: 36px; margin-top: 7px; border-radius: 999px; background: #8fe568; color: #24502c; font-size: 13px; font-weight: 900; }


.vp-game__overlay-actions { display:flex; justify-content:center; gap:10px; margin-top:14px; }
.vp-game__overlay-actions button { min-width:130px; height:40px; margin:0; border:0; border-radius:999px; background:#58b632; color:#fff; font-weight:900; }
.vp-game__overlay-actions .vp-game__overlay-exit { background:#edf4ee; color:#3f6049; }
.vp-game__overlay-actions button::after { border:0; }
.vp-game__debug {
  position: absolute;
  z-index: 40;
  left: 8px;
  bottom: 8px;
  display: flex;
  gap: 5px;
}
.vp-game__debug button { height: 30px; margin: 0; padding: 0 10px; font-size: 11px; line-height: 30px; }

@media screen and (max-height: 390px) {
  .vp-game__hud { top: 5px; min-height: 42px; padding: 5px 8px; gap: 8px; }
  .vp-game__stat { min-width: 56px; }
  .vp-game__bench { bottom: 9vh; width: 53vh; height: 29vh; }
  .vp-game__stage { bottom: 9vh; width: 36.3vh; height: 60vh; }
  .vp-game__action-prompt { bottom: 7px; padding: 5px 10px; font-size: 11px; }
  .vp-game__feedback { top: 17%; }
}

@media screen and (min-width: 900px) {
  .vp-game__hud { left: 18px; right: 18px; }
}

/* Shared 1280x720 stage overrides. */
.vp-game__event--bird { width: 60px; height: 50px; }
.vp-game__event--flowers { width: 187px; height: 68px; }
.vp-game__event--lamp { width: 53px; height: 202px; }
.vp-game__event--butterfly { width: 53px; height: 50px; }
.vp-game__event--fountain { width: 216px; height: 130px; }
.vp-game__event--kite { width: 96px; height: 108px; }
.vp-game__event--dog { width: 101px; height: 79px; }
.vp-game__event--flags { width: 166px; height: 58px; }
.vp-game__event--rainbow { width: 180px; height: 94px; }
.vp-game__event--celebration { width: 331px; height: 180px; }
.vp-game__bench { bottom: 65px; width: 396px; height: 216px; }
.vp-game__ground-shadow { bottom: 23px; width: 266px; height: 58px; }
.vp-game__stage { bottom: 65px; width: 270px; height: 446px; }
.vp-game__hud { top: var(--vp-safe-top, 8px); left: var(--game-safe-left, 12px); right: var(--game-safe-right, 12px); min-height: 44px; padding: 5px 9px; gap: 12px; }
.vp-game__stat { min-width: 68px; }
.vp-game__action-prompt { bottom: 12px; width: 340px; max-width: 340px; padding: 7px 13px; font-size: 12px; }
.vp-game__overlay-card { max-width: 896px; }
</style>
