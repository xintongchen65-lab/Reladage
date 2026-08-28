<template>
  <view class="report-page">
    <app-page-header title="康复报告" :subtitle="memberLabel">
      <template #action><view class="professional-link pressable" @tap="openProfessional"><text>专业训练数据</text><text class="chevron">›</text></view></template>
    </app-page-header>

    <view class="page-content">
      <view class="week-card pressable" @tap="openWeek">
        <view class="card-title-row"><text class="week-title">本周训练</text><view class="card-link" @tap.stop="openWeek"><text>查看完整周报</text><text class="chevron">›</text></view></view>
        <view class="week-metrics">
          <view class="week-metric week-primary">
            <view class="number-line">
              <text class="week-number">{{ dashboard.week.completedSessions }}</text>
              <text class="week-unit"> / {{ dashboard.week.plannedSessions }}次</text>
            </view>
            <text class="week-label">计划完成</text>
          </view>
          <view class="week-metric week-secondary">
            <view class="number-line">
              <text class="week-number">{{ dashboard.week.qualified }}</text>
              <text class="week-unit">%</text>
            </view>
            <text class="week-label">动作达标率</text>
          </view>
        </view>
        <view class="week-summary">
          <text class="summary-title">{{ dashboard.week.summary }}</text>
          <text class="summary-problem">主要问题：{{ dashboard.week.mainProblem }}</text>
        </view>
      </view>

      <view class="open-section month-section">
        <view class="section-title-row"><text class="section-title">{{ dashboard.month.label }}</text><view class="section-link pressable" @tap="openMonth"><text>查看完整月报</text><text class="chevron">›</text></view></view>
        <view class="month-primary">
          <view class="number-line dark-number-line">
            <text class="month-main-number">{{ dashboard.month.completedSessions }}</text>
            <text class="month-main-unit"> / {{ dashboard.month.plannedSessions }}次</text>
          </view>
          <text class="open-label">计划完成</text>
        </view>
        <view class="month-rule"></view>
        <view class="month-metrics">
          <view class="month-metric">
            <view class="number-line dark-number-line"><text class="month-number">{{ dashboard.month.qualified }}</text><text class="month-unit">%</text></view>
            <text class="open-label">动作达标率</text>
          </view>
          <view class="month-divider"></view>
          <view class="month-metric">
            <view class="number-line dark-number-line"><text class="month-number">{{ dashboard.month.totalMinutes }}</text><text class="month-unit"> min</text></view>
            <text class="open-label">累计训练</text>
          </view>
          <view class="month-divider"></view>
          <view class="month-metric month-metric-short">
            <view class="number-line dark-number-line"><text class="month-number">{{ dashboard.month.uncompleted }}</text><text class="month-unit">次</text></view>
            <text class="open-label">未完成</text>
          </view>
        </view>
      </view>

      <view class="open-section trend-section">
        <view class="section-title-row"><text class="section-title">长期训练趋势</text><view class="section-link pressable" @tap="openTrend"><text>查看完整趋势</text><text class="chevron">›</text></view></view>
        <view class="trend-overview">
          <view class="trend-current">
            <view class="number-line dark-number-line"><text class="trend-current-number">{{ dashboard.trend.current }}</text><text class="trend-current-unit">%</text></view>
            <text class="trend-name">近4周动作达标率</text>
          </view>
          <text class="trend-change-chip" :class="{ negative: dashboard.trend.change < 0 }">{{ trendChangeText }}</text>
        </view>
        <report-line-chart canvas-id="reportHomeTrend" :values="dashboard.trend.values" :labels="dashboard.trend.labels" suffix="%" :min-value="trendChartMin" :max-value="trendChartMax" compact />
        <view class="trend-interpretation"><view></view><text>{{ dashboard.trend.interpretation }}</text></view>
      </view>

      <view class="xiaosi-entry pressable" @tap="openXiaosi">
        <text class="xiaosi-mark">✦</text><text class="xiaosi-copy">让小思帮我解读报告</text><text class="xiaosi-arrow">›</text>
      </view>
      <text class="report-note">报告反映居家训练执行情况，不替代医生诊断或康复处方。</text>
    </view>
  </view>
