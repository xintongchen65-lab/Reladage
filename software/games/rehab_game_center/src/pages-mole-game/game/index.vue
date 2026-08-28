<template>
  <view class="mg-game" :style="viewportCss">
    <image class="mg-game__background" :src="assets.background" mode="aspectFill" />
    <view class="mg-game__shade" />
    <image v-if="!multiplayerSnapshot" class="mg-game__title-sign" :src="assets.sign" mode="aspectFit" />

    <view class="mg-game__hud">
      <view class="mg-game__progress">
        <view class="mg-game__progress-title">
          <text>总进度 {{ Math.round(frame.overall_completion_percent) }}%</text>
          <text>第 {{ frame.set_index }}/{{ frame.target_sets }} 组 · {{ currentCount }}/{{ frame.target_count }}</text>
        </view>
        <view class="mg-game__track"><view :style="{ width: `${clampPercent(frame.overall_completion_percent)}%` }" /></view>
      </view>
      <button class="mg-game__hud-btn mg-game__hud-btn--pause" @click="togglePause">{{ guardState === 'USER_PAUSED' ? '继续' : '暂停' }}</button>
      <button class="mg-game__hud-btn mg-game__hud-btn--exit" @click="stopConfirm">退出</button>
    </view>

    <view v-if="multiplayerSnapshot" class="mg-game__room-strip">
      <text>{{ multiplayerSnapshot.mode === 'PK' ? 'PK' : '合作' }} · 房间 {{ multiplayerSnapshot.roomCode }}</text>
      <text v-if="multiplayerSnapshot.mode === 'COOP'">团队 {{ multiplayerSnapshot.teamContribution }}/{{ multiplayerSnapshot.teamTarget }}</text>
      <text v-else>我的排名 #{{ selfRank }}</text>
    </view>

    <view class="mg-game__arena">
      <view
        v-for="lane in laneModels"
        :key="lane.index"
        class="mg-game__lane"
        :class="{
          'mg-game__lane--self': lane.isSelf,
          'mg-game__lane--target': game.hammerLane === lane.index && game.phase === 'WARNING'
        }"
        :style="lane.style"
      >
        <image v-if="game.hammerLane === lane.index && game.phase === 'WARNING'" class="mg-game__warning-ring" :src="assets.warningRing" mode="aspectFit" />
        <view class="mg-game__mole-window">
          <image
            class="mg-game__mole"
            :class="{ 'mg-game__mole--hit': game.hammerLane === lane.index && game.phase === 'HIT' }"
            :src="lane.moleSrc"
            mode="aspectFit"
            :style="moleStyleFor(lane.index)"
          />
        </view>
        <view class="mg-game__lane-label" :class="{ 'mg-game__lane-label--self': lane.isSelf }">
          <text>{{ lane.label }}</text>
          <text v-if="lane.score !== null">{{ lane.score }}分</text>
        </view>
        <image
          v-if="game.hammerLane === lane.index"
          class="mg-game__hammer"
          :class="[`mg-game__hammer--${game.phase.toLowerCase()}`, { 'mg-game__hammer--local': localHammerTurn }]"
          :src="assets.hammer"
          mode="aspectFit"
        />
        <image v-if="game.hammerLane === lane.index && game.phase === 'HIT'" class="mg-game__hit-burst" :src="assets.hitBurst" mode="aspectFit" />
      </view>
    </view>

    <view class="mg-game__status-card">
      <view class="mg-game__status-main">
        <text class="mg-game__status-kicker">当前动作</text>
        <text class="mg-game__status-stage">{{ stageLabel }}</text>
      </view>
      <view class="mg-game__motion-track"><view :style="{ height: `${Math.round(clamp01(frame.motion_progress) * 100)}%` }" /></view>
      <view v-if="config.debugEnabled" class="mg-game__status-meta">
        <text>对称 {{ Math.round(frame.symmetry_percent) }}%</text>
        <text v-if="frame.warning !== 'none'" class="mg-game__warning-text">{{ warningLabel }}</text>
      </view>
    </view>

    <view class="mg-game__prompt" :class="{ 'mg-game__prompt--danger': localHammerTurn }">
      <text class="mg-game__prompt-arrow">{{ localHammerTurn ? '↓' : '●' }}</text>
      <text>{{ actionPrompt }}</text>
    </view>

    <view class="mg-game__score-card">
      <view><text>闪避</text><strong>{{ game.dodges }}</strong></view>
      <view><text>被敲</text><strong>{{ game.hits }}</strong></view>
      <view><text>连击</text><strong>x{{ game.combo }}</strong></view>
      <view class="mg-game__score-main"><text>得分</text><strong>{{ game.score }}</strong></view>
    </view>

    <view v-if="frame.training_state === 'REST' || overlayVisible" class="mg-game__overlay">
      <view class="mg-game__overlay-card">
        <text class="mg-game__overlay-title">{{ overlayTitle }}</text>
        <text class="mg-game__overlay-text">{{ overlayText }}</text>
        <view class="mg-game__overlay-actions">
          <button v-if="guardState === 'USER_PAUSED'" @click="togglePause">继续训练</button>
          <button class="mg-game__overlay-exit" @click="stopConfirm">退出训练</button>
        </view>
      </view>
    </view>

    <view v-if="config.debugEnabled" class="mg-game__debug">
      <button @touchstart="dir('down')" @touchend="dir('none')" @mousedown="dir('down')" @mouseup="dir('none')">下蹲</button>
      <button @touchstart="dir('up')" @touchend="dir('none')" @mousedown="dir('up')" @mouseup="dir('none')">起身</button>
      <button @click="cycle">完整一次</button>
      <button @click="finish('FINISHED')">完成</button>
      <text v-if="diagnostics.length">{{ diagnostics.join(' · ') }}</text>
    </view>
  </view>
