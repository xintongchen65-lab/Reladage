<template>
  <view class="rfg-demo" :style="viewportCssVars">
    <view class="rfg-demo__card">
      <view class="rfg-demo__intro">
        <text class="rfg-demo__eyebrow">RehabMotion 康复训练</text>
        <text class="rfg-demo__title">摘水果</text>
        <text class="rfg-demo__subtitle">肘关节屈伸 · 左右侧交替训练</text>

        <view class="rfg-demo__info">
          <view class="rfg-demo__row"><text>数据源</text><text>FakeDataSource</text></view>
          <view class="rfg-demo__row"><text>输出频率</text><text>25 Hz</text></view>
          <view class="rfg-demo__row"><text>目标次数</text><text>左 10 / 右 10</text></view>
        </view>

        <view class="rfg-demo__entry-buttons">
          <button class="rfg-demo__start" @click="enterGame">单人训练</button>
          <button class="rfg-demo__start rfg-demo__start--multi" @click="enterMultiplayer">多人模式</button>
        </view>
        <text class="rfg-demo__hint">H5：W/↑ 屈肘增大角度，S/↓ 伸肘减小角度；返回低角度后自动判次</text>
        <text class="rfg-demo__hint">微信：进入游戏后自动横屏，可使用屏幕角度控制按钮</text>
        <text v-if="lastResultText" class="rfg-demo__result">最近结果：{{ lastResultText }}</text>
      </view>

      <scroll-view class="rfg-demo__records" :scroll-y="isLandscape" :show-scrollbar="isLandscape">
        <view class="rfg-demo__records-header">
          <text class="rfg-demo__records-title">训练记录</text>
          <text class="rfg-demo__records-count">共 {{ records.length }} 条</text>
        </view>

        <text v-if="records.length === 0" class="rfg-demo__empty">完成至少一次有效动作后，训练记录会显示在这里</text>

        <view
          v-for="record in visibleRecords"
          :key="record.id"
          class="rfg-demo__record"
          @click="toggleRecord(record.id)"
        >
          <view class="rfg-demo__record-summary">
            <view class="rfg-demo__record-heading">
              <text class="rfg-demo__record-status" :class="`rfg-demo__record-status--${record.status.toLowerCase()}`">
                {{ statusText(record.status) }}
              </text>
              <text class="rfg-demo__record-time">{{ formatDate(record.result.completedAtMs) }}</text>
            </view>
            <text class="rfg-demo__record-line">
              左 {{ record.result.training.left_total_count }} / 右 {{ record.result.training.right_total_count }} · 整体完成度 {{ record.result.training.overall_completion_percent }}%
            </text>
            <text class="rfg-demo__record-line">
              水果 {{ record.result.game.harvestedCount }} 个 · 得分 {{ record.result.game.score }}
            </text>
            <text class="rfg-demo__record-expand">{{ expandedRecordId === record.id ? '收起详情' : '查看详情' }}</text>
          </view>

          <view v-if="expandedRecordId === record.id" class="rfg-demo__record-detail" @click.stop>
            <view class="rfg-demo__detail-section">
              <text class="rfg-demo__detail-title">康复训练数据</text>
              <view class="rfg-demo__detail-grid">
                <view class="rfg-demo__detail-item"><text>左侧总有效次数</text><text>{{ record.result.training.left_total_count }}</text></view>
                <view class="rfg-demo__detail-item"><text>右侧总有效次数</text><text>{{ record.result.training.right_total_count }}</text></view>
                <view class="rfg-demo__detail-item"><text>左侧最大 ROM</text><text>{{ formatNumber(record.result.training.session_left_rom_deg || 0) }}°</text></view>
                <view class="rfg-demo__detail-item"><text>右侧最大 ROM</text><text>{{ formatNumber(record.result.training.session_right_rom_deg || 0) }}°</text></view>
                <view class="rfg-demo__detail-item"><text>训练组次</text><text>{{ record.result.training.set_index }}/{{ record.result.training.target_sets }}</text></view>
                <view class="rfg-demo__detail-item"><text>整体完成度</text><text>{{ record.result.training.overall_completion_percent }}%</text></view>
              </view>
            </view>
            <view class="rfg-demo__detail-section rfg-demo__detail-section--game">
              <text class="rfg-demo__detail-title">游戏成绩</text>
              <view class="rfg-demo__detail-row"><text>得分</text><text>{{ record.result.game.score }}</text></view>
              <view class="rfg-demo__detail-row"><text>摘取水果</text><text>{{ record.result.game.harvestedCount }} 个</text></view>
              <view class="rfg-demo__detail-row"><text>普通水果</text><text>{{ record.result.game.normalFruitCount }} 个</text></view>
              <view class="rfg-demo__detail-row"><text>特殊水果</text><text>金 {{ record.result.game.goldenAppleCount }} · 彩 {{ record.result.game.rainbowFruitCount }} · 双 {{ record.result.game.bothWatermelonCount }}</text></view>
              <view class="rfg-demo__detail-row"><text>最大连击</text><text>×{{ record.result.game.maxCombo }}</text></view>
              <view class="rfg-demo__detail-row"><text>错侧动作</text><text>{{ record.result.game.wrongSideCount }} 次</text></view>
              <view class="rfg-demo__detail-row"><text>总用时</text><text>{{ seconds(record.result.elapsedMs) }} 秒</text></view>
              <view class="rfg-demo__detail-row"><text>有效训练时长</text><text>{{ seconds(record.result.activeElapsedMs) }} 秒</text></view>
            </view>
          </view>
        </view>

        <button v-if="records.length > 3" class="rfg-demo__all-records" @click="showAllRecords = !showAllRecords">
          {{ showAllRecords ? '收起记录' : `所有记录（${records.length}）` }}
        </button>
      </scroll-view>
    </view>
  </view>
