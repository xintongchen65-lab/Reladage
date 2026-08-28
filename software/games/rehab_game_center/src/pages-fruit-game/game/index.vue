<template>
  <view class="rfg-game" :style="viewportCssVars">
    <view class="rfg-game__stage">
      <image class="rfg-game__background" :src="orchardBackgroundSrc" mode="aspectFill" />

      <RfgGameHud
        :left-count="displayFrame.left_count"
        :right-count="displayFrame.right_count"
        :target-count="displayFrame.target_count"
        :left-angle="displayFrame.left_angle_deg"
        :right-angle="displayFrame.right_angle_deg"
        :progress="displayFrame.overall_completion_percent"
        :set-index="displayFrame.set_index"
        :target-sets="displayFrame.target_sets"
        :score="gameState.score"
        :combo="gameState.combo"
        :active-side="gameState.activeSide"
        :paused="paused"
        @pause="togglePause"
        @exit="requestStopTraining"
      />

      <view v-if="multiplayerSnapshot" class="rfg-game__multiplayer" :class="`rfg-game__multiplayer--target-${gameState.activeSide}`">
        <RfgPkLeaderboard v-if="multiplayerSnapshot.mode === 'PK'" :players="multiplayerSnapshot.players" :self-player-id="multiplayerSelfId" />
        <RfgCoopProgress v-else :snapshot="multiplayerSnapshot" />
      </view>

      <view class="rfg-game__target" :class="`rfg-game__target--${gameState.activeSide}`">
        <view class="rfg-game__target-visual">
          <view class="rfg-game__target-glow" />
          <image class="rfg-game__fruit" :src="fruitSrc" mode="aspectFit" />
        </view>
        <text class="rfg-game__target-label">{{ targetLabel }}</text>
      </view>

      <view v-if="gameState.bonusActive" class="rfg-game__bonus">
        <text class="rfg-game__bonus-time">双手奖励 {{ bonusSeconds }}s</text>
        <image class="rfg-game__bonus-fruit" :src="bothWatermelonSrc" mode="aspectFit" />
        <text class="rfg-game__bonus-label">双手同时屈肘 +500</text>
      </view>

      <RfgCharacterRig :left-progress="leftProgress" :right-progress="rightProgress" />

      <button class="rfg-game__basket-button" @click="openCollection">
        <image class="rfg-game__basket" :src="basketSrc" mode="heightFix" />
        <image v-if="effectVisible" class="rfg-game__basket-burst" :src="harvestBurstSrc" mode="aspectFit" />
        <text class="rfg-game__basket-label">查看水果篮</text>
      </button>
      <text class="rfg-game__feedback">{{ feedbackText }}</text>

      <view v-if="effectVisible" class="rfg-game__effect" :key="gameState.effectId">
        <image class="rfg-game__effect-burst" :src="harvestBurstSrc" mode="aspectFit" />
        <image class="rfg-game__effect-fruit" :src="effectFruitSrc" mode="aspectFit" />
        <image v-if="effectSpecial" class="rfg-game__effect-star" :src="rewardStarSrc" mode="aspectFit" />
        <image v-if="gameState.combo >= 3" class="rfg-game__effect-combo" :src="comboSrc" mode="aspectFit" />
        <text class="rfg-game__effect-score">+{{ effectPoints }}</text>
      </view>

      <view v-if="overlayVisible && !basketOpen" class="rfg-game__pause-overlay">
        <text class="rfg-game__pause-title">{{ overlayTitle }}</text>
        <text class="rfg-game__pause-message">{{ overlayMessage }}</text>
        <button v-if="guardState === 'USER_PAUSED'" class="rfg-game__resume" @click="togglePause">▶ 继续训练</button>
      </view>

      <RfgCollectionBook
        v-if="basketOpen"
        :inventory="gameState.inventory"
        :total="gameState.harvestedCount"
        :special-total="specialFruitTotal"
        :score="gameState.score"
        :safe-top="viewportLayout.safeTopPx"
        @close="closeCollection"
      />

      <text v-if="config.debugEnabled && !basketOpen" class="rfg-game__diagnostics">诊断 {{ diagnosticCount }}</text>
      <RfgDebugControls
        v-if="config.debugEnabled && !basketOpen"
        :expanded="debugExpanded"
        @toggle="debugExpanded = !debugExpanded"
        @hold="handleHeld"
        @pause="togglePause"
        @finish="developerFinish"
        @stop="developerStop"
        @reset="resetTraining"
      />
    </view>

    <view class="rfg-game__portrait-warning">
      <text class="rfg-game__portrait-icon">↻</text>
      <text class="rfg-game__portrait-text">请将设备横屏后继续训练</text>
    </view>
  </view>
</template>

