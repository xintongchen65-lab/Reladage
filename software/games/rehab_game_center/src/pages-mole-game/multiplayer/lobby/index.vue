<template>
  <view class="mg-lobby" :style="viewportCss">
    <view class="mg-lobby__card">
      <view class="mg-lobby__top">
        <button @click="exit">← 返回</button>
        <view class="mg-lobby__connection" :class="`mg-lobby__connection--${connectionTone}`"><i /><text>{{ connectionLabel }}</text></view>
      </view>
      <text class="mg-lobby__title">多人地鼠大作战</text>
      <text class="mg-lobby__sub">2～5 人私有房间</text>
      <view class="mg-lobby__modes">
        <button :disabled="!canSubmit" @click="create('PK')">创建多人PK</button>
        <button class="mg-lobby__coop" :disabled="!canSubmit" @click="create('COOP')">创建合作房间</button>
      </view>
      <view class="mg-lobby__join">
        <input v-model="roomCode" :disabled="!canSubmit" type="number" maxlength="6" placeholder="输入6位房间码" />
        <button :disabled="!canSubmit" @click="join">加入房间</button>
      </view>
      <button v-if="connectionState === 'FAILED'" class="mg-lobby__retry" @click="retry">重新连接</button>
      <text class="mg-lobby__tip">训练参数：{{ config.targetSets }}组，每组{{ config.targetCount }}次。只有参数一致的玩家才能准备。</text>
    </view>
  </view>
</template>

<script lang="ts">
import type { MultiplayerBootstrap, RoomMode } from '../../../game-platform/multiplayer/types'
import type { MultiplayerClient, MultiplayerConnectionState } from '../../../game-platform/multiplayer/client'
import { shouldClearMultiplayerOnUnload } from '../../../game-platform/runtime/multiplayer-session'
import { appendReturnUrl, configureCallerReturnUrl, returnToCaller } from '../../../game-platform/runtime/navigation'
import { resolveDebugEnabled } from '../../../game-platform/runtime/debug-gate'
import { getViewportLayout, viewportStyle } from '../../../game-platform/runtime/viewport'
import { configureMoleSession, getMoleConfig, registerMoleResultEmitter } from '../../runtime/session-runtime'
import { clearMoleMultiplayer, initializeMoleMultiplayer, registerMoleMultiplayerEmitter } from '../../runtime/multiplayer-runtime'

function guest(config: ReturnType<typeof getMoleConfig>): MultiplayerBootstrap {
  const suffix = Math.floor(Math.random() * 9000 + 1000).toString()
  return {
    identity: { playerId: `guest-${Date.now()}-${suffix}`, displayName: `玩家${suffix}` },
    wsEndpoint: 'ws://127.0.0.1:8787',
    gameId: 'mole',
    trainingConfig: { targetSets: config.targetSets, targetCount: config.targetCount, gameId: 'mole' }
  }
}