</template>

<script lang="ts">
import { MotionSessionGuard, type SessionGuardState } from '../../game-platform/motion/session-guard'
import { getViewportLayout, viewportStyle } from '../../game-platform/runtime/viewport'
import { appendReturnUrl, configureCallerReturnUrl, returnToCaller } from '../../game-platform/runtime/navigation'
import { createInitialSquatFrame, type SquatMotionFrame } from '../types/motion'
import type { MoleTrainingResult } from '../types/result'
import {
  advanceMole,
  createInitialMoleState,
  isLocalHammerTurn,
  markLocalHidden,
  MoleRepReconciler,
  MoleTrainingAccumulator,
  npcHideProgress,
  registerDodge,
  type MoleGameState
} from '../core/game-engine'
import { createMoleMotionDataSource } from '../data-sources/source-factory'
import { isMoleControllableSource, type MoleMotionDataSource } from '../data-sources/contracts'
import { getMoleConfig, publishMoleResult } from '../runtime/session-runtime'
import {
  getMoleMultiplayerClient,
  isMoleMultiplayer,
  leaveMoleRoom,
  restoreMoleMultiplayer,
  setMoleLocalResult
} from '../runtime/multiplayer-runtime'
import { MOLE_GAME_ASSETS } from '../runtime/asset-paths'
import type { RoomSnapshot } from '../../game-platform/multiplayer/types'

interface LaneModel {
  index: number
  label: string
  moleSrc: string
  isSelf: boolean
  score: number | null
  style: Record<string, string>
}

const BACKGROUND_SIZE = { width: 822, height: 463 }
const LANE_POSITIONS = [
  { x: 190, y: 315, scale: 0.92 },
  { x: 307, y: 385, scale: 1.04 },
  { x: 432, y: 315, scale: 0.98 },
  { x: 559, y: 385, scale: 1.04 },
  { x: 682, y: 315, scale: 0.92 }
]

function readStageSize(): { width: number; height: number } {
  try {
    const info = uni.getSystemInfoSync()
    return {
      width: Math.max(1, Number(info.windowWidth) || 844),
      height: Math.max(1, Number(info.windowHeight) || 390)
    }
  } catch {
    return { width: 844, height: 390 }
  }
}

