<template>
  <view class="page-shell history-page">
    <view class="safe-top"></view>
    <view class="page-heading history-heading"><button class="history-back pressable" @tap="back">‹</button><view class="history-heading-copy"><text class="page-title">历史趋势</text><text class="page-subtitle">设备本地训练档案</text></view><view class="period-switch"><text :class="{ active: period === 'week' }" @tap="period = 'week'">周</text><text :class="{ active: period === 'month' }" @tap="period = 'month'">月</text></view></view>

    <view class="metric-grid">
      <view class="metric-card dark"><text>完成率</text><text>{{ summary.completion }}%</text><text>已完成 {{ completedCount }} / {{ records.length }} 次</text></view>
      <view class="metric-card light"><text>动作合格率</text><text>{{ summary.qualified }}%</text><text>较上周 +5%</text></view>
      <view class="metric-card warm"><text>最大角度</text><text>{{ summary.maxAngle }}°</text><text>近 7 天最佳</text></view>
    </view>

    <view class="section-head"><text class="section-title">最大角度趋势</text><text class="section-link">单位 °</text></view>
    <view class="chart-card card">
      <view class="chart-y"><text>90</text><text>60</text><text>30</text><text>0</text></view>
      <view class="chart-main">
        <view class="chart-lines"><view v-for="n in 4" :key="n"></view></view>
        <view class="bar-row">
          <view v-for="item in records.slice().reverse()" :key="item.date" class="bar-cell">
            <view class="bar-space"><view class="bar" :class="{ missed: !item.completed, best: item.maxAngle === summary.maxAngle }" :style="{ height: Math.max(8, item.maxAngle / 90 * 100) + '%' }"><text v-if="item.maxAngle === summary.maxAngle">{{ item.maxAngle }}</text></view></view>
            <text>{{ item.date.slice(3) }}</text>
          </view>
        </view>
      </view>
    </view>

    <view class="section-head"><text class="section-title">每日执行情况</text><text class="section-link">近 7 天</text></view>
    <view class="day-list card">
      <view v-for="item in records" :key="item.date" class="day-row">
        <view class="day-status" :class="{ missed: !item.completed }">{{ item.completed ? '✓' : '!' }}</view>
        <view class="day-copy"><text>{{ item.weekday }} · {{ item.date }}</text><text v-if="item.completed">{{ item.minutes }} 分钟 · 合格率 {{ item.qualified }}% · 中断 {{ item.interrupted }} 次</text><text v-else>未检测到训练记录，记为漏练</text></view>
        <view class="day-progress" :class="{ zero: !item.completed }"><text>{{ item.completion }}%</text><view><view :style="{ width: item.completion + '%' }"></view></view></view>
      </view>
    </view>

    <view class="insight card"><view class="insight-mark">✦</view><view><text>趋势解读</text><text>最大角度逐步提升，动作合格率保持稳定。本周有 {{ missedCount }} 次漏练、{{ interruptedCount }} 次训练中断，建议家属在晚间提醒。</text></view></view>
  </view>
</template>

<script>
import { getHistory } from '../../services/device.js'

export default {
  data() { return { period: 'week', records: getHistory() } },
  computed: {
    completedCount() { return this.records.filter(item => item.completed).length },
    missedCount() { return this.records.filter(item => !item.completed).length },
    interruptedCount() { return this.records.reduce((sum, item) => sum + item.interrupted, 0) },
    summary() {
      const done = this.records.filter(item => item.completed)
      return { completion: Math.round(this.records.reduce((sum, item) => sum + item.completion, 0) / this.records.length), qualified: Math.round(done.reduce((sum, item) => sum + item.qualified, 0) / done.length), maxAngle: Math.max(...this.records.map(item => item.maxAngle)) }
    }
  },
  methods: { back() { uni.navigateBack() } },
  onShow() { this.records = getHistory() }
}
</script>

