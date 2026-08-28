<template>
  <view class="rfg-mp-lobby" :style="viewportCssVars">
    <view class="rfg-mp-lobby__card">
      <view class="rfg-mp-lobby__topbar">
        <button class="rfg-mp-lobby__back" @click="exitLobby">← 返回</button>
        <view class="rfg-mp-lobby__connection" :class="`rfg-mp-lobby__connection--${connectionTone}`">
          <text class="rfg-mp-lobby__connection-dot" />
          <text>{{ connectionLabel }}</text>
        </view>
      </view>
      <text class="rfg-mp-lobby__title">多人摘水果</text>
      <text class="rfg-mp-lobby__sub">2～5人私有房间</text>
      <view class="rfg-mp-lobby__modes">
        <button class="rfg-mp-lobby__button" :disabled="!canSubmit" @click="create('PK')">创建多人PK</button>
        <button class="rfg-mp-lobby__button rfg-mp-lobby__button--coop" :disabled="!canSubmit" @click="create('COOP')">创建合作房间</button>
      </view>
      <view class="rfg-mp-lobby__join">
        <input v-model="roomCode" class="rfg-mp-lobby__input" :disabled="!canSubmit" type="number" maxlength="6" placeholder="输入6位房间码" />
        <button class="rfg-mp-lobby__join-button" :disabled="!canSubmit" @click="join">加入房间</button>
      </view>
      <button v-if="connectionState === 'FAILED'" class="rfg-mp-lobby__retry" @click="retryConnection">重新连接</button>
      <text class="rfg-mp-lobby__identity">当前玩家：{{ identityName }}</text>
      <text class="rfg-mp-lobby__tip">训练参数：{{ trainingConfig.targetSets }}组，每组左右各{{ trainingConfig.targetCount }}次；加入者参数必须一致。</text>
    </view>
  </view>
</template>
<script lang="ts">
import type { MultiplayerBootstrap, RoomMode } from '../../types/multiplayer'
import { clearMultiplayerRuntime, initializeMultiplayerRuntime, registerMultiplayerResultEmitter } from '../../runtime/multiplayer-runtime'
import { configureSession, registerResultEmitter } from '../../runtime/session-runtime'
import { resolveDebugEnabled } from '../../runtime/launch-config'
import { appendReturnUrl, configureCallerReturnUrl, returnToCaller } from '../../runtime/navigation-runtime'
import { getViewportLayout, viewportStyle } from '../../runtime/viewport-layout'
import type { MultiplayerClient, MultiplayerConnectionState } from '../../runtime/multiplayer-client'
import { shouldClearMultiplayerOnUnload } from '../../../game-platform/runtime/multiplayer-session'

function guestBootstrap(): MultiplayerBootstrap {
  const suffix = Math.floor(Math.random() * 9000 + 1000).toString()
  return {
    identity: { playerId: `guest-${Date.now()}-${suffix}`, displayName: `访客${suffix}` },
    wsEndpoint: 'ws://127.0.0.1:8787',
    trainingConfig: { targetSets: 3, targetCount: 10 }
  }
}

