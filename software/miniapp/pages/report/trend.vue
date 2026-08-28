<template>
  <view class="page-shell trend-page">
    <secondary-page-header title="长期训练趋势" :subtitle="`已记录 ${metric.recordedWeeks} 个有效周期`" @back="back" />

    <view class="filter-row"><picker :range="metricNames" :value="metricIndex" @change="changeMetric"><view class="filter-pill"><text>{{ metric.name }}</text><view class="down-chevron"></view></view></picker><picker :range="ranges" :value="rangeIndex" @change="changeRange"><view class="filter-pill secondary"><text>{{ ranges[rangeIndex] }}</text><view class="down-chevron"></view></view></picker></view>

    <view class="trend-hero card">
      <view class="metric-head"><view><text>{{ metric.name }}</text><view class="trend-value"><text>{{ metric.current }}</text><text>{{ metric.unit }}</text></view></view><text class="change-chip" :class="{ good: isPositiveChange }">{{ changeText }}</text></view>
      <report-line-chart canvas-id="mainTrendChart" :values="metric.values" :labels="metric.labels" :suffix="metric.unit" :min-value="chartMin" :max-value="chartMax" />
      <text class="chart-interpretation">{{ metric.interpretation }}</text>
    </view>

    <view class="data-range-note"><text>按实际记录展示</text><text>选择更长时间范围时，仅展示已有训练数据，不补造空白记录。</text></view>
    <view class="section-head"><text class="section-title">指标说明</text></view><view class="explain-card card"><text>{{ explanation.title }}</text><text>{{ explanation.desc }}</text></view>
    <button class="professional-entry pressable" @tap="openProfessional"><view><text>查看专业训练数据</text><text>角度、ROM、处方剂量和动作问题构成</text></view><view class="right-chevron"></view></button>
  </view>
</template>

<script>
import ReportLineChart from '../../components/report-line-chart/report-line-chart.vue'
import { trendMetricOptions, trendRangeOptions, getTrendMetric } from '../../services/report-dashboard.js'
import SecondaryPageHeader from '../../components/secondary-page-header/secondary-page-header.vue'
export default {
  components: { ReportLineChart, SecondaryPageHeader },
  data() { return { metrics: trendMetricOptions, ranges: trendRangeOptions, metricIndex: 0, rangeIndex: 0, metric: getTrendMetric('qualified', '近4周') } },
  computed: {
    metricNames() { return this.metrics.map(item => item.name) },
    chartMin() { return this.metric.unit === '%' || this.metric.unit === '/10' ? 0 : 0 },
    chartMax() { if (this.metric.unit === '%') return 100; if (this.metric.unit === '/10') return 10; return Math.max(5, Math.ceil(Math.max(...this.metric.values, 0) / 5) * 5) },
    isPositiveChange() { return this.metric.id === 'problems' || this.metric.id === 'pain' ? this.metric.change < 0 : this.metric.change > 0 },
    changeText() { const prefix = this.metric.change > 0 ? '+' : ''; return `较首个周期 ${prefix}${this.metric.change}${this.metric.unit}` },
    explanation() {
      const map = {
        qualified: { title: '动作达标率', desc: '达到角度、节奏和动作完整性要求的动作，占已完成动作的比例。' },
        adherence: { title: '训练计划完成率', desc: '按计划完整完成的训练次数，占同期已到期计划训练次数的比例。' },
        problems: { title: '动作问题发生次数', desc: '设备记录到的幅度不足、节奏过快或双侧不对称等问题次数。' },
        pain: { title: '训练后疼痛反馈', desc: '老人每次训练后主动填写的疼痛评分，只反映主观记录变化。' }
      }
      return map[this.metric.id]
    }
  },
  methods: {
    back() { uni.navigateBack() },
    refreshMetric() { this.metric = getTrendMetric(this.metrics[this.metricIndex].id, this.ranges[this.rangeIndex]) },
    changeMetric(event) { this.metricIndex = Number(event.detail.value); this.refreshMetric() },
    changeRange(event) { this.rangeIndex = Number(event.detail.value); this.refreshMetric() },
    openProfessional() { uni.navigateTo({ url: '/pages/report/professional' }) }
  }
}
</script>

<style scoped>
.trend-page{padding-bottom:50rpx}.detail-nav{height:86rpx;display:flex;align-items:center}.detail-nav>view:nth-child(2){flex:1;margin-left:15rpx}.detail-nav>view:nth-child(2) text{display:block;color:#1c4037;font-size:32rpx;font-weight:600}.detail-nav>view:nth-child(2) text:last-child{margin-top:3rpx;color:#8a9792;font-size:24rpx;font-weight:400}.nav-space{width:68rpx}.filter-row{margin-top:13rpx;display:flex;justify-content:space-between;gap:12rpx}.filter-row picker{flex:1}.filter-pill{height:66rpx;padding:0 17rpx;border-radius:19rpx;background:#174f42;color:#fff;display:flex;align-items:center;justify-content:space-between}.filter-pill.secondary{background:#e3ebe7;color:#315c50}.filter-pill text{font-size:28rpx;font-weight:600}.down-chevron{width:11rpx;height:11rpx;margin-top:-6rpx;border-right:3rpx solid currentColor;border-bottom:3rpx solid currentColor;transform:rotate(45deg)}
.trend-hero{margin-top:16rpx;padding:25rpx 22rpx 22rpx;box-shadow:none}.metric-head{display:flex;align-items:flex-start;justify-content:space-between;gap:16rpx}.metric-head>view>text{display:block;color:#61766e;font-size:28rpx;font-weight:600}.trend-value{margin-top:9rpx;display:flex;align-items:baseline}.trend-value text:first-child{color:#174f42;font-size:60rpx;line-height:1;font-weight:700}.trend-value text:last-child{margin-left:4rpx;color:#3f6258;font-size:28rpx;font-weight:500}.change-chip{margin-top:6rpx;padding:7rpx 11rpx;border-radius:999rpx;background:#f2e9df;color:#9a684b;font-size:24rpx;font-weight:500}.change-chip.good{background:#e4efe5;color:#54765a}.chart-interpretation{display:block;margin-top:8rpx;padding:20rpx 4rpx 0;border-top:1rpx solid #e6ebe8;color:#566d65;font-size:28rpx;line-height:1.5;font-weight:400}
.data-range-note{margin-top:16rpx;padding:17rpx;border-radius:18rpx;background:#e8eeeb}.data-range-note text{display:block;color:#566c64;font-size:28rpx;font-weight:500}.data-range-note text:last-child{margin-top:6rpx;color:#87938e;font-size:24rpx;line-height:1.45;font-weight:400}.explain-card{padding:22rpx;box-shadow:none}.explain-card text{display:block;color:#294d43;font-size:28rpx;font-weight:600}.explain-card text:last-child{margin-top:8rpx;color:#6c7e77;line-height:1.55;font-weight:400}.professional-entry{width:100%;min-height:94rpx;margin-top:24rpx;padding:17rpx 20rpx;border-radius:23rpx;background:#174f42;color:#fff;display:flex;align-items:center}.professional-entry>view:first-child{flex:1;text-align:left}.professional-entry text{display:block;font-size:28rpx;font-weight:600}.professional-entry text:last-child{margin-top:6rpx;color:#a8c1b9;font-size:24rpx;font-weight:400}.right-chevron{width:13rpx;height:13rpx;border-right:3rpx solid #d9ee7f;border-top:3rpx solid #d9ee7f;transform:rotate(45deg)}
</style>