export default {
  name: 'MoleGame',
  data() {
    const config = getMoleConfig()
    return {
      assets: MOLE_GAME_ASSETS,
      config,
      viewport: getViewportLayout(),
      stageSize: readStageSize(),
      frame: createInitialSquatFrame(config.targetCount, config.targetSets),
      game: createInitialMoleState(2, config.warningWindowMs) as MoleGameState,
      source: null as MoleMotionDataSource | null,
      unsub: null as (() => void) | null,
      timer: null as ReturnType<typeof setInterval> | null,
      multiUnsub: null as (() => void) | null,
      multiplayer: false,
      multiplayerSnapshot: null as RoomSnapshot | null,
      localLaneIndex: 2,
      guard: new MotionSessionGuard<SquatMotionFrame>({ dataTimeoutMs: config.dataTimeoutMs, returnAngleDeg: config.returnAngleDeg }),
      guardState: 'REARMING' as SessionGuardState,
      lastTick: Date.now(),
      ending: false,
      accumulator: new MoleTrainingAccumulator(),
      reconciler: new MoleRepReconciler(),
      diagnostics: [] as string[]
    }
  },
  computed: {
    viewportCss(): Record<string, string> {
      return viewportStyle(this.viewport, '--mg-safe-top')
    },
    currentCount(): number {
      return Math.max(this.frame.left_count, this.frame.right_count)
    },
    localHammerTurn(): boolean {
      return isLocalHammerTurn(this.game, this.localLaneIndex)
    },
    selfRank(): number {
      if (!this.multiplayerSnapshot) return 1
      const id = getMoleMultiplayerClient()?.bootstrap.identity.playerId
      return this.multiplayerSnapshot.players.find((p) => p.playerId === id)?.rank ?? 1
    },
    laneModels(): LaneModel[] {
      const players = [...(this.multiplayerSnapshot?.players ?? [])].sort((a, b) => a.joinedAtMs - b.joinedAtMs)
      return LANE_POSITIONS.map((position, index) => {
        const player = players[index]
        const mapped = this.mapBackgroundPoint(position.x, position.y)
        return {
          index,
          label: player ? `${player.displayName}${index === this.localLaneIndex ? ' · 我' : ''}` : index === this.localLaneIndex ? '我的地鼠' : `地鼠 ${index + 1}`,
          moleSrc: this.assets.moles[index],
          isSelf: index === this.localLaneIndex,
          score: player ? player.score : null,
          style: {
            left: `${mapped.left}%`,
            top: `${mapped.top}%`,
            transform: `translate(-50%, -80%) scale(${position.scale})`
          }
        }
      })
    },
    stageLabel(): string {
      return {
        STANDING: '站立',
        DESCENDING: '下蹲中',
        BOTTOM: '已蹲下',
        RISING: '起身中'
      }[this.frame.motion_stage]
    },
    warningLabel(): string {
      if (this.frame.warning === 'left_right_asymmetry') return '左右发力不对称'
      if (this.frame.warning === 'resting') return '休息中'
      return '请保持动作稳定'
    },
    actionPrompt(): string {
      if (this.frame.training_state === 'REST') return `本组完成，休息 ${this.frame.rest_remaining_sec}s`
      if (this.guardState === 'DATA_INTERRUPTED') return '动作数据中断，请检查设备连接'
      if (this.guardState === 'USER_PAUSED') return '训练已暂停'
      if (this.guardState === 'REARMING') return '请先站稳，准备下一次'
      if (this.localHammerTurn) return '锤子来了！请下蹲躲避！'
      if (this.game.phase === 'WARNING') return `锤子正在 ${this.game.hammerLane + 1} 号洞口上方，保持站稳`
      return this.game.feedback
    },
    overlayVisible(): boolean {
      return ['USER_PAUSED', 'BACKGROUND_PAUSED', 'SOURCE_PAUSED', 'DATA_INTERRUPTED'].includes(this.guardState)
    },
    overlayTitle(): string {
      if (this.frame.training_state === 'REST') return '本组完成 · 休息中'
      if (this.guardState === 'DATA_INTERRUPTED') return '动作数据中断'
      if (this.guardState === 'BACKGROUND_PAUSED') return '训练已暂停'
      return '训练暂停'
    },
    overlayText(): string {
      if (this.frame.training_state === 'REST') return `休息 ${this.frame.rest_remaining_sec}s，下一组自动开始`
      if (this.guardState === 'DATA_INTERRUPTED') return '等待主控恢复数据，恢复后请先重新站稳'
      return '继续后请先站稳，再根据锤子提示完成箱式深蹲'
    }
  },
  onLoad(query: Record<string, string | undefined>) {
    configureCallerReturnUrl(query.returnUrl, '/pages-mole-game/home/index')
    this.multiplayer = query.multiplayer === '1'
    if (this.multiplayer && !getMoleMultiplayerClient()) restoreMoleMultiplayer()
    if (this.multiplayer && !isMoleMultiplayer()) {
      this.ending = true
      uni.showToast({ title: '房间已退出', icon: 'none' })
      setTimeout(() => returnToCaller(), 0)
      return
    }
    if (this.multiplayer) {
      const client = getMoleMultiplayerClient()
      this.multiplayerSnapshot = client?.store.getSnapshot() ?? null
      this.refreshLocalLane()
      this.multiUnsub = client?.store.subscribe((snapshot) => {
        this.multiplayerSnapshot = snapshot
        this.refreshLocalLane()
      }) ?? null
    }
    this.game = createInitialMoleState(this.localLaneIndex, this.config.warningWindowMs)
    this.accumulator.reset()
    this.reconciler.reset(this.frame)
    this.start()
  },
  onResize() {
    this.viewport = getViewportLayout()
    this.stageSize = readStageSize()
  },
  onHide() {
    if (this.ending || ['USER_PAUSED', 'RESTING', 'SOURCE_PAUSED', 'DATA_INTERRUPTED'].includes(this.guardState)) return
    this.guard.pauseForBackground()
    if (this.source && isMoleControllableSource(this.source)) this.source.setPaused(true)
    this.guardState = this.guard.getState()
  },
  onShow() {
    if (this.guardState === 'BACKGROUND_PAUSED') {
      if (this.source && isMoleControllableSource(this.source)) this.source.setPaused(false)
      this.guard.resume()
      this.guardState = this.guard.getState()
    }
  },
  onBackPress() {
    this.stopConfirm()
    return true
  },
  onUnload() {
    this.cleanup()
    if (!this.ending) this.finish('STOPPED', false)
  },
  methods: {
    mapBackgroundPoint(x: number, y: number): { left: number; top: number } {
      const scale = Math.max(
        this.stageSize.width / BACKGROUND_SIZE.width,
        this.stageSize.height / BACKGROUND_SIZE.height
      )
      const cropX = (BACKGROUND_SIZE.width * scale - this.stageSize.width) / 2
      const cropY = (BACKGROUND_SIZE.height * scale - this.stageSize.height) / 2
      return {
        left: ((x * scale - cropX) / this.stageSize.width) * 100,
        top: ((y * scale - cropY) / this.stageSize.height) * 100
      }
    },
    clamp01(value: number): number {
      return Math.max(0, Math.min(1, Number.isFinite(value) ? value : 0))
    },
    clampPercent(value: number): number {
      return Math.max(0, Math.min(100, Number.isFinite(value) ? value : 0))
    },
    refreshLocalLane() {
      if (!this.multiplayerSnapshot) {
        this.localLaneIndex = 2
        return
      }
      const selfId = getMoleMultiplayerClient()?.bootstrap.identity.playerId
      const seatedPlayers = [...this.multiplayerSnapshot.players].sort((a, b) => a.joinedAtMs - b.joinedAtMs)
      const index = seatedPlayers.findIndex((player) => player.playerId === selfId)
      if (index >= 0) this.localLaneIndex = Math.min(4, index)
    },
    moleStyleFor(index: number): Record<string, string> {
      let progress = 0
      if (index === this.localLaneIndex) progress = this.clamp01(this.frame.motion_progress)
      else progress = npcHideProgress(this.game, index, this.localLaneIndex)
      return { transform: `translate(-50%, ${Math.round(progress * 94)}%)` }
    },
    start() {
      this.source = createMoleMotionDataSource(this.config)
      this.unsub = this.source.subscribe((frame) => this.onFrame(frame))
      this.source.start()
      this.lastTick = Date.now()
      this.timer = setInterval(() => this.tick(), 40)
      // #ifdef H5
      window.addEventListener('keydown', this.keyDown)
      window.addEventListener('keyup', this.keyUp)
      // #endif
    },
    keyDown(event: KeyboardEvent) {
      if (event.repeat) return
      if (event.key === 'w' || event.key === 'ArrowDown') this.dir('down')
      if (event.key === 's' || event.key === 'ArrowUp') this.dir('up')
      if (event.key === ' ') this.cycle()
      if (event.key === 'p') this.togglePause()
      if (event.key === 'Escape') this.stopConfirm()
    },
    keyUp(event: KeyboardEvent) {
      if (['w', 's', 'ArrowDown', 'ArrowUp'].includes(event.key)) this.dir('none')
    },
    onFrame(frame: SquatMotionFrame) {
      if (this.ending) return
      this.frame = frame
      this.accumulator.accept(frame)
      this.guard.acceptFrame(frame)
      this.guardState = this.guard.getState()
      // motion_progress drives the real-time hide animation. Reaching the squat target makes the hammer visually miss,
      // while the later both_rep_done still owns the medical/game confirmation and points.
      if (frame.motion_progress >= 0.88 && isLocalHammerTurn(this.game, this.localLaneIndex)) {
        this.game = markLocalHidden(this.game, this.localLaneIndex)
      }
      const decision = this.reconciler.accept(frame)
      if (decision.countJump && !this.diagnostics.includes('次数跳增，仅同步康复数据')) {
        this.diagnostics = [...this.diagnostics, '次数跳增，仅同步康复数据']
      }
      if (decision.accepted && this.guard.canScore('both_rep_done')) {
        this.game = registerDodge(this.game, this.localLaneIndex)
      }
      if (frame.training_state === 'FINISHED' || frame.training_state === 'STOPPED') {
        this.finish(frame.training_state)
        return
      }
      this.sendProgress()
    },
    tick() {
      const now = Date.now()
      const delta = Math.min(200, Math.max(0, now - this.lastTick))
      this.lastTick = now
      if (this.guard.checkTimeout(now)) this.guardState = this.guard.getState()
      if (['RUNNING', 'REARMING'].includes(this.guardState) && this.frame.training_state === 'RUNNING') {
        this.game = advanceMole(this.game, delta, this.config.warningWindowMs, this.localLaneIndex)
      }
    },
    dir(direction: 'down' | 'up' | 'none') {
      if (this.source && isMoleControllableSource(this.source)) this.source.setSquatDirection(direction)
    },
    cycle() {
      if (this.source && isMoleControllableSource(this.source)) this.source.simulateCompleteCycle()
    },
    togglePause() {
      if (this.guardState === 'USER_PAUSED') {
        if (this.source && isMoleControllableSource(this.source)) this.source.setPaused(false)
        this.guard.resume()
      } else {
        this.guard.pauseByUser()
        if (this.source && isMoleControllableSource(this.source)) this.source.setPaused(true)
      }
      this.guardState = this.guard.getState()
    },
    stopConfirm() {
      if (this.ending) return
      uni.showModal({
        title: '退出训练',
        content: '是否提前退出并保留本次训练数据？',
        confirmText: '退出',
        cancelText: '继续训练',
        success: (result) => {
          if (result.confirm) this.finish('STOPPED')
        }
      })
    },
    finish(reason: 'FINISHED' | 'STOPPED', navigate = true) {
      if (this.ending) return
      this.ending = true
      this.guard.finish()
      const aggregate = this.accumulator.snapshot()
      const result: MoleTrainingResult = {
        gameId: 'mole',
        endReason: reason,
        elapsedMs: this.guard.getTotalElapsedMs(),
        activeElapsedMs: this.guard.getActiveElapsedMs(),
        completedAtMs: Date.now(),
        training: {
          left_count: this.frame.left_count,
          right_count: this.frame.right_count,
          left_total_count: aggregate.leftTotalCount,
          right_total_count: aggregate.rightTotalCount,
          left_rom_deg: this.frame.left_rom_deg,
          right_rom_deg: this.frame.right_rom_deg,
          max_rom_deg: Math.max(aggregate.leftMaxRomDeg, aggregate.rightMaxRomDeg),
          target_count: this.frame.target_count,
          completion_percent: this.frame.completion_percent,
          set_index: aggregate.finalSetIndex,
          target_sets: this.frame.target_sets,
          overall_completion_percent: aggregate.overallCompletionPercent,
          symmetry_percent: this.frame.symmetry_percent,
          quality: this.frame.quality,
          warning: this.frame.warning
        },
        game: {
          rounds: this.game.rounds,
          dodges: this.game.dodges,
          hits: this.game.hits,
          score: this.game.score,
          combo: this.game.combo,
          bestCombo: this.game.bestCombo,
          coins: this.game.coins
        }
      }
      publishMoleResult(result)
      if (this.multiplayer) setMoleLocalResult(result)
      this.cleanup()
      if (navigate) {
        uni.redirectTo({ url: appendReturnUrl(this.multiplayer ? '/pages-mole-game/multiplayer/result/index' : '/pages-mole-game/result/index') })
      } else if (this.multiplayer) {
        leaveMoleRoom()
      }
    },
    cleanup() {
      if (this.timer) clearInterval(this.timer)
      this.timer = null
      this.unsub?.()
      this.unsub = null
      this.multiUnsub?.()
      this.multiUnsub = null
      this.source?.stop()
      // #ifdef H5
      window.removeEventListener('keydown', this.keyDown)
      window.removeEventListener('keyup', this.keyUp)
      // #endif
    },
    sendProgress() {
      if (!this.multiplayer) return
      const client = getMoleMultiplayerClient()
      if (!client) return
      const aggregate = this.accumulator.snapshot()
      client.sendProgress({
        motionSeq: this.frame.seq,
        repEvent: this.frame.rep_event,
        trainingState: this.frame.training_state,
        setIndex: this.frame.set_index,
        leftCount: this.frame.left_count,
        rightCount: this.frame.right_count,
        leftTotalCount: aggregate.leftTotalCount,
        rightTotalCount: aggregate.rightTotalCount,
        overallCompletionPercent: aggregate.overallCompletionPercent,
        activeElapsedMs: this.guard.getActiveElapsedMs(),
        score: this.game.score,
        harvestedCount: this.game.dodges,
        attempts: this.game.rounds,
        successes: this.game.dodges
      })
    }
  }
}
</script>