</template>

<script lang="ts">
import {
  loadTrainingRecords,
  persistTrainingResult,
  selectVisibleTrainingRecords
} from './training-records'
import type {
  DemoRecordStatus,
  DemoTrainingRecord,
  DemoTrainingResult
} from './training-records'
import type { MultiplayerTrainingResult } from '../../pages-fruit-game/types/multiplayer'

function pad(value: number): string {
  return String(value).padStart(2, '0')
}

function getDemoSafeTopPx(): number {
  let safeTopPx = 10
  // #ifdef MP-WEIXIN
  try {
    const capsule = uni.getMenuButtonBoundingClientRect()
    if (capsule && Number.isFinite(capsule.bottom)) safeTopPx = Math.max(safeTopPx, capsule.bottom + 6)
  } catch {
    safeTopPx = 52
  }
  // #endif
  return safeTopPx
}

function getIsLandscape(): boolean {
  try {
    const info = uni.getWindowInfo ? uni.getWindowInfo() : uni.getSystemInfoSync()
    return Number(info.windowWidth) > Number(info.windowHeight)
  } catch {
    return false
  }
}

export default {
  name: 'RfgDemoPage',
  data() {
    return {
      lastResultText: '',
      records: [] as DemoTrainingRecord[],
      showAllRecords: false,
      expandedRecordId: '',
      navigating: false,
      safeTopPx: getDemoSafeTopPx(),
      isLandscape: getIsLandscape()
    }
  },
  computed: {
    visibleRecords(): DemoTrainingRecord[] {
      return selectVisibleTrainingRecords(this.records, this.showAllRecords)
    },
    viewportCssVars(): Record<string, string> {
      return { '--rfg-demo-safe-top': `${this.safeTopPx}px` }
    }
  },
  onLoad() {
    this.refreshRecords()
  },
  onShow() {
    this.navigating = false
    this.refreshRecords()
  },
  onResize() {
    this.safeTopPx = getDemoSafeTopPx()
    this.isLandscape = getIsLandscape()
  },
  methods: {
    enterGame(): void {
      if (this.navigating) return
      this.navigating = true
      const returnUrl = encodeURIComponent('/pages/demo/index')
      uni.navigateTo({
        url: `/pages-fruit-game/prepare/index?targetSets=3&targetCount=10&frameRateHz=25&targetAngleDeg=80&validAngleDeg=60&returnAngleDeg=20&restDurationSec=30&dataTimeoutMs=1000&debug=1&returnUrl=${returnUrl}`,
        events: {
          fruitGameResult: (result: DemoTrainingResult) => {
            this.lastResultText = `${result.endReason}，左 ${result.training.left_total_count ?? result.training.left_count} / 右 ${result.training.right_total_count ?? result.training.right_count}，水果 ${result.game.harvestedCount}`
            const persisted = persistTrainingResult(result)
            this.records = persisted.records
            if (persisted.reason === 'storage_error') {
              uni.showToast({ title: '记录保存失败，请检查本地存储空间', icon: 'none', duration: 3000 })
            }
          }
        },
        fail: () => {
          this.navigating = false
          uni.showToast({ title: '单人训练页面打开失败，请重试', icon: 'none' })
        }
      })
    },
    enterMultiplayer(): void {
      if (this.navigating) return
      this.navigating = true
      const suffix = Math.floor(Math.random() * 9000 + 1000).toString()
      const returnUrl = encodeURIComponent('/pages/demo/index')
      uni.navigateTo({
        url: `/pages-fruit-game/multiplayer/lobby/index?debug=1&returnUrl=${returnUrl}`,
        events: {
          fruitGameResult: (result: DemoTrainingResult) => {
            this.lastResultText = `多人断线后转单人，左 ${result.training.left_total_count ?? result.training.left_count} / 右 ${result.training.right_total_count ?? result.training.right_count}`
            const persisted = persistTrainingResult(result)
            this.records = persisted.records
          },
          fruitGameMultiplayerResult: (result: MultiplayerTrainingResult) => {
            this.lastResultText = `${result.mode} 房间 ${result.roomCode}，本地得分 ${result.localTraining.game.score}`
            const persisted = persistTrainingResult(result.localTraining)
            this.records = persisted.records
          }
        },
        success: (navigation) => {
          navigation.eventChannel.emit('fruitGameMultiplayerBootstrap', {
            identity: { playerId: `guest-${Date.now()}-${suffix}`, displayName: `访客${suffix}` },
            wsEndpoint: 'ws://127.0.0.1:8787',
            trainingConfig: { targetSets: 3, targetCount: 10 }
          })
        },
        fail: () => {
          this.navigating = false
          uni.showToast({ title: '多人模式页面打开失败，请重试', icon: 'none' })
        }
      })
    },
    refreshRecords(): void {
      this.records = loadTrainingRecords()
    },
    toggleRecord(id: string): void {
      this.expandedRecordId = this.expandedRecordId === id ? '' : id
    },
    statusText(status: DemoRecordStatus): string {
      return status === 'COMPLETED' ? '已完成' : '未完全完成'
    },
    formatDate(timestamp: number): string {
      const date = new Date(timestamp)
      return `${date.getFullYear()}-${pad(date.getMonth() + 1)}-${pad(date.getDate())} ${pad(date.getHours())}:${pad(date.getMinutes())}`
    },
    formatNumber(value: number): string {
      return Number(value).toFixed(1)
    },
    seconds(value: number): number {
      return Math.max(0, Math.round(Number(value) / 1000))
    }
  }
}
</script>