<script lang="ts">
import RfgCharacterRig from '../components/CharacterRig.vue'
import RfgCollectionBook from '../components/CollectionBook.vue'
import RfgDebugControls from '../components/DebugControls.vue'
import RfgGameHud from '../components/GameHud.vue'
import RfgPkLeaderboard from '../components/PkLeaderboard.vue'
import RfgCoopProgress from '../components/CoopProgress.vue'
import {
  activateBonusForProgress,
  advanceComboTimer,
  basketStage,
  createInitialGameState,
  createTrainingResult,
  expireBonus,
  reduceGameState,
  resolveOrchardTheme,
  withOrchardTheme
} from '../core/game-engine'
import { TrainingAccumulator } from '../core/game-engine'
import type { CollectibleFruitName, GameState } from '../core/game-engine'
import { MotionFrameAdapter } from '../core/motion-adapter'
import { mapAngleToProgress } from '../core/motion-mapper'
import { SessionGuard, shouldAdvanceGameTimers } from '../core/session-guard'
import { createSeededRandom } from '../core/seeded-random'
import type { SessionGuardState } from '../core/session-guard'
import type { HeldControl, MotionDataSource } from '../data-sources/contracts'
import { isAngleControllableSource, isSessionAwareSource } from '../data-sources/contracts'
import { H5KeyboardController } from '../data-sources/h5-keyboard-controller'
import { createMotionDataSource } from '../data-sources/source-factory'
import { FRUIT_GAME_ASSETS } from '../runtime/asset-paths'
import { appendReturnUrl, configureCallerReturnUrl, returnToCaller } from '../runtime/navigation-runtime'
import { getSessionConfig, publishResult } from '../runtime/session-runtime'
import { getViewportLayout, viewportStyle } from '../runtime/viewport-layout'
import { clearMultiplayerRuntime, getMultiplayerClient, isMultiplayerSession, restoreMultiplayerRuntime, setLocalMultiplayerResult } from '../runtime/multiplayer-runtime'
import type { MotionFrame } from '../types/motion'
import { createInitialMotionFrame } from '../types/motion'
import type { RoomSnapshot } from '../types/multiplayer'

const FRUIT_PATHS = FRUIT_GAME_ASSETS.fruits
const HELD_CONTROLS: HeldControl[] = ['leftFlex', 'leftExtend', 'rightFlex', 'rightExtend']
const BONUS_DURATION_MS = 6000