<style scoped>
.mg-game{position:relative;width:100vw;height:100vh;overflow:hidden;background:#79ccef;font-family:"Microsoft YaHei",sans-serif;color:#fff}.mg-game__background{position:absolute;z-index:0;inset:0;width:100%;height:100%}.mg-game__shade{position:absolute;z-index:1;inset:0;background:linear-gradient(180deg,rgba(0,70,83,.02),transparent 34%,rgba(35,70,16,.035) 74%,rgba(26,61,16,.16))}.mg-game__title-sign{position:absolute;z-index:3;left:2.2%;top:calc(var(--mg-safe-top,8px) + clamp(58px,10.5vh,78px));display:flex;align-items:center;justify-content:center;box-sizing:border-box;width:clamp(102px,12vw,176px);height:clamp(48px,9vh,72px);padding:6px 10px;border:4px solid #70401e;border-radius:11px 14px;background:linear-gradient(180deg,#e4a44e,#b8682f);box-shadow:inset 0 0 0 3px rgba(255,213,124,.35),0 6px 8px rgba(74,48,19,.22);transform:rotate(-4deg);pointer-events:none}.mg-game__title-sign text{color:#fff5c7;font-size:clamp(14px,2.8vh,22px);font-weight:900;text-shadow:0 2px 0 #77401f;white-space:nowrap}.mg-game__hud{position:absolute;z-index:30;top:var(--mg-safe-top,8px);left:1.5%;right:1.5%;display:flex;align-items:center;gap:10px;box-sizing:border-box;height:clamp(48px,9.2vh,66px);padding:7px 10px 7px 16px;border:1px solid rgba(178,255,231,.35);border-radius:18px;background:linear-gradient(90deg,rgba(9,93,76,.92),rgba(11,109,99,.88));box-shadow:0 5px 18px rgba(20,73,53,.18)}.mg-game__progress{flex:1;min-width:0}.mg-game__progress-title{display:flex;justify-content:space-between;gap:8px;font-size:clamp(11px,2.6vh,16px);font-weight:900}.mg-game__track{height:8px;margin-top:5px;overflow:hidden;border-radius:999px;background:rgba(255,255,255,.34)}.mg-game__track>view{height:100%;border-radius:inherit;background:linear-gradient(90deg,#7bef55,#ffcf3d)}.mg-game__hud-btn{display:flex;align-items:center;justify-content:center;min-width:64px;height:38px;margin:0;padding:0 13px;border:0;border-radius:999px;font-weight:900;line-height:1}.mg-game__hud-btn--pause{background:#ffd34d;color:#624b10}.mg-game__hud-btn--exit{background:rgba(255,255,255,.94);color:#246050}.mg-game__room-strip{position:absolute;z-index:25;top:calc(var(--mg-safe-top,8px) + clamp(54px,10.5vh,74px));left:2%;display:flex;gap:12px;padding:5px 10px;border-radius:999px;background:rgba(255,255,255,.9);color:#245c4d;font-size:11px;font-weight:800}.mg-game__arena{position:absolute;z-index:5;inset:10% 0 9%;pointer-events:none}.mg-game__lane{position:absolute;width:clamp(108px,15.5vw,190px);height:clamp(96px,24vh,174px);transition:left .34s ease,top .34s ease,transform .34s ease}.mg-game__mole-window{position:absolute;z-index:4;left:50%;bottom:18%;width:74%;height:74%;overflow:hidden;transform:translateX(-50%);border-radius:48% 48% 30% 30%}.mg-game__mole{position:absolute;left:50%;bottom:0;width:100%;height:100%;transform:translate(-50%,0);transform-origin:50% 100%;transition:transform 100ms linear,filter 150ms ease}.mg-game__mole--hit{filter:saturate(.7) brightness(.94)}.mg-game__hole{position:absolute;z-index:6;left:50%;bottom:2%;width:98%;height:43%;transform:translateX(-50%);filter:drop-shadow(0 6px 5px rgba(88,45,18,.2))}.mg-game__lane--self .mg-game__hole{filter:drop-shadow(0 0 8px rgba(85,221,255,.8)) drop-shadow(0 7px 5px rgba(88,45,18,.22))}.mg-game__warning-ring{position:absolute;z-index:2;left:50%;bottom:-2%;width:104%;height:47%;transform:translateX(-50%);filter:drop-shadow(0 0 10px rgba(255,212,45,.68));animation:mgPulse .68s ease-in-out infinite alternate}.mg-game__lane-label{position:absolute;z-index:8;left:50%;bottom:-17px;display:flex;gap:5px;align-items:center;justify-content:center;max-width:95%;padding:3px 8px;border-radius:999px;background:rgba(32,72,42,.72);transform:translateX(-50%);white-space:nowrap;font-size:10px;font-weight:800}.mg-game__lane-label--self{background:#1d7c68}.mg-game__hammer{position:absolute;z-index:12;left:50%;top:-12%;width:clamp(68px,8.8vw,128px);height:clamp(72px,18vh,142px);transform:translate(-50%,-50%) rotate(-18deg);transform-origin:50% 90%;transition:left .38s cubic-bezier(.2,.8,.2,1),top .38s cubic-bezier(.2,.8,.2,1),transform .18s ease,opacity .18s ease;filter:drop-shadow(0 8px 7px rgba(91,50,18,.28))}.mg-game__hammer--warning{animation:mgHammerHover .68s ease-in-out infinite alternate}.mg-game__hammer--hit{transform:translate(-50%,-20%) rotate(9deg) scale(1.08)}.mg-game__hammer--dodged{transform:translate(-50%,-66%) rotate(-25deg);opacity:.72}.mg-game__hammer--next{transform:translate(-50%,-60%) rotate(-24deg);opacity:.72}.mg-game__hammer--local{filter:drop-shadow(0 0 12px rgba(255,223,54,.92)) drop-shadow(0 8px 7px rgba(91,50,18,.28))}.mg-game__hit-burst{position:absolute;z-index:13;left:50%;top:36%;width:clamp(90px,13vw,170px);height:clamp(75px,19vh,140px);transform:translate(-50%,-50%);pointer-events:none}.mg-game__status-card{position:absolute;z-index:22;left:2%;bottom:2.4%;display:grid;grid-template-columns:auto 12px auto;align-items:center;gap:9px;width:clamp(178px,22vw,270px);box-sizing:border-box;padding:9px 12px;border:1px solid rgba(177,255,225,.35);border-radius:16px;background:rgba(10,92,73,.9);box-shadow:0 5px 16px rgba(16,74,51,.16)}.mg-game__status-kicker,.mg-game__status-stage,.mg-game__status-meta text{display:block}.mg-game__status-kicker{font-size:10px;opacity:.78}.mg-game__status-stage{margin-top:1px;color:#85f363;font-size:clamp(17px,4vh,28px);font-weight:900}.mg-game__motion-track{position:relative;width:8px;height:52px;overflow:hidden;border-radius:999px;background:rgba(255,255,255,.4)}.mg-game__motion-track>view{position:absolute;bottom:0;left:0;width:100%;border-radius:999px;background:linear-gradient(#f4d340,#67f05a);transition:height 80ms linear}.mg-game__status-meta{font-size:10px;line-height:1.45}.mg-game__warning-text{color:#ffe083}.mg-game__prompt{position:absolute;z-index:24;left:50%;bottom:3.4%;display:flex;align-items:center;justify-content:center;gap:8px;max-width:48vw;padding:9px 18px;border:2px solid rgba(255,255,255,.78);border-radius:999px;background:rgba(255,255,245,.94);transform:translateX(-50%);color:#44612c;font-size:clamp(12px,2.8vh,18px);font-weight:900;box-shadow:0 5px 14px rgba(67,84,32,.16);white-space:nowrap}.mg-game__prompt--danger{border-color:#ffd451;background:#fff8dd;color:#7a4b0e}.mg-game__prompt-arrow{color:#ffad26;font-size:22px}.mg-game__score-card{position:absolute;z-index:22;right:2%;bottom:2.4%;display:flex;gap:6px;padding:7px;border-radius:16px;background:rgba(10,92,73,.9);box-shadow:0 5px 16px rgba(16,74,51,.16)}.mg-game__score-card>view{min-width:54px;padding:4px 7px;border-right:1px solid rgba(255,255,255,.17);text-align:center}.mg-game__score-card>view:last-child{border-right:0}.mg-game__score-card text,.mg-game__score-card strong{display:block}.mg-game__score-card text{font-size:9px;opacity:.82}.mg-game__score-card strong{font-size:17px}.mg-game__score-main strong{color:#ffd94d}.mg-game__overlay{position:absolute;z-index:50;inset:0;display:flex;align-items:center;justify-content:center;background:rgba(10,42,39,.5)}.mg-game__overlay-card{width:min(430px,72vw);box-sizing:border-box;padding:22px;border:3px solid #5fc59f;border-radius:22px;background:#fff;color:#315f4d;text-align:center}.mg-game__overlay-title,.mg-game__overlay-text{display:block}.mg-game__overlay-title{font-size:28px;font-weight:900}.mg-game__overlay-text{margin-top:6px}.mg-game__overlay-actions{display:flex;justify-content:center;gap:10px;margin-top:15px}.mg-game__overlay-actions button{min-width:130px;height:42px;margin:0;border:0;border-radius:999px;background:#54b82c;color:#fff;font-weight:900}.mg-game__overlay-actions .mg-game__overlay-exit{background:#edf3ee;color:#426052}.mg-game__debug{position:absolute;z-index:60;right:2%;bottom:12%;display:flex;gap:5px}.mg-game__debug button{height:36px;margin:0;padding:0 10px;border:0;border-radius:10px;background:rgba(255,255,255,.94);color:#44602d;font-size:11px}.mg-game__debug button::after,.mg-game__hud-btn::after,.mg-game__overlay-actions button::after{border:0}@keyframes mgPulse{from{transform:translateX(-50%) scale(.94);opacity:.7}to{transform:translateX(-50%) scale(1.04);opacity:1}}@keyframes mgHammerHover{from{transform:translate(-50%,-50%) rotate(-23deg)}to{transform:translate(-50%,-55%) rotate(-11deg)}}@media(max-height:420px){.mg-game__hud{height:48px}.mg-game__room-strip{top:calc(var(--mg-safe-top,8px) + 52px)}.mg-game__arena{inset:8% 0 7%}.mg-game__lane{width:clamp(98px,15vw,146px);height:clamp(88px,24vh,136px)}.mg-game__status-card{padding:6px 9px}.mg-game__motion-track{height:42px}.mg-game__prompt{bottom:2.2%;padding:7px 13px}.mg-game__score-card{bottom:1.8%;padding:5px}.mg-game__debug{bottom:15%}}@media(max-width:720px) and (orientation:landscape){.mg-game__hud{left:1%;right:1%;gap:5px;padding-left:10px}.mg-game__hud-btn{min-width:52px;padding:0 9px}.mg-game__lane-label{font-size:8px}.mg-game__status-card{left:1%;width:170px}.mg-game__score-card{right:1%}.mg-game__score-card>view{min-width:40px;padding:3px 4px}.mg-game__prompt{max-width:42vw;font-size:11px}}
/* Supplied five-hole scene: every gameplay layer shares the source-image anchor. */
.mg-game__shade{background:linear-gradient(180deg,rgba(0,70,83,.02),transparent 35%,rgba(28,61,14,.1))}
.mg-game__title-sign{left:1.5%;top:calc(var(--mg-safe-top,8px) + clamp(48px,9.2vh,66px) + 3px);width:clamp(118px,16vw,196px);height:clamp(56px,11vh,90px);padding:0;border:0;border-radius:0;background:none;box-shadow:none;transform:none;filter:drop-shadow(0 5px 5px rgba(74,48,19,.2))}
.mg-game__arena{inset:0}
.mg-game__lane{width:clamp(112px,15.2vw,174px);height:clamp(100px,25vh,164px)}
.mg-game__mole-window{bottom:17%;width:88%;height:86%;border-radius:48% 48% 34% 34%}
.mg-game__mole{bottom:-1%}
.mg-game__hole{bottom:0;width:104%;height:40%;filter:drop-shadow(0 5px 4px rgba(88,45,18,.18))}
.mg-game__warning-ring{bottom:-5%;width:116%;height:50%}
.mg-game__lane-label{bottom:-13px;background:rgba(32,72,42,.82)}
@media(max-height:420px){.mg-game__arena{inset:0}.mg-game__lane{width:clamp(104px,15vw,146px);height:clamp(94px,24vh,136px)}}
</style>