export default {
  name: 'MoleLobby',
  data() {
    return {
      client: null as MultiplayerClient | null,
      config: getMoleConfig(),
      roomCode: '',
      connectionState: 'CONNECTING' as MultiplayerConnectionState,
      actionPending: false,
      navigating: false,
      leaving: false,
      viewport: getViewportLayout(),
      roomUnsubscribe: null as (() => void) | null,
      connectionUnsubscribe: null as (() => void) | null,
      resultChannel: null as any
    }
  },
  computed: {
    viewportCss(): Record<string, string> { return viewportStyle(this.viewport, '--mg-safe-top') },
    canSubmit(): boolean { return this.connectionState === 'CONNECTED' && !this.actionPending },
    connectionLabel(): string {
      if (this.connectionState === 'CONNECTED') return '已连接'
      if (this.connectionState === 'FAILED' || this.connectionState === 'CLOSED') return '连接失败'
      return this.connectionState === 'RECONNECTING' ? '正在连接（重试中）' : '正在连接'
    },
    connectionTone(): string {
      if (this.connectionState === 'CONNECTED') return 'success'
      if (this.connectionState === 'FAILED' || this.connectionState === 'CLOSED') return 'error'
      return 'pending'
    }
  },
  onLoad(query: Record<string, string | undefined>) {
    configureCallerReturnUrl(query.returnUrl, '/pages-mole-game/home/index')
    this.config = configureMoleSession({ ...this.config, debugEnabled: resolveDebugEnabled(query.debug) })
    const channel = (this as any).getOpenerEventChannel?.()
    this.resultChannel = channel
    channel?.on('moleGameMultiplayerBootstrap', (bootstrap: MultiplayerBootstrap) => this.setup(bootstrap))
    registerMoleResultEmitter((result) => channel?.emit('moleGameResult', result))
    setTimeout(() => { if (!this.client) this.setup(guest(this.config)) }, 80)
  },
  onShow() { this.navigating = false; this.actionPending = false },
  onResize() { this.viewport = getViewportLayout() },
  onBackPress() { this.exit(); return true },
  onUnload() {
    this.cleanup()
    if (shouldClearMultiplayerOnUnload(this.navigating, this.leaving)) clearMoleMultiplayer()
  },
  methods: {
    setup(bootstrap: MultiplayerBootstrap) {
      if (this.client) return
      this.config = configureMoleSession({ targetSets: bootstrap.trainingConfig?.targetSets, targetCount: bootstrap.trainingConfig?.targetCount, debugEnabled: this.config.debugEnabled })
      this.client = initializeMoleMultiplayer({ ...bootstrap, gameId: 'mole' })
      registerMoleMultiplayerEmitter((result) => this.resultChannel?.emit('moleGameMultiplayerResult', result))
      this.client.onError((message) => { this.actionPending = false; uni.showToast({ title: message, icon: 'none' }) })
      this.connectionUnsubscribe = this.client.subscribeConnection((state) => {
        this.connectionState = state
        if (state !== 'CONNECTED') this.actionPending = false
      })
      this.roomUnsubscribe = this.client.store.subscribe((snapshot) => {
        this.actionPending = false
        if (!snapshot || this.navigating) return
        this.navigating = true
        uni.navigateTo({ url: appendReturnUrl('/pages-mole-game/multiplayer/room/index'), fail: () => { this.navigating = false } })
      })
    },
    create(mode: RoomMode) {
      if (!this.canSubmit || !this.client) return
      this.actionPending = true
      if (!this.client.createRoom(mode, { targetSets: this.config.targetSets, targetCount: this.config.targetCount, gameId: 'mole' })) this.actionPending = false
    },
    join() {
      if (!this.canSubmit || !this.client) return
      const code = this.roomCode.trim()
      if (!/^\d{6}$/.test(code)) { uni.showToast({ title: '请输入6位房间码', icon: 'none' }); return }
      this.actionPending = true
      if (!this.client.joinRoom(code, { targetSets: this.config.targetSets, targetCount: this.config.targetCount, gameId: 'mole' })) this.actionPending = false
    },
    retry() { this.actionPending = false; this.client?.retryConnection() },
    exit() {
      if (this.leaving) return
      this.leaving = true
      this.cleanup()
      registerMoleResultEmitter(null)
      clearMoleMultiplayer()
      returnToCaller()
    },
    cleanup() {
      this.roomUnsubscribe?.(); this.roomUnsubscribe = null
      this.connectionUnsubscribe?.(); this.connectionUnsubscribe = null
    }
  }
}
</script>

<style scoped>
.mg-lobby{display:flex;align-items:center;justify-content:center;box-sizing:border-box;min-height:100vh;padding:calc(var(--mg-safe-top,8px) + 10px) 20px 22px;background:linear-gradient(145deg,#e7f6d7,#fff0b8);font-family:"Microsoft YaHei",sans-serif;color:#42592a}.mg-lobby__card{box-sizing:border-box;width:min(540px,94vw);padding:25px;border:3px solid #7aa43e;border-radius:22px;background:#fff}.mg-lobby__top{display:flex;align-items:center;justify-content:space-between;gap:10px}.mg-lobby__top button,.mg-lobby__retry{display:flex;align-items:center;justify-content:center;height:40px;margin:0;padding:0 14px;border:0;border-radius:999px;background:#edf6e4;color:#527431}.mg-lobby__connection{display:flex;align-items:center;gap:6px;font-weight:800}.mg-lobby__connection i{width:10px;height:10px;border-radius:50%;background:#e3a326}.mg-lobby__connection--success{color:#26833b}.mg-lobby__connection--success i{background:#37ad50}.mg-lobby__connection--error{color:#c64a36}.mg-lobby__connection--error i{background:#df5341}.mg-lobby__title,.mg-lobby__sub,.mg-lobby__tip{display:block}.mg-lobby__title{margin-top:12px;color:#70932d;font-size:34px;font-weight:900}.mg-lobby__sub{color:#738464}.mg-lobby__modes{display:flex;gap:10px;margin-top:16px}.mg-lobby__modes button{display:flex;flex:1;align-items:center;justify-content:center;box-sizing:border-box;min-width:0;height:56px;margin:0;padding:0 8px;border:0;border-radius:14px;background:#e8ae29;color:#5a4008;font-size:17px;font-weight:900;line-height:1.2;white-space:nowrap}.mg-lobby__modes .mg-lobby__coop{background:#78a843;color:#fff}.mg-lobby__join{display:flex;gap:9px;margin-top:14px}.mg-lobby__join input{flex:1;height:46px;padding:0 12px;border:2px solid #cdddaf;border-radius:12px}.mg-lobby__join button{display:flex;align-items:center;justify-content:center;width:120px;height:48px;margin:0;padding:0;border:0;border-radius:12px;background:#e8ae29;color:#5a4008;font-weight:900}.mg-lobby button[disabled]{background:#bdcbb0;color:#eef3e8}.mg-lobby__retry{margin:12px auto 0;background:#e79028;color:#fff}.mg-lobby__tip{margin-top:14px;color:#788668;font-size:12px}.mg-lobby button::after{border:0}
</style>