<style scoped>
.rfg-demo { box-sizing: border-box; display: flex; align-items: flex-start; justify-content: center; width: 100%; min-height: 100vh; min-height: 100dvh; padding: var(--rfg-demo-safe-top, 10px) 14px 24px; overflow: visible; background: linear-gradient(145deg, #e8f9dc, #fff8d8 55%, #d9f4ff); color: #2e491f; font-family: "Microsoft YaHei", sans-serif; }
.rfg-demo__card { box-sizing: border-box; display: flex; flex-direction: column; width: min(520px, 94vw); padding: 24px; overflow: visible; border: 3px solid #75aa3e; border-radius: 22px; background: rgba(255, 255, 255, 0.96); box-shadow: 0 12px 34px rgba(55, 105, 41, 0.2); }
.rfg-demo__intro { display: flex; flex: none; flex-direction: column; min-width: 0; }
.rfg-demo__eyebrow { color: #5b8e35; font-size: 14px; font-weight: 700; }
.rfg-demo__title { margin-top: 4px; color: #24722e; font-size: clamp(36px, 6vh, 52px); font-weight: 900; line-height: 1.1; }
.rfg-demo__subtitle { margin-top: 3px; color: #607054; font-size: 16px; }
.rfg-demo__info { margin: 16px 0; padding: 6px 16px; border-radius: 14px; background: #f2f9e9; }
.rfg-demo__row { display: flex; justify-content: space-between; gap: 16px; padding: 8px 0; border-bottom: 1px solid #d7e8c6; font-size: 15px; white-space: nowrap; }
.rfg-demo__row:last-child { border-bottom: 0; }
.rfg-demo__start { margin: 0; border-radius: 999px; background: linear-gradient(#8bd836, #4da617); color: #fff; font-size: 20px; font-weight: 900; line-height: 2.2; box-shadow: 0 5px 0 #347c12; }
.rfg-demo__entry-buttons{display:grid;grid-template-columns:1fr 1fr;gap:10px}.rfg-demo__start--multi{background:linear-gradient(#45b8ee,#247fc5);box-shadow:0 5px 0 #1d6397}
.rfg-demo__start::after, .rfg-demo__all-records::after { border: 0; }
.rfg-demo__hint { margin-top: 10px; color: #718166; font-size: 12px; line-height: 1.45; }
.rfg-demo__hint + .rfg-demo__hint { margin-top: 2px; }
.rfg-demo__result { margin-top: 8px; padding: 7px 10px; border-radius: 9px; background: #fff3cb; color: #76501d; font-size: 12px; }
.rfg-demo__records { display: block; flex: none; height: auto; margin-top: 16px; padding-top: 14px; overflow: visible; border-top: 2px solid #d9e9c7; }
.rfg-demo__records-header, .rfg-demo__record-heading { display: flex; align-items: center; justify-content: space-between; gap: 12px; }
.rfg-demo__records-title { color: #2d742d; font-size: 20px; font-weight: 900; }
.rfg-demo__records-count { color: #75816b; font-size: 13px; }
.rfg-demo__empty { display: block; margin-top: 10px; padding: 14px; border-radius: 11px; background: #f4f7ef; color: #849078; text-align: center; font-size: 13px; }
.rfg-demo__record { margin-top: 10px; overflow: hidden; border: 2px solid #d9c071; border-radius: 12px; background: #fffdf0; }
.rfg-demo__record-summary { display: flex; flex-direction: column; padding: 11px 14px; }
.rfg-demo__record-status { padding: 3px 8px; border-radius: 999px; color: #fff; font-size: 12px; font-weight: 900; }
.rfg-demo__record-status--completed { background: #52a923; }
.rfg-demo__record-status--partial { background: #dc8a21; }
.rfg-demo__record-time { color: #7f755e; font-size: 12px; }
.rfg-demo__record-line { margin-top: 5px; color: #654923; font-size: 13px; font-weight: 700; }
.rfg-demo__record-expand { align-self: flex-end; margin-top: 4px; color: #2984c7; font-size: 12px; }
.rfg-demo__record-detail { display: grid; grid-template-columns: 1.5fr 1fr; gap: 10px; padding: 11px; border-top: 1px solid #ead99d; background: #fffaf0; }
.rfg-demo__detail-section { padding: 9px; border-radius: 10px; background: #fff2c8; }
.rfg-demo__detail-section--game { background: #edf8dc; }
.rfg-demo__detail-title { color: #39772a; font-size: 13px; font-weight: 900; }
.rfg-demo__detail-grid { display: grid; grid-template-columns: repeat(2, 1fr); gap: 5px; margin-top: 7px; }
.rfg-demo__detail-item, .rfg-demo__detail-row { display: flex; justify-content: space-between; gap: 6px; padding: 4px 6px; border-bottom: 1px solid rgba(114, 83, 29, 0.14); color: #6d542f; font-size: 11px; }
.rfg-demo__detail-item { flex-direction: column; border-radius: 6px; background: rgba(255, 255, 255, 0.5); }
.rfg-demo__detail-item text:last-child, .rfg-demo__detail-row text:last-child { color: #5f3908; font-weight: 900; }
.rfg-demo__all-records { margin: 12px 0 0; border-radius: 999px; background: #e3f2d3; color: #39772a; font-size: 14px; font-weight: 900; }

@media (orientation: landscape) {
  .rfg-demo { align-items: center; width: 100vw; height: 100vh; height: 100dvh; min-height: 0; padding-bottom: 14px; overflow: hidden; }
  .rfg-demo__card { display: grid; grid-template-columns: minmax(280px, 0.92fr) minmax(300px, 1.08fr); gap: clamp(16px, 3vw, 34px); width: min(1000px, 94vw); height: min(560px, calc(100vh - var(--rfg-demo-safe-top, 10px) - 18px)); height: min(560px, calc(100dvh - var(--rfg-demo-safe-top, 10px) - 18px)); padding: clamp(12px, 2.5vh, 22px); }
  .rfg-demo__intro { justify-content: center; }
  .rfg-demo__title { font-size: clamp(30px, 8vh, 48px); }
  .rfg-demo__subtitle { font-size: clamp(13px, 2.6vh, 17px); }
  .rfg-demo__info { margin: clamp(8px, 2vh, 14px) 0; }
  .rfg-demo__row { padding: clamp(4px, 1.1vh, 7px) 0; font-size: clamp(12px, 2.2vh, 15px); }
  .rfg-demo__start { font-size: clamp(16px, 3.2vh, 20px); line-height: 2; }
  .rfg-demo__hint { margin-top: 6px; font-size: clamp(10px, 1.8vh, 12px); }
  .rfg-demo__records { min-height: 0; height: 100%; margin-top: 0; padding: 0 4px 0 0; overflow: hidden; border-top: 0; }
}

@media (max-height: 390px) and (orientation: landscape) {
  .rfg-demo__card { grid-template-columns: minmax(270px, 0.9fr) minmax(290px, 1.1fr); gap: 14px; padding: 10px 14px; }
  .rfg-demo__eyebrow { font-size: 11px; }
  .rfg-demo__info { padding: 3px 10px; }
  .rfg-demo__result { margin-top: 5px; }
}
</style>