</template>

<script>
import AppPageHeader from '../../components/app-page-header/app-page-header.vue'
import ReportLineChart from '../../components/report-line-chart/report-line-chart.vue'
import { getReportHomeData } from '../../services/report-dashboard.js'
import { getCurrentMemberLabel } from '../../services/members.js'

export default {
  components: { AppPageHeader, ReportLineChart },
  data() { return { dashboard: getReportHomeData(), memberLabel: getCurrentMemberLabel() } },
  computed: {
    trendChangeText() {
      const change = Number(this.dashboard && this.dashboard.trend ? this.dashboard.trend.change : 0)
      if (change === 0) return '与第1周持平'
      return `较第1周${change > 0 ? '+' : ''}${change}个百分点`
    },
    trendChartMin() {
      const values = this.dashboard && this.dashboard.trend ? this.dashboard.trend.values.map(Number).filter(Number.isFinite) : []
      if (!values.length) return 0
      return Math.max(0, Math.floor((Math.min(...values) - 5) / 5) * 5)
    },
    trendChartMax() {
      const values = this.dashboard && this.dashboard.trend ? this.dashboard.trend.values.map(Number).filter(Number.isFinite) : []
      if (!values.length) return 100
      const lower = this.trendChartMin
      return Math.min(100, Math.max(lower + 10, Math.ceil((Math.max(...values) + 5) / 5) * 5))
    }
  },
  onShow() {
    this.dashboard = getReportHomeData()
    this.memberLabel = getCurrentMemberLabel()
  },
  methods: {
    openWeek() { uni.navigateTo({ url: '/pages/report/week' }) },
    openMonth() { uni.navigateTo({ url: '/pages/report/month' }) },
    openTrend() { uni.navigateTo({ url: '/pages/report/trend' }) },
    openProfessional() { uni.navigateTo({ url: '/pages/report/professional' }) },
    openXiaosi() { uni.navigateTo({ url: '/pages/agent/index?source=report' }) }
  }
}
</script>

