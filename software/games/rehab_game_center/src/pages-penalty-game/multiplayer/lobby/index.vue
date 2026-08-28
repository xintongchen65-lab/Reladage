<template>
  <view class="rpg-lobby" :style="viewportCss">
    <view class="rpg-lobby__card">
      <view class="rpg-lobby__top"><button @click="exit">← 返回</button><text :class="tone">● {{ label }}</text></view>
      <text class="rpg-lobby__title">多人点球大战</text><text class="rpg-lobby__sub">2～5人私有房间 · 相同训练参数</text>
      <view class="rpg-lobby__modes"><button :disabled="!canSubmit" @click="create('PK')">创建多人PK</button><button :disabled="!canSubmit" class="rpg-lobby__coop" @click="create('COOP')">创建合作房间</button></view>
      <view class="rpg-lobby__join"><input v-model="roomCode" maxlength="6" type="number" placeholder="输入6位房间码"/><button :disabled="!canSubmit" @click="join">加入房间</button></view>
      <button v-if="connectionState==='FAILED'||connectionState==='CLOSED'" class="rpg-lobby__retry" @click="retry">重新连接</button>
      <text class="rpg-lobby__tip">{{ identity }} · {{ config.targetSets }}组，每组左右各{{ config.targetCount }}次</text>
    </view>
  </view>
</template>

<script lang="ts">
import { appendReturnUrl, configureCallerReturnUrl, returnToCaller } from '../../../game-platform/runtime/navigation'
import { shouldClearMultiplayerOnUnload } from '../../../game-platform/runtime/multiplayer-session'
import { getViewportLayout, viewportStyle } from '../../../game-platform/runtime/viewport'
import type { MultiplayerClient, MultiplayerConnectionState } from '../../../pages-fruit-game/runtime/multiplayer-client'
import type { MultiplayerBootstrap, RoomMode } from '../../../game-platform/multiplayer/types'
import type { PenaltyTrainingResult } from '../../types/result'
import { clearPenaltyMultiplayer, initializePenaltyMultiplayer, registerPenaltyMultiplayerEmitter } from '../../runtime/multiplayer-runtime'
import { configurePenaltySession, registerPenaltyResultEmitter } from '../../runtime/session-runtime'

function guest(): MultiplayerBootstrap {
  const suffix = String(Math.floor(Math.random() * 9000 + 1000))
  return { identity: { playerId: `penalty-${Date.now()}-${suffix}`, displayName: `访客${suffix}` }, wsEndpoint: 'ws://127.0.0.1:8787', gameId: 'penalty', trainingConfig: { targetSets: 3, targetCount: 10, gameId: 'penalty' } }
}