<style scoped>
.history-page { padding-bottom: 72rpx; }.period-switch { padding: 6rpx; border-radius: 18rpx; background: #e5ebe8; display: flex; }.period-switch text { width: 61rpx; height: 47rpx; border-radius: 13rpx; display: flex; align-items: center; justify-content: center; color: #8b9792; font-size: 26rpx; }.period-switch text.active { background: #fff; color: #24594c; font-weight: 700; box-shadow: 0 4rpx 10rpx rgba(34,72,62,.07); }.metric-grid { display: grid; grid-template-columns: 1.25fr 1fr; gap: 14rpx; }.metric-card { min-height: 181rpx; padding: 25rpx; border-radius: 29rpx; }.metric-card text { display: block; font-size: 26rpx; }.metric-card text:nth-child(2) { margin-top: 14rpx; font-size: 48rpx; font-weight: 700; }.metric-card text:last-child { margin-top: 9rpx; opacity: .7; font-size: 22rpx; }.metric-card.dark { grid-row: span 2; background: #174f42; color: #fff; display: flex; flex-direction: column; justify-content: center; }.metric-card.dark text:nth-child(2) { font-size: 64rpx; color: #d9ee7f; }.metric-card.light { background: #e4eee7; color: #2a5a4d; }.metric-card.warm { background: #f4e7d9; color: #8e5e48; }
.chart-card { height: 350rpx; padding: 29rpx 24rpx 21rpx; display: flex; }.chart-y { width: 45rpx; height: 260rpx; padding-bottom: 24rpx; display: flex; flex-direction: column; justify-content: space-between; color: #a1aaa7; font-size: 22rpx; }.chart-main { position: relative; flex: 1; height: 100%; }.chart-lines { position: absolute; inset: 0 0 36rpx; display: flex; flex-direction: column; justify-content: space-between; }.chart-lines view { border-top: 1rpx dashed #e3e9e6; }.bar-row { position: absolute; inset: 0; display: flex; justify-content: space-around; }.bar-cell { flex: 1; display: flex; flex-direction: column; align-items: center; justify-content: flex-end; gap: 11rpx; color: #9aa4a0; font-size: 22rpx; }.bar-space { width: 100%; height: 260rpx; display: flex; align-items: flex-end; justify-content: center; }.bar { position: relative; width: 27rpx; border-radius: 9rpx 9rpx 3rpx 3rpx; background: #a9c0b5; }.bar.best { background: #729753; }.bar.missed { height: 8rpx !important; background: #d7a18e; }.bar text { position: absolute; top: -29rpx; left: 50%; transform: translateX(-50%); color: #587541; font-size: 22rpx; font-weight: 700; }
.day-list { padding: 0 25rpx; }.day-row { min-height: 121rpx; border-bottom: 1rpx solid #edf0ee; display: flex; align-items: center; }.day-row:last-child { border-bottom: 0; }.day-status { flex: none; width: 45rpx; height: 45rpx; border-radius: 15rpx; background: #e1efe4; color: #5e8765; display: flex; align-items: center; justify-content: center; font-size: 28rpx; font-weight: 700; }.day-status.missed { background: #f7e5de; color: #af6351; }.day-copy { min-width: 0; flex: 1; margin-left: 17rpx; }.day-copy text { display: block; color: #294b42; font-size: 28rpx; font-weight: 600; }.day-copy text:last-child { margin-top: 7rpx; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; color: #929d99; font-size: 22rpx; font-weight: 400; }.day-progress { width: 88rpx; text-align: right; }.day-progress>text { color: #3f6a5d; font-size: 26rpx; font-weight: 700; }.day-progress>view { height: 5rpx; margin-top: 9rpx; overflow: hidden; border-radius: 5rpx; background: #e3e9e6; }.day-progress>view view { height: 100%; background: #789a65; }.day-progress.zero>text { color: #b36c59; }.insight { margin-top: 23rpx; padding: 25rpx; display: flex; gap: 17rpx; background: #edf3dc; box-shadow: none; }.insight-mark { flex: none; width: 49rpx; height: 49rpx; border-radius: 16rpx; background: #d9e8ae; color: #607d38; display: flex; align-items: center; justify-content: center; }.insight text { display: block; color: #3b5b52; font-size: 28rpx; font-weight: 700; }.insight text:last-child { margin-top: 8rpx; color: #718078; font-size: 24rpx; line-height: 1.6; font-weight: 400; }

/* RehabMotion type system v2 */
.period-switch text { font-weight: 500; }
.metric-card text:nth-child(2) {
  font-size: 48rpx;
  font-weight: 700;
  line-height: 1.05;
  letter-spacing: -1rpx;
  font-variant-numeric: tabular-nums;
}
.metric-card.dark text:nth-child(2) { font-size: 64rpx; }
.bar text, .day-progress > text { font-weight: 600; font-variant-numeric: tabular-nums; }
.day-copy text, .insight text { font-weight: 600; }
.day-row { min-height: 108rpx; }


/* Secondary-page navigation v7 */
.history-heading { gap: 16rpx; }
.history-back {
  flex: none;
  width: 64rpx;
  height: 64rpx;
  padding: 0;
  border-radius: 20rpx;
  background: #fff;
  color: #174f42;
  font-size: 48rpx;
  font-weight: 500;
  line-height: 60rpx;
  box-shadow: 0 8rpx 22rpx rgba(27, 70, 59, .06);
}
.history-heading-copy { flex: 1; min-width: 0; }

</style>