export default {
  name: 'RfgMultiplayerLobby',
  data() { return { client: null as MultiplayerClient | null, roomCode: '', identityName: '正在读取身份', trainingConfig: { targetSets: 3, targetCount: 10 }, debugEnabled: false, connectionState: 'CONNECTING' as MultiplayerConnectionState, actionPending: false, navigating: false, leaving: false, viewportLayout: getViewportLayout(), unsubscribe: null as (() => void) | null, connectionUnsubscribe: null as (() => void) | null, bootstrapTimer: null as ReturnType<typeof setTimeout> | null, resultChannel: null as any } },
  computed: {
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
    },
    viewportCssVars(): Record<string, string> { return viewportStyle(this.viewportLayout) }
  },
  onLoad(query: Record<string, string | undefined>) {
    configureCallerReturnUrl(query?.returnUrl)
    this.debugEnabled = resolveDebugEnabled(query?.debug)
    const channel = (this as any).getOpenerEventChannel?.()
    this.resultChannel = channel
    channel?.on('fruitGameMultiplayerBootstrap', (bootstrap: MultiplayerBootstrap) => this.setup(bootstrap))
    registerResultEmitter((result) => channel?.emit('fruitGameResult', result))
    this.bootstrapTimer = setTimeout(() => { if (!this.client) this.setup(guestBootstrap()) }, 100)
  },
  onShow() { this.navigating = false; this.actionPending = false },
  onResize() { this.viewportLayout = getViewportLayout() },
  onBackPress() { this.exitLobby(); return true },
  onUnload() { this.cleanupLobby(); if (shouldClearMultiplayerOnUnload(this.navigating, this.leaving)) clearMultiplayerRuntime() },
  methods: {
    setup(bootstrap: MultiplayerBootstrap): void {
      if (this.client) return
      this.identityName = bootstrap.identity.displayName
      this.trainingConfig = bootstrap.trainingConfig ?? { targetSets: 3, targetCount: 10 }
      configureSession({
        targetSets: this.trainingConfig.targetSets,
        targetCount: this.trainingConfig.targetCount,
        debugEnabled: this.debugEnabled
      })
      this.client = initializeMultiplayerRuntime(bootstrap)
      registerMultiplayerResultEmitter((result) => this.resultChannel?.emit('fruitGameMultiplayerResult', result))
      this.client.onError((message) => {
        this.actionPending = false
        uni.showToast({ title: message, icon: 'none' })
      })
      this.connectionUnsubscribe = this.client.subscribeConnection((state) => {
        this.connectionState = state
        if (state !== 'CONNECTED') this.actionPending = false
      })
      this.unsubscribe = this.client.store.subscribe((snapshot) => {
        this.actionPending = false
        if (!snapshot || this.navigating) return
        this.navigating = true
        uni.navigateTo({ url: appendReturnUrl('/pages-fruit-game/multiplayer/room/index') })
      })
    },
    create(mode: RoomMode): void {
      if (!this.canSubmit || !this.client) return
      this.actionPending = true
      if (!this.client.createRoom(mode, this.trainingConfig)) this.actionPending = false
    },
    join(): void {
      if (!this.canSubmit || !this.client) return
      const code = this.roomCode.trim()
      if (!/^\d{6}$/.test(code)) { uni.showToast({ title: '请输入6位房间码', icon: 'none' }); return }
      this.actionPending = true
      if (!this.client.joinRoom(code, this.trainingConfig)) this.actionPending = false
    },
    retryConnection(): void {
      this.actionPending = false
      this.client?.retryConnection()
    },
    exitLobby(): void {
      if (this.leaving) return
      this.leaving = true
      this.cleanupLobby()
      registerResultEmitter(null)
      clearMultiplayerRuntime()
      returnToCaller()
    },
    cleanupLobby(): void {
      if (this.bootstrapTimer) clearTimeout(this.bootstrapTimer)
      this.bootstrapTimer = null
      this.unsubscribe?.()
      this.unsubscribe = null
      this.connectionUnsubscribe?.()
      this.connectionUnsubscribe = null
    }
  }
}
</script>
<style scoped>
.rfg-mp-lobby { display:flex; align-items:center; justify-content:center; box-sizing:border-box; min-height:100vh; padding:calc(var(--rfg-safe-top,8px) + 10px) 24px 24px; background:linear-gradient(145deg,#e7f9d8,#fff4c7); color:#365126; font-family:"Microsoft YaHei",sans-serif; }
.rfg-mp-lobby__card { width:min(520px,92vw); box-sizing:border-box; padding:28px; border:3px solid #70a93d; border-radius:22px; background:#fff; box-shadow:0 12px 30px rgba(49,91,35,.2); }
.rfg-mp-lobby__topbar{display:flex;align-items:center;justify-content:space-between;gap:12px;margin-bottom:10px}.rfg-mp-lobby__back{display:flex;align-items:center;justify-content:center;height:40px;margin:0;padding:0 14px;border-radius:999px;background:#eaf5df;color:#397527;font-size:15px;font-weight:800;line-height:1}.rfg-mp-lobby__connection{display:flex;align-items:center;gap:6px;font-size:14px;font-weight:800}.rfg-mp-lobby__connection-dot{width:10px;height:10px;border-radius:50%;background:#e7a623}.rfg-mp-lobby__connection--success{color:#27813a}.rfg-mp-lobby__connection--success .rfg-mp-lobby__connection-dot{background:#3db653}.rfg-mp-lobby__connection--error{color:#c74635}.rfg-mp-lobby__connection--error .rfg-mp-lobby__connection-dot{background:#e25442}
.rfg-mp-lobby__title,.rfg-mp-lobby__sub,.rfg-mp-lobby__identity,.rfg-mp-lobby__tip { display:block; }
.rfg-mp-lobby__title { color:#24722e; font-size:36px; font-weight:900; }.rfg-mp-lobby__sub{margin:4px 0 18px;color:#718164}
.rfg-mp-lobby__modes{display:flex;gap:10px}.rfg-mp-lobby__button{display:flex;flex:1;align-items:center;justify-content:center;box-sizing:border-box;min-width:0;height:56px;margin:0;padding:0 8px;border-radius:14px;background:#68c527;color:#fff;font-size:17px;font-weight:900;line-height:1.2;white-space:nowrap}.rfg-mp-lobby__button--coop{background:#2fa66b}.rfg-mp-lobby__button[disabled],.rfg-mp-lobby__join-button[disabled]{background:#afc5a3;color:#eef3ea}
.rfg-mp-lobby__join{display:flex;gap:8px;margin-top:16px}.rfg-mp-lobby__input{flex:1;height:44px;padding:0 12px;border:2px solid #b9cf9f;border-radius:12px}.rfg-mp-lobby__join-button{width:120px;background:#3195d3}
.rfg-mp-lobby__join-button{display:flex;align-items:center;justify-content:center;box-sizing:border-box;height:48px;margin:0;padding:0 8px;border-radius:14px;color:#fff;font-size:16px;font-weight:900;line-height:1.2;white-space:nowrap}.rfg-mp-lobby__retry{display:flex;align-items:center;justify-content:center;width:160px;height:42px;margin:12px auto 0;padding:0 12px;border-radius:999px;background:#f09a2b;color:#fff;font-size:15px;font-weight:900;line-height:1}.rfg-mp-lobby__back::after,.rfg-mp-lobby__button::after,.rfg-mp-lobby__join-button::after,.rfg-mp-lobby__retry::after{border:0}
.rfg-mp-lobby__identity{margin-top:18px;font-weight:800}.rfg-mp-lobby__tip{margin-top:7px;color:#79836f;font-size:12px;line-height:1.5}
</style>