export default {
  name: 'RpgLobby',
  data() { return { client: null as MultiplayerClient | null, connectionState: 'CONNECTING' as MultiplayerConnectionState, roomCode: '', pending: false, navigating: false, leaving: false, identity: '正在读取身份', config: { targetSets: 3, targetCount: 10 }, unsubscribe: null as (() => void) | null, connectionUnsubscribe: null as (() => void) | null, bootstrapTimer: null as ReturnType<typeof setTimeout> | null, channel: null as any, viewport: getViewportLayout() } },
  computed: {
    canSubmit(): boolean { return this.connectionState === 'CONNECTED' && !this.pending },
    label(): string { return this.connectionState === 'CONNECTED' ? '已连接' : this.connectionState === 'FAILED' || this.connectionState === 'CLOSED' ? '连接失败' : this.connectionState === 'RECONNECTING' ? '正在连接（重试中）' : '正在连接' },
    tone(): string { return this.connectionState === 'CONNECTED' ? 'rpg-lobby__ok' : this.connectionState === 'FAILED' || this.connectionState === 'CLOSED' ? 'rpg-lobby__bad' : '' },
    viewportCss(): Record<string, string> { return viewportStyle(this.viewport, '--rpg-safe-top') }
  },
  onLoad(query: Record<string, string | undefined>) {
    configureCallerReturnUrl(query?.returnUrl, '/pages-penalty-game/home/index')
    this.channel = (this as any).getOpenerEventChannel?.()
    this.channel?.on('penaltyGameMultiplayerBootstrap', (bootstrap: MultiplayerBootstrap) => this.setup(bootstrap))
    registerPenaltyResultEmitter((result: PenaltyTrainingResult) => this.channel?.emit('penaltyGameResult', result))
    this.bootstrapTimer = setTimeout(() => { if (!this.client) this.setup(guest()) }, 100)
  },
  onShow() { this.navigating = false; this.pending = false },
  onUnload() { this.cleanup(); if (shouldClearMultiplayerOnUnload(this.navigating, this.leaving)) clearPenaltyMultiplayer() },
  onResize() { this.viewport = getViewportLayout() },
  onBackPress() { this.exit(); return true },
  methods: {
    setup(bootstrap: MultiplayerBootstrap) {
      if (this.client) return
      this.identity = bootstrap.identity.displayName
      this.config = { targetSets: bootstrap.trainingConfig?.targetSets ?? 3, targetCount: bootstrap.trainingConfig?.targetCount ?? 10 }
      configurePenaltySession(this.config)
      this.client = initializePenaltyMultiplayer({ ...bootstrap, gameId: 'penalty', trainingConfig: { ...this.config, gameId: 'penalty' } })
      registerPenaltyMultiplayerEmitter((result) => this.channel?.emit('penaltyGameMultiplayerResult', result))
      this.client.onError((message) => { this.pending = false; uni.showToast({ title: message, icon: 'none' }) })
      this.connectionUnsubscribe = this.client.subscribeConnection((state) => { this.connectionState = state; if (state !== 'CONNECTED') this.pending = false })
      this.unsubscribe = this.client.store.subscribe((snapshot) => {
        this.pending = false
        if (snapshot && !this.navigating) { this.navigating = true; uni.navigateTo({ url: appendReturnUrl('/pages-penalty-game/multiplayer/room/index'), fail: () => { this.navigating = false } }) }
      })
    },
    create(mode: RoomMode) { if (!this.client || !this.canSubmit) return; this.pending = true; if (!this.client.createRoom(mode, { ...this.config, gameId: 'penalty' })) this.pending = false },
    join() { if (!this.client || !this.canSubmit) return; const code = this.roomCode.trim(); if (!/^\d{6}$/.test(code)) { uni.showToast({ title: '请输入6位房间码', icon: 'none' }); return } this.pending = true; if (!this.client.joinRoom(code, { ...this.config, gameId: 'penalty' })) this.pending = false },
    retry() { this.client?.retryConnection() },
    cleanup() { if (this.bootstrapTimer) clearTimeout(this.bootstrapTimer); this.bootstrapTimer = null; this.unsubscribe?.(); this.connectionUnsubscribe?.(); this.unsubscribe = null; this.connectionUnsubscribe = null },
    exit() { if (this.leaving) return; this.leaving = true; this.cleanup(); registerPenaltyResultEmitter(null); clearPenaltyMultiplayer(); returnToCaller() }
  }
}
</script>

<style scoped>
.rpg-lobby{display:flex;align-items:center;justify-content:center;box-sizing:border-box;min-height:100vh;padding:calc(var(--rpg-safe-top,8px) + 10px) 20px 22px;background:linear-gradient(145deg,#def3ff,#fff3bf);font-family:"Microsoft YaHei",sans-serif;color:#245471}.rpg-lobby__card{width:min(540px,94vw);box-sizing:border-box;padding:26px;border:3px solid #2b8bcc;border-radius:22px;background:#fff}.rpg-lobby__top{display:flex;align-items:center;justify-content:space-between}.rpg-lobby__top button,.rpg-lobby__retry{display:flex;align-items:center;justify-content:center;height:40px;margin:0;padding:0 14px;border:0;border-radius:999px;background:#e5f3fc;color:#236e9e}.rpg-lobby__ok{color:#269b4c}.rpg-lobby__bad{color:#dc4b3c}.rpg-lobby__title,.rpg-lobby__sub,.rpg-lobby__tip{display:block}.rpg-lobby__title{margin-top:12px;color:#0878c9;font-size:36px;font-weight:900}.rpg-lobby__sub{color:#738895}.rpg-lobby__modes{display:flex;gap:10px;margin-top:18px}.rpg-lobby__modes button{display:flex;flex:1;align-items:center;justify-content:center;box-sizing:border-box;min-width:0;height:56px;margin:0;padding:0 8px;border:0;border-radius:14px;background:#2d98dc;color:#fff;font-size:17px;font-weight:900;line-height:1.2;white-space:nowrap}.rpg-lobby__modes .rpg-lobby__coop{background:#3caf68}.rpg-lobby button[disabled]{background:#adc4d0;color:#edf3f6}.rpg-lobby__join{display:flex;gap:9px;margin-top:15px}.rpg-lobby__join input{flex:1;height:46px;padding:0 12px;border:2px solid #b8d7e9;border-radius:12px}.rpg-lobby__join button{display:flex;align-items:center;justify-content:center;width:120px;height:48px;margin:0;padding:0;border:0;border-radius:12px;background:#ffc72a;color:#714600;font-weight:900}.rpg-lobby__retry{margin:12px auto 0;background:#ef9c28;color:#fff}.rpg-lobby__tip{margin-top:16px;font-size:13px}
</style>