export default {
  name: 'RfgGamePage',
  components: { RfgCharacterRig, RfgCollectionBook, RfgDebugControls, RfgGameHud, RfgPkLeaderboard, RfgCoopProgress },
  data() {
    const config = getSessionConfig()
    const startedAtMs = Date.now()
    const gameRandom = Math.random
    return {
      config,
      adapter: new MotionFrameAdapter({ gapDiagnosticMs: config.dataTimeoutMs }),
      guard: new SessionGuard({
        dataTimeoutMs: config.dataTimeoutMs,
        returnAngleDeg: config.returnAngleDeg,
        startedAtMs
      }),
      trainingAccumulator: new TrainingAccumulator(),
      source: null as MotionDataSource | null,
      unsubscribe: null as (() => void) | null,
      keyboard: null as H5KeyboardController | null,
      watchdogTimer: null as ReturnType<typeof setInterval> | null,
      displayFrame: createInitialMotionFrame(config.targetCount, config.targetSets),
      latestFrame: createInitialMotionFrame(config.targetCount, config.targetSets),
      visualFromFrame: createInitialMotionFrame(config.targetCount, config.targetSets),
      visualTargetFrame: createInitialMotionFrame(config.targetCount, config.targetSets),
      visualStartedAtMs: startedAtMs,
      visualDurationMs: 40,
      lastMotionTimestampMs: 0,
      gameRandom,
      gameState: createInitialGameState(gameRandom, resolveOrchardTheme()) as GameState,
      multiplayerSnapshot: null as RoomSnapshot | null,
      multiplayerUnsubscribe: null as (() => void) | null,
      lastMultiplayerSignature: '',
      guardState: 'REARMING' as SessionGuardState,
      leftProgress: 0,
      rightProgress: 0,
      viewportLayout: getViewportLayout(),
      debugExpanded: false,
      basketOpen: false,
      basketResumeOnClose: false,
      effectVisible: false,
      effectPoints: 0,
      effectFruit: 'apple' as CollectibleFruitName,
      effectTimer: null as ReturnType<typeof setTimeout> | null,
      bonusRemainingMs: 0,
      lastRuntimeTickAtMs: startedAtMs,
      lastThemeCheckAtMs: startedAtMs,
      lastUiAtMs: 0,
      diagnosticCount: 0,
      backgrounded: false,
      stateBeforeBackground: 'REARMING' as SessionGuardState,
      ending: false
    }
  },
  computed: {
    orchardBackgroundSrc(): string {
      return FRUIT_GAME_ASSETS.orchard[this.gameState.orchardTheme]
    },
    fruitSrc(): string {
      return FRUIT_PATHS[this.gameState.activeFruit]
    },
    targetLabel(): string {
      if (this.gameState.activeFruitForced) return '连击奖励 · 金苹果'
      return this.gameState.activeSide === 'left' ? '左手摘取' : '右手摘取'
    },
    basketSrc(): string {
      return FRUIT_GAME_ASSETS.basket[basketStage(this.gameState, this.displayFrame.target_count)]
    },
    bothWatermelonSrc(): string {
      return FRUIT_PATHS.bothWatermelon
    },
    harvestBurstSrc(): string {
      return FRUIT_GAME_ASSETS.effects.harvestBurst
    },
    rewardStarSrc(): string {
      return FRUIT_GAME_ASSETS.effects.rewardStar
    },
    comboSrc(): string {
      return FRUIT_GAME_ASSETS.effects.combo
    },
    effectFruitSrc(): string {
      return FRUIT_PATHS[this.effectFruit]
    },
    effectSpecial(): boolean {
      return this.effectFruit === 'goldenApple' || this.effectFruit === 'rainbowFruit' || this.effectFruit === 'bothWatermelon'
    },
    bonusSeconds(): number {
      return Math.max(0, Math.ceil(this.bonusRemainingMs / 1000))
    },
    specialFruitTotal(): number {
      return this.gameState.goldenAppleCount + this.gameState.rainbowFruitCount + this.gameState.bothWatermelonCount
    },
    paused(): boolean {
      return this.guardState === 'USER_PAUSED' || this.guardState === 'SOURCE_PAUSED' || this.guardState === 'RESTING'
    },
    feedbackText(): string {
      const side = this.gameState.activeSide
      const sideText = side === 'left' ? '左' : '右'
      if (this.guard.needsRearm(side)) return `${sideText}手请先回到低角度伸肘位`
      if (
        this.gameState.feedback.includes('当前') ||
        this.gameState.feedback.includes('成功') ||
        this.gameState.feedback.includes('连击')
      ) return this.gameState.feedback
      return `当前目标：${sideText}侧，请完成屈肘动作`
    },
    viewportCssVars(): Record<string, string> {
      return viewportStyle(this.viewportLayout)
    },
    multiplayerSelfId(): string {
      return getMultiplayerClient()?.bootstrap.identity.playerId ?? ''
    },
    overlayVisible(): boolean {
      return this.guardState === 'USER_PAUSED' || this.guardState === 'SOURCE_PAUSED' || this.guardState === 'RESTING' || this.guardState === 'DATA_INTERRUPTED'
    },
    overlayTitle(): string {
      if (this.guardState === 'DATA_INTERRUPTED') return '动作数据中断'
      if (this.guardState === 'RESTING') return `第 ${this.displayFrame.set_index}/${this.displayFrame.target_sets} 组完成`
      return '训练暂停'
    },
    overlayMessage(): string {
      if (this.guardState === 'DATA_INTERRUPTED') return '请检查设备连接，恢复后先将双臂回位'
      if (this.guardState === 'RESTING') return `休息 ${this.displayFrame.rest_remaining_sec} 秒，水果与计分已暂停`
      if (this.guardState === 'SOURCE_PAUSED') return '等待动作数据恢复运行'
      return '继续后请先回到低角度伸肘位'
    }
  },
  onLoad(query: Record<string, string | undefined>) {
    if (query?.returnUrl) configureCallerReturnUrl(query.returnUrl)
    this.refreshOrchardTheme()
    const multiplayerRequested = query?.multiplayer === '1'
    if (multiplayerRequested && !getMultiplayerClient()) restoreMultiplayerRuntime()
    if (multiplayerRequested && !isMultiplayerSession()) {
      this.ending = true
      uni.showToast({ title: '房间已退出', icon: 'none' })
      setTimeout(() => returnToCaller(), 0)
      return
    }
    if (!multiplayerRequested && getMultiplayerClient()) clearMultiplayerRuntime()
    const multiplayerClient = multiplayerRequested ? getMultiplayerClient() : null
    const initialRoom = multiplayerClient?.store.getSnapshot() ?? null
    if (multiplayerClient && initialRoom) {
      this.gameRandom = createSeededRandom(initialRoom.randomSeed)
      this.gameState = createInitialGameState(this.gameRandom, resolveOrchardTheme()) as GameState
      this.multiplayerSnapshot = initialRoom
      this.multiplayerUnsubscribe = multiplayerClient.store.subscribe((snapshot) => { this.multiplayerSnapshot = snapshot })
      multiplayerClient.onError((message) => this.handleMultiplayerError(message))
    }
    this.startSession()
  },
  onResize() {
    this.viewportLayout = getViewportLayout()
  },
  onShow() {
    this.refreshOrchardTheme()
    if (!this.backgrounded || this.ending) return
    this.lastRuntimeTickAtMs = Date.now()
    this.backgrounded = false
    if (this.stateBeforeBackground === 'USER_PAUSED') {
      this.guard.pauseByUser()
      this.syncGuardState()
      return
    }
    if (this.stateBeforeBackground === 'SOURCE_PAUSED' || this.stateBeforeBackground === 'RESTING' || this.stateBeforeBackground === 'DATA_INTERRUPTED') {
      this.guard.pauseForSource()
      this.sessionAwareSource()?.setPaused(false)
      this.syncGuardState()
      return
    }
    this.guard.resume()
    this.sessionAwareSource()?.setPaused(false)
    this.sessionAwareSource()?.resetRepCycleDetectors()
    this.syncGuardState()
  },
  onHide() {
    if (this.ending) return
    this.backgrounded = true
    this.stateBeforeBackground = this.guard.getState()
    this.guard.pauseForBackground()
    this.sessionAwareSource()?.setPaused(true)
    this.sessionAwareSource()?.resetRepCycleDetectors()
    this.releaseHeldControls()
    this.syncGuardState()
  },
  onUnload() {
    if (!this.ending) this.completeSession('STOPPED', false)
    else this.cleanup()
  },
  onBackPress() {
    if (this.basketOpen) {
      this.closeCollection()
      return true
    }
    if (!this.ending) this.requestStopTraining()
    return true
  },
  methods: {
    startSession(): void {
      this.source = createMotionDataSource({
        targetCount: this.config.targetCount,
        targetSets: this.config.targetSets,
        frameRateHz: this.config.frameRateHz,
        targetAngleDeg: this.config.targetAngleDeg,
        validAngleDeg: this.config.validAngleDeg,
        returnAngleDeg: this.config.returnAngleDeg,
        restDurationSec: this.config.restDurationSec
      })
      this.unsubscribe = this.source.subscribe((frame) => this.handleFrame(frame))
      this.keyboard = new H5KeyboardController(this.source, {
        debugEnabled: this.config.debugEnabled,
        onPause: () => this.togglePause(),
        onFinish: () => this.developerFinish(),
        onStop: () => this.developerStop(),
        onReset: () => this.resetTraining()
      })
      this.keyboard.attach()
      this.watchdogTimer = setInterval(() => this.runtimeTick(), 40)
      try {
        this.source.start()
      } catch (error) {
        this.cleanup()
        uni.showModal({
          title: '数据源不可用',
          content: error instanceof Error ? error.message : '无法启动动作数据源',
          showCancel: false,
          success: () => uni.navigateBack()
        })
      }
    },
    handleFrame(rawFrame: MotionFrame): void {
      const nowMs = Date.now()
      const parsed = this.adapter.ingest(rawFrame, nowMs)
      if (!parsed.accepted || this.ending) return
      this.diagnosticCount += parsed.diagnostics.length
      const frame = parsed.frame
      this.latestFrame = frame
      this.trainingAccumulator.accept(frame)
      this.guard.acceptFrame(frame, nowMs)
      this.syncGuardState()
      if (frame.training_state === 'FINISHED' || frame.training_state === 'STOPPED') {
        this.syncMultiplayerProgress(frame)
        this.applyDisplayFrame(frame)
        this.completeSession(frame.training_state, true)
        return
      }
      this.queueDisplayFrame(frame, nowMs)
      if (frame.training_state === 'IDLE' || frame.training_state === 'PAUSED' || frame.training_state === 'REST' || this.basketOpen) {
        this.syncMultiplayerProgress(frame)
        return
      }
      if (
        this.guardState === 'USER_PAUSED' ||
        this.guardState === 'BACKGROUND_PAUSED' ||
        this.guardState === 'SOURCE_PAUSED' ||
        this.guardState === 'RESTING' ||
        this.guardState === 'DATA_INTERRUPTED'
      ) {
        this.syncMultiplayerProgress(frame)
        return
      }

      const beforeBonus = this.gameState.bonusActive
      this.gameState = activateBonusForProgress(this.gameState, frame.overall_completion_percent)
      if (!beforeBonus && this.gameState.bonusActive) this.bonusRemainingMs = BONUS_DURATION_MS

      if (frame.rep_event !== 'none') {
        if (this.guard.canScore(frame.rep_event)) {
          const transition = reduceGameState(this.gameState, frame, this.gameRandom)
          this.gameState = transition.state
          if (transition.harvested && transition.harvestedFruit) {
            this.showHarvestEffect(transition.harvestedFruit, transition.pointsAwarded)
          }
        } else {
          const eventSide = frame.rep_event === 'left_rep_done' ? '左' : frame.rep_event === 'right_rep_done' ? '右' : '双侧'
          this.gameState = { ...this.gameState, feedback: `${eventSide}手臂尚未回位，本次不计游戏得分` }
        }
      }

      if (frame.rep_event !== 'none') this.lastUiAtMs = nowMs
      this.syncMultiplayerProgress(frame)
    },
    runtimeTick(): void {
      const nowMs = Date.now()
      const deltaMs = Math.min(250, Math.max(0, nowMs - this.lastRuntimeTickAtMs))
      this.lastRuntimeTickAtMs = nowMs
      this.applyInterpolatedDisplay(nowMs)
      this.checkDataTimeout()
      if (nowMs - this.lastThemeCheckAtMs >= 60000) {
        this.lastThemeCheckAtMs = nowMs
        this.refreshOrchardTheme()
      }
      const gameTimersActive = shouldAdvanceGameTimers(this.guardState, this.basketOpen)
      if (gameTimersActive) this.gameState = advanceComboTimer(this.gameState, deltaMs)
      if (
        !this.gameState.bonusActive ||
        !gameTimersActive
      ) return
      this.bonusRemainingMs = Math.max(0, this.bonusRemainingMs - deltaMs)
      if (this.bonusRemainingMs === 0) this.gameState = expireBonus(this.gameState)
    },
    refreshOrchardTheme(): void {
      this.gameState = withOrchardTheme(this.gameState, resolveOrchardTheme())
    },
    checkDataTimeout(): void {
      if (!this.guard.checkTimeout()) return
      this.sessionAwareSource()?.resetRepCycleDetectors()
      this.releaseHeldControls()
      this.syncGuardState()
    },
    applyDisplayFrame(frame: MotionFrame): void {
      this.displayFrame = { ...frame }
      const mapOptions = { minAngleDeg: 0, maxAngleDeg: Math.max(1, frame.target_angle_deg) }
      this.leftProgress = mapAngleToProgress(frame.left_angle_deg, mapOptions)
      this.rightProgress = mapAngleToProgress(frame.right_angle_deg, mapOptions)
    },
    queueDisplayFrame(frame: MotionFrame, nowMs: number): void {
      this.visualFromFrame = { ...this.displayFrame }
      this.visualTargetFrame = { ...frame }
      const sourceDelta = this.lastMotionTimestampMs > 0 ? frame.timestamp_ms - this.lastMotionTimestampMs : 40
      this.visualDurationMs = Math.min(250, Math.max(40, sourceDelta))
      this.visualStartedAtMs = nowMs
      this.lastMotionTimestampMs = frame.timestamp_ms
      this.displayFrame = {
        ...frame,
        left_angle_deg: this.visualFromFrame.left_angle_deg,
        right_angle_deg: this.visualFromFrame.right_angle_deg
      }
    },
    applyInterpolatedDisplay(nowMs: number): void {
      const ratio = Math.min(1, Math.max(0, (nowMs - this.visualStartedAtMs) / this.visualDurationMs))
      const frame = {
        ...this.visualTargetFrame,
        left_angle_deg: this.visualFromFrame.left_angle_deg + (this.visualTargetFrame.left_angle_deg - this.visualFromFrame.left_angle_deg) * ratio,
        right_angle_deg: this.visualFromFrame.right_angle_deg + (this.visualTargetFrame.right_angle_deg - this.visualFromFrame.right_angle_deg) * ratio
      }
      this.applyDisplayFrame(frame)
    },
    showHarvestEffect(fruit: CollectibleFruitName, points: number): void {
      if (this.effectTimer) clearTimeout(this.effectTimer)
      this.effectFruit = fruit
      this.effectPoints = points
      this.effectVisible = true
      this.effectTimer = setTimeout(() => {
        this.effectVisible = false
        this.effectTimer = null
      }, 820)
    },
    openCollection(): void {
      if (this.ending || this.basketOpen) return
      this.basketResumeOnClose = this.guardState === 'RUNNING' || this.guardState === 'REARMING'
      this.releaseHeldControls()
      this.debugExpanded = false
      if (this.basketResumeOnClose) {
        this.guard.pauseByUser()
        this.sessionAwareSource()?.setPaused(true)
        this.sessionAwareSource()?.resetRepCycleDetectors()
        this.syncGuardState()
      }
      this.basketOpen = true
    },
    closeCollection(): void {
      if (!this.basketOpen) return
      this.basketOpen = false
      if (!this.basketResumeOnClose) return
      this.basketResumeOnClose = false
      this.guard.resume()
      this.sessionAwareSource()?.setPaused(false)
      this.sessionAwareSource()?.resetRepCycleDetectors()
      this.syncGuardState()
    },
    syncGuardState(): void {
      this.guardState = this.guard.getState()
    },
    syncMultiplayerProgress(frame: MotionFrame): void {
      const client = getMultiplayerClient()
      if (!client || !isMultiplayerSession()) return
      const aggregate = this.trainingAccumulator.snapshot()
      const signature = [frame.training_state, frame.set_index, frame.left_count, frame.right_count, aggregate.leftTotalCount, aggregate.rightTotalCount, frame.overall_completion_percent, this.gameState.score, this.gameState.harvestedCount].join(':')
      if (signature === this.lastMultiplayerSignature) return
      this.lastMultiplayerSignature = signature
      client.sendProgress({
        motionSeq: frame.seq,
        repEvent: frame.rep_event,
        trainingState: frame.training_state,
        setIndex: frame.set_index,
        leftCount: frame.left_count,
        rightCount: frame.right_count,
        leftTotalCount: aggregate.leftTotalCount,
        rightTotalCount: aggregate.rightTotalCount,
        overallCompletionPercent: frame.overall_completion_percent,
        activeElapsedMs: this.guard.getActiveElapsedMs(),
        score: this.gameState.score,
        harvestedCount: this.gameState.harvestedCount
      })
    },
    handleMultiplayerError(message: string): void {
      if (!message.includes('重连超过15秒')) {
        uni.showToast({ title: message, icon: 'none' })
        return
      }
      uni.showModal({
        title: '多人连接已中断',
        content: '可保留当前进度继续单人训练，或结束本次训练。',
        confirmText: '继续单人',
        cancelText: '结束训练',
        success: (choice) => {
          if (choice.confirm) {
            this.multiplayerUnsubscribe?.()
            this.multiplayerUnsubscribe = null
            clearMultiplayerRuntime()
            this.multiplayerSnapshot = null
            uni.showToast({ title: '已切换为单人训练', icon: 'none' })
          } else {
            this.completeSession('STOPPED', true)
          }
        }
      })
    },
    sessionAwareSource() {
      return this.source && isSessionAwareSource(this.source) ? this.source : null
    },
    handleHeld(payload: { control: HeldControl; pressed: boolean }): void {
      if (this.basketOpen) return
      if (this.source && isAngleControllableSource(this.source)) this.source.setHeldControl(payload.control, payload.pressed)
    },
    releaseHeldControls(): void {
      if (!this.source || !isAngleControllableSource(this.source)) return
      HELD_CONTROLS.forEach((control) => this.source && isAngleControllableSource(this.source) && this.source.setHeldControl(control, false))
    },
    togglePause(): void {
      if (this.basketOpen) return
      if (this.guardState === 'USER_PAUSED') {
        this.guard.resume()
        this.sessionAwareSource()?.setPaused(false)
        this.sessionAwareSource()?.resetRepCycleDetectors()
        this.syncGuardState()
        return
      }
      if (this.guardState !== 'RUNNING' && this.guardState !== 'REARMING') {
        uni.showToast({ title: '当前状态不能手动继续', icon: 'none' })
        return
      }
      this.releaseHeldControls()
      this.guard.pauseByUser()
      this.sessionAwareSource()?.setPaused(true)
      this.sessionAwareSource()?.resetRepCycleDetectors()
      this.syncGuardState()
    },
    requestStopTraining(): void {
      if (this.ending) return
      uni.showModal({
        title: '结束训练',
        content: '确认提前结束并保存当前训练结果吗？',
        confirmText: '结束训练',
        success: (result) => {
          if (result.confirm) this.completeSession('STOPPED', true)
        }
      })
    },
    developerFinish(): void {
      if (this.config.debugEnabled) this.completeSession('FINISHED', true)
    },
    developerStop(): void {
      if (this.config.debugEnabled) this.completeSession('STOPPED', true)
    },
    resetTraining(): void {
      if (this.ending || !this.config.debugEnabled) return
      this.ending = true
      this.cleanup()
      uni.redirectTo({ url: appendReturnUrl('/pages-fruit-game/prepare/index') })
    },
    completeSession(reason: 'FINISHED' | 'STOPPED', navigateToResult: boolean): void {
      if (this.ending) return
      this.ending = true
      const completedAtMs = Date.now()
      this.guard.finish(completedAtMs)
      const frame: MotionFrame = { ...this.latestFrame, training_state: reason, rep_event: 'none' }
      const result = createTrainingResult(
        reason,
        frame,
        this.gameState,
        this.guard.getTotalElapsedMs(completedAtMs),
        this.guard.getActiveElapsedMs(completedAtMs),
        completedAtMs,
        this.trainingAccumulator.snapshot()
      )
      if (isMultiplayerSession()) {
        this.syncMultiplayerProgress(frame)
        setLocalMultiplayerResult(result)
      } else {
        publishResult(result)
      }
      this.cleanup()
      if (navigateToResult) {
        const resultUrl = isMultiplayerSession()
          ? '/pages-fruit-game/multiplayer/result/index'
          : '/pages-fruit-game/result/index'
        uni.redirectTo({ url: appendReturnUrl(resultUrl) })
      }
    },
    cleanup(): void {
      if (this.effectTimer) clearTimeout(this.effectTimer)
      if (this.watchdogTimer) clearInterval(this.watchdogTimer)
      this.effectTimer = null
      this.watchdogTimer = null
      this.releaseHeldControls()
      this.keyboard?.detach()
      this.keyboard = null
      this.unsubscribe?.()
      this.unsubscribe = null
      this.multiplayerUnsubscribe?.()
      this.multiplayerUnsubscribe = null
      this.source?.stop()
      this.source = null
    }
  }
}
</script>

