<template>
  <view class="rfg-home" :style="viewportCss">
    <view class="rfg-home__card">
      <button class="rfg-home__back" @click="back">← 游戏中心</button>
      <text class="rfg-home__eyebrow">肘关节屈伸康复训练</text>
      <text class="rfg-home__title">摘水果</text>
      <view class="rfg-home__buttons">
        <button class="rfg-home__mode" @click="single">单人训练</button>
        <button class="rfg-home__mode rfg-home__multi" @click="multi">多人模式</button>
      </view>
      <text class="rfg-home__hint">左右侧交替训练；康复次数来自动作数据，水果和得分属于游戏奖励</text>
      <view class="rfg-home__records">
        <view class="rfg-home__records-head"><text class="rfg-home__records-title">训练记录</text><text>共 {{ records.length }} 条</text></view>
        <text v-if="!records.length" class="rfg-home__empty">完成至少一次有效动作后显示记录</text>
        <view v-for="record in visible" :key="record.id" class="rfg-home__record" @click="expanded=expanded===record.id?'':record.id">
          <view class="rfg-home__record-top"><strong class="rfg-home__status">{{ record.status==='COMPLETED'?'已完成':'未完全完成' }}</strong><text>{{ dateText(record.result.completedAtMs) }}</text></view>
          <text class="rfg-home__line">左 {{ record.result.training.left_total_count }} / 右 {{ record.result.training.right_total_count }} · {{ record.result.training.overall_completion_percent }}%</text>
          <text class="rfg-home__line">水果 {{ record.result.game.harvestedCount }} 个 · {{ record.result.game.score }}分</text>
          <view v-if="expanded===record.id" class="rfg-home__detail">
            <text class="rfg-home__detail-line">最大ROM：左 {{ fmt(record.result.training.session_left_rom_deg) }}° / 右 {{ fmt(record.result.training.session_right_rom_deg) }}°</text>
            <text class="rfg-home__detail-line">特殊水果：金 {{ record.result.game.goldenAppleCount }} · 彩 {{ record.result.game.rainbowFruitCount }} · 双手 {{ record.result.game.bothWatermelonCount }}</text>
            <text class="rfg-home__detail-line">最大连击 ×{{ record.result.game.maxCombo }} · 错侧 {{ record.result.game.wrongSideCount }}次</text>
            <text class="rfg-home__detail-line">总用时 {{ seconds(record.result.elapsedMs) }}秒 · 有效 {{ seconds(record.result.activeElapsedMs) }}秒</text>
          </view>
        </view>
        <button v-if="records.length>3" class="rfg-home__all" @click="showAll=!showAll">{{ showAll?'收起记录':'所有记录' }}</button>
      </view>
    </view>
  </view>
</template>

<script lang="ts">
import { returnToGameCenter } from '../../game-platform/runtime/navigation'
import { getViewportLayout, viewportStyle } from '../../game-platform/runtime/viewport'
import type { MultiplayerTrainingResult } from '../types/multiplayer'
import type { TrainingResult } from '../types/result'
import { loadFruitRecords, saveFruitResult, type FruitRecord } from './records'

export default {
  name: 'RfgHome',
  data() { return { records: [] as FruitRecord[], showAll: false, expanded: '', navigating: false, viewport: getViewportLayout() } },
  computed: {
    visible(): FruitRecord[] { return this.showAll ? this.records : this.records.slice(0, 3) },
    viewportCss(): Record<string, string> { return viewportStyle(this.viewport) }
  },
  onLoad() { this.refresh() },
  onShow() { this.navigating = false; this.refresh() },
  onResize() { this.viewport = getViewportLayout() },
  methods: {
    single() { this.go('/pages-fruit-game/prepare/index') },
    multi() { this.go('/pages-fruit-game/multiplayer/lobby/index') },
    go(path: string) {
      if (this.navigating) return
      this.navigating = true
      const ret = encodeURIComponent('/pages-fruit-game/home/index')
      uni.navigateTo({ url: `${path}?debug=1&returnUrl=${ret}`, events: { fruitGameResult: (r: TrainingResult) => this.save(r), fruitGameMultiplayerResult: (r: MultiplayerTrainingResult) => this.save(r.localTraining) }, fail: () => { this.navigating = false } })
    },
    save(result: TrainingResult) { const saved = saveFruitResult(result); this.records = saved.records; if (saved.reason === 'storage_error') uni.showToast({ title: '记录保存失败', icon: 'none' }) },
    refresh() { this.records = loadFruitRecords() },
    back() { returnToGameCenter() },
    seconds(value: number) { return Math.round(value / 1000) },
    fmt(value: number | undefined) { return Number(value || 0).toFixed(1) },
    dateText(value: number) { const date = new Date(value); const pad = (item: number) => String(item).padStart(2, '0'); return `${date.getFullYear()}-${pad(date.getMonth() + 1)}-${pad(date.getDate())} ${pad(date.getHours())}:${pad(date.getMinutes())}` }
  }
}
</script>

<style scoped>
.rfg-home{box-sizing:border-box;min-height:100vh;padding:10px 14px 28px;background:linear-gradient(145deg,#e6f8da,#fff5c9);font-family:"Microsoft YaHei",sans-serif;color:#365429}.rfg-home__card{width:min(570px,94vw);box-sizing:border-box;margin:auto;padding:calc(var(--rfg-safe-top,8px) + 8px) 22px 22px;border:3px solid #6ca83e;border-radius:24px;background:#fff}.rfg-home__back{display:flex;align-items:center;justify-content:center;height:40px;margin:0 0 10px;padding:0 14px;border:0;border-radius:999px;background:#eaf5e1;color:#397527}.rfg-home__eyebrow,.rfg-home__title,.rfg-home__hint,.rfg-home__line,.rfg-home__detail-line{display:block}.rfg-home__eyebrow{font-weight:800}.rfg-home__title{color:#25772e;font-size:42px;font-weight:900}.rfg-home__buttons{display:flex;gap:10px;margin:14px 0}.rfg-home__mode{display:flex;flex:1;align-items:center;justify-content:center;height:56px;margin:0;padding:0 8px;border:0;border-radius:16px;background:#69c52a;color:#fff;font-size:18px;font-weight:900;white-space:nowrap}.rfg-home__multi{background:#2e9cd5}.rfg-home__hint{color:#6e8067;font-size:13px}.rfg-home__records{margin-top:16px;border-top:2px solid #d9e9ce;padding-top:12px}.rfg-home__records-head,.rfg-home__record-top{display:flex;justify-content:space-between}.rfg-home__records-title{font-size:21px;font-weight:900}.rfg-home__empty{display:block;padding:18px;text-align:center;color:#84957b}.rfg-home__record{margin-top:10px;padding:12px;border:2px solid #c6dda9;border-radius:14px;background:#fbfef7;font-size:13px}.rfg-home__status{padding:3px 8px;border-radius:999px;background:#55ad34;color:#fff}.rfg-home__line{margin-top:5px}.rfg-home__detail{margin-top:8px;border-top:1px solid #d6e6c3;padding-top:7px;color:#637258}.rfg-home__all{margin-top:12px;border:0;border-radius:999px;background:#eaf5e1;color:#397527}
</style>