<style scoped>
.report-page{min-height:100vh;box-sizing:border-box;background:#f5f6f3;color:#16201d;padding-bottom:calc(48rpx + env(safe-area-inset-bottom))}
.professional-link,.card-link,.section-link{display:flex;align-items:center;justify-content:flex-end;gap:8rpx;font-size:28rpx;font-weight:500}
.professional-link{flex:none;height:48rpx;padding:0 16rpx;border-radius:999rpx;background:#d9ee7f;color:#174f42;font-weight:600}
.professional-link .chevron{font-size:32rpx}
.chevron{font-size:36rpx;line-height:1;font-weight:400}
.page-content{padding:10rpx 30rpx 0}
.card-title-row,.section-title-row{display:flex;align-items:center;justify-content:space-between;gap:20rpx}
.number-line{display:flex;align-items:baseline;white-space:nowrap;font-variant-numeric:tabular-nums}

.week-card{box-sizing:border-box;padding:26rpx 27rpx 24rpx;border-radius:29rpx;background:#174f42;color:#f8fbf9;box-shadow:0 11rpx 27rpx rgba(23,79,66,.10)}
.week-title{display:block;color:#fff;font-size:32rpx;line-height:1.4;font-weight:600}
.card-link{flex:none;color:#c9ee66;font-size:26rpx}
.week-metrics{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));align-items:stretch;margin-top:26rpx}
.week-metric{min-width:0;display:flex;flex-direction:column;align-items:flex-start}
.week-primary{padding-right:32rpx}
.week-secondary{padding-left:32rpx;border-left:1rpx solid rgba(255,255,255,.16)}
.week-number{color:#d9ee7f;font-size:58rpx;line-height:1.05;letter-spacing:-1rpx;font-weight:700}
.week-unit{margin-left:4rpx;color:#d9ee7f;font-size:28rpx;line-height:1;font-weight:500}
.week-label{margin-top:10rpx;color:#a7c2ba;font-size:28rpx;line-height:1.35;font-weight:400}
.week-summary{display:flex;flex-direction:column;gap:7rpx;margin-top:24rpx;padding-top:18rpx;border-top:1rpx solid rgba(232,248,240,.16)}
.summary-title,.summary-problem{font-size:28rpx;line-height:1.5;font-weight:400}
.summary-title{color:#f6faf8}
.summary-problem{color:#a7c2ba}

.open-section{box-sizing:border-box;margin-top:18rpx;padding:26rpx 24rpx 22rpx;border:1rpx solid #e5ebe8;border-radius:28rpx;background:#fff}
.section-title{display:block;color:#17231f;font-size:32rpx;line-height:1.35;font-weight:600}
.section-title-row .section-link{flex:none;color:#17644f;font-size:26rpx}
.dark-number-line{color:#153f34}
.month-primary{margin-top:24rpx}
.month-main-number{font-size:54rpx;line-height:1.05;letter-spacing:-1rpx;font-weight:700}
.month-main-unit{margin-left:5rpx;font-size:28rpx;line-height:1;font-weight:500}
.open-label{display:block;margin-top:9rpx;color:#7b8581;font-size:28rpx;line-height:1.35;font-weight:400}
.month-rule{height:1rpx;margin:23rpx 0;background:#edf0ee}
.month-metrics{display:grid;grid-template-columns:minmax(0,1fr) 1rpx minmax(0,1fr) 1rpx minmax(0,.82fr);align-items:stretch;column-gap:22rpx}
.month-metric{min-width:0;display:flex;flex-direction:column;align-items:flex-start}
.month-divider{width:1rpx;min-height:78rpx;background:#e3e8e5}
.month-number{font-size:46rpx;line-height:1.05;letter-spacing:-1rpx;font-weight:700}
.month-unit{margin-left:3rpx;font-size:26rpx;line-height:1;font-weight:500}

.trend-section{position:relative;z-index:0;overflow:hidden}
.trend-overview{display:flex;align-items:flex-end;justify-content:space-between;gap:24rpx;margin-top:23rpx}
.trend-current{min-width:0}
.trend-current-number{color:#153f34;font-size:52rpx;line-height:1.05;letter-spacing:-1rpx;font-weight:700}
.trend-current-unit{margin-left:3rpx;color:#153f34;font-size:28rpx;line-height:1;font-weight:500}
.trend-name{display:block;margin-top:8rpx;color:#75817c;font-size:28rpx;line-height:1.35;font-weight:400}
.trend-change-chip{flex:none;margin-bottom:4rpx;padding:8rpx 13rpx;border-radius:999rpx;background:#e6f1e9;color:#39715d;font-size:24rpx;line-height:1.25;font-weight:500;font-variant-numeric:tabular-nums}
.trend-change-chip.negative{background:#f6e9e2;color:#a05e49}
.trend-interpretation{display:flex;align-items:flex-start;gap:13rpx;margin-top:8rpx;padding-top:18rpx;border-top:1rpx solid #edf0ee}
.trend-interpretation view{flex:none;width:5rpx;height:31rpx;margin-top:5rpx;border-radius:5rpx;background:#83aa88}
.trend-interpretation text{flex:1;color:#5f6f69;font-size:28rpx;line-height:1.5;font-weight:400}

.xiaosi-entry{position:relative;z-index:2;display:flex;align-items:center;min-height:104rpx;box-sizing:border-box;margin-top:18rpx;padding:22rpx 26rpx;border:1rpx solid #cde3da;border-radius:24rpx;background:#eaf5ef}
.xiaosi-mark{width:52rpx;color:#0e7257;font-size:42rpx;line-height:1;font-weight:600}
.xiaosi-copy{flex:1;color:#164c3d;font-size:30rpx;line-height:1.4;font-weight:600}
.xiaosi-arrow{color:#17644f;font-size:40rpx;line-height:1;font-weight:400}
.report-note{display:block;margin:22rpx 10rpx 0;color:#8b9692;font-size:24rpx;line-height:1.5;font-weight:400;text-align:center}
</style>