<style scoped>
.rfg-game { position: relative; display: flex; align-items: center; justify-content: center; width: 100vw; height: 100vh; height: 100dvh; overflow: hidden; background: #183d18; font-family: "Microsoft YaHei", sans-serif; }
.rfg-game__stage { position: relative; width: 100vw; height: 56.25vw; max-width: 177.78vh; max-width: 177.78dvh; max-height: 100vh; max-height: 100dvh; overflow: hidden; background: #8ed65b; }
.rfg-game__multiplayer { position:absolute; top:calc(var(--rfg-safe-top, 8px) + 50px); z-index:19; transition:left 180ms ease,right 180ms ease; }
.rfg-game__multiplayer--target-left { right:12px; }
.rfg-game__multiplayer--target-right { left:12px; }
.rfg-game__background { position: absolute; inset: 0; width: 100%; height: 100%; transition: opacity 300ms ease; }
.rfg-game__target { position: absolute; top: max(20%, calc(var(--rfg-safe-top, 8px) + 48px)); z-index: 10; display: flex; flex-direction: column; align-items: center; width: clamp(104px, 14vw, 170px); transition: left 180ms ease, right 180ms ease; }
.rfg-game__target--left { left: 8%; }
.rfg-game__target--right { right: 8%; }
.rfg-game__target-visual { position: relative; width: 100%; aspect-ratio: 1; }
.rfg-game__target-glow, .rfg-game__fruit { position: absolute; inset: 0; box-sizing: border-box; width: 100%; height: 100%; transform-origin: 50% 50%; }
.rfg-game__target-glow { border: 4px solid rgba(255, 233, 57, 0.72); border-radius: 50%; background: radial-gradient(circle, rgba(255, 249, 147, 0.86), rgba(255, 255, 255, 0) 66%); box-shadow: 0 0 16px #fff45e; animation: rfg-target-pulse 1.2s ease-in-out infinite; }
.rfg-game__fruit { z-index: 1; object-position: center; filter: drop-shadow(0 6px 5px rgba(71, 42, 11, 0.25)); animation: rfg-fruit-float 1.4s ease-in-out infinite; }
.rfg-game__target-label { position: relative; z-index: 2; margin-top: -5px; padding: 4px 11px; border: 2px solid #c97815; border-radius: 999px; background: #fff3ae; color: #743c0b; font-size: clamp(11px, 1.8vh, 15px); font-weight: 900; line-height: 1.2; white-space: nowrap; box-shadow: 0 3px 0 rgba(103, 54, 8, 0.25); }
.rfg-game__bonus { position: absolute; left: 50%; top: max(17%, calc(var(--rfg-safe-top, 8px) + 45px)); z-index: 16; display: flex; flex-direction: column; align-items: center; width: 17%; transform: translateX(-50%); animation: rfg-bonus-enter 350ms ease-out; }
.rfg-game__bonus-fruit { width: 100%; aspect-ratio: 1; filter: drop-shadow(0 0 12px #fff279); animation: rfg-fruit-float 1s ease-in-out infinite; }
.rfg-game__bonus-time, .rfg-game__bonus-label { padding: 2px 9px; border-radius: 999px; background: rgba(255, 245, 181, 0.95); color: #8a4009; font-size: clamp(10px, 1.7vh, 13px); font-weight: 900; white-space: nowrap; }
.rfg-game__bonus-label { margin-top: -8%; border: 2px solid #e09412; }
.rfg-game__basket-button { position: absolute; right: 2.5%; bottom: 2%; z-index: 18; display: flex; flex-direction: column; align-items: center; width: 16%; height: 23%; margin: 0; padding: 0; border: 0; background: transparent; line-height: 1; }
.rfg-game__basket { height: 82%; filter: drop-shadow(0 6px 5px rgba(48, 34, 13, 0.3)); }
.rfg-game__basket-burst { position: absolute; left: 22%; top: -20%; width: 78%; height: 78%; opacity: 0; animation: rfg-basket-burst 360ms 430ms ease-out both; pointer-events: none; }
.rfg-game__basket-label { margin-top: -4px; padding: 3px 10px; border-radius: 999px; background: rgba(255, 246, 204, 0.95); color: #74420d; font-size: clamp(10px, 1.7vh, 13px); font-weight: 900; box-shadow: 0 3px 0 rgba(93, 53, 8, 0.28); }
.rfg-game__basket-button::after { border: 0; }
.rfg-game__feedback { position: absolute; left: 50%; bottom: 2.5%; z-index: 18; box-sizing: border-box; max-width: 42%; padding: 5px 18px; overflow: hidden; border-radius: 999px; background: rgba(255, 249, 214, 0.94); color: #70400c; font-size: clamp(11px, 1.9vh, 16px); font-weight: 800; line-height: 1.25; white-space: nowrap; text-overflow: ellipsis; transform: translateX(-50%); }
.rfg-game__effect { position: absolute; left: 50%; top: 27%; z-index: 30; width: 25%; aspect-ratio: 1; transform: translateX(-50%); animation: rfg-reward-pop 820ms ease-out both; pointer-events: none; }
.rfg-game__effect-burst { position: absolute; inset: 0; width: 100%; height: 100%; }
.rfg-game__effect-fruit { position: absolute; left: 30%; top: 28%; width: 40%; height: 40%; animation: rfg-fruit-to-basket 820ms ease-in both; }
.rfg-game__effect-star { position: absolute; right: -5%; top: 0; width: 34%; height: 34%; animation: rfg-star-spin 700ms ease-out; }
.rfg-game__effect-combo { position: absolute; left: -28%; top: 6%; width: 54%; height: 42%; }
.rfg-game__effect-score { position: absolute; left: 50%; bottom: 5%; color: #fff; font-size: clamp(24px, 5.5vh, 42px); font-weight: 900; text-shadow: 0 3px 0 #e66c0f, 0 0 8px #ffef62; transform: translateX(-50%); }
.rfg-game__pause-overlay { position: absolute; inset: 0; z-index: 35; display: flex; flex-direction: column; align-items: center; justify-content: center; background: rgba(23, 58, 20, 0.76); }
.rfg-game__pause-title { color: #fff; font-size: clamp(34px, 8vh, 62px); font-weight: 900; text-shadow: 0 5px 0 #35591c; }
.rfg-game__pause-message { margin-top: 10px; color: #fff8c8; font-size: clamp(14px, 2.5vh, 20px); font-weight: 700; }
.rfg-game__resume { width: 190px; margin-top: 18px; border-radius: 999px; background: linear-gradient(#9bea45, #50ac18); color: #fff; font-size: 21px; font-weight: 900; box-shadow: 0 6px 0 #357911; }
.rfg-game__resume::after { border: 0; }
.rfg-game__diagnostics { position: absolute; left: 1.5%; bottom: calc(1.5% + 38px); z-index: 40; padding: 3px 8px; border-radius: 6px; background: rgba(0, 0, 0, 0.5); color: #fff; font-size: clamp(10px, 1.6vh, 13px); }
.rfg-game__portrait-warning { display: none; }

@keyframes rfg-target-pulse { 50% { transform: scale(1.1); opacity: 0.74; } }
@keyframes rfg-fruit-float { 50% { transform: translateY(-8%); } }
@keyframes rfg-bonus-enter { from { opacity: 0; transform: translate(-50%, -20%) scale(0.7); } to { opacity: 1; transform: translateX(-50%) scale(1); } }
@keyframes rfg-reward-pop { 0% { opacity: 0; transform: translate(-50%, 28%) scale(0.4); } 28% { opacity: 1; transform: translate(-50%, 0) scale(1.08); } 100% { opacity: 0; transform: translate(-50%, -18%) scale(0.96); } }
@keyframes rfg-fruit-to-basket { 0%, 42% { opacity: 1; transform: translate(0, 0) scale(1); } 100% { opacity: 0; transform: translate(230%, 155%) scale(0.28); } }
@keyframes rfg-star-spin { to { transform: rotate(210deg) scale(0.75); opacity: 0; } }
@keyframes rfg-basket-burst { 0% { opacity: 0; transform: scale(0.35); } 45% { opacity: 1; transform: scale(1.15); } 100% { opacity: 0; transform: scale(0.8); } }

@media (orientation: portrait) {
  .rfg-game__stage { display: none; }
  .rfg-game__portrait-warning { display: flex; flex-direction: column; align-items: center; padding: 32px; color: #fff; text-align: center; }
  .rfg-game__portrait-icon { font-size: 70px; font-weight: 900; }
  .rfg-game__portrait-text { margin-top: 14px; font-size: 22px; font-weight: 800; }
}
</style>
