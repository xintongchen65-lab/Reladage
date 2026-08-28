<template>
  <view class="page-shell detail-page">
    <secondary-page-header title="月度报告" :subtitle="memberLabel" @back="back"><template #action><button class="export-text header-chip pressable" @tap="exportPdf">导出</button></template></secondary-page-header>
    <view class="period-switch"><button class="period-arrow" @tap="changeMonth(-1)">‹</button><view><text>{{ report.period }}</text><text>{{ report.cutoff }}</text></view><button class="period-arrow" :disabled="!report.canNext" @tap="changeMonth(1)">›</button></view>

    <view class="month-hero">
      <view><view><text>{{ report.completedSessions }}</text><text>/{{ report.plannedSessions }}次</text></view><text>计划完成</text></view>
      <view><view><text>{{ report.qualified }}</text><text>%</text></view><text>动作达标率</text></view>
      <view><view><text>{{ report.totalMinutes }}</text><text>min</text></view><text>累计训练</text></view>
    </view>

    <view class="section-head"><text class="section-title">本月训练打卡</text><text class="section-link">{{ report.uncompleted }} 次未完成</text></view>
    <view class="calendar-card card"><month-execution-calendar :days="report.calendar" @select="openDay" /><text class="chart-interpretation">{{ report.interpretation }}</text></view>

    <view class="section-head"><text class="section-title">动作达标率趋势</text><text class="section-link">{{ report.qualified }}%</text></view>
    <view class="chart-card card"><report-line-chart canvas-id="monthQualifiedChart" :values="report.weeklyTrend.values" :labels="report.weeklyTrend.labels" suffix="%" :min-value="0" :max-value="100" /><text class="chart-interpretation">{{ trendInterpretation }}</text></view>

    <view class="section-head"><text class="section-title">主要动作问题 TOP 3</text><text class="section-link">有效训练记录</text></view>
    <view class="problem-card card"><view v-for="item in report.problems" :key="item.label" class="problem-row"><text>{{ item.label }}</text><view><view :style="{ width: item.percent + '%' }"></view></view><text>{{ item.count }} 次</text></view></view>

    <view class="section-head"><text class="section-title">训练后反馈</text></view>
    <view class="feedback-card card"><text class="label">训练强度</text><view class="dominant"><text>多数为</text><text>{{ report.feedback.dominantIntensity }}</text></view><view class="feedback-divider"></view><text class="label">疼痛反馈</text><view class="feedback-list"><view v-for="item in report.feedback.pain" :key="item.label"><text>{{ item.label }}</text><text>{{ item.count }} 次</text></view></view><view class="feedback-change"><view></view><text>{{ report.feedback.comparison }}</text></view><text class="feedback-note">只陈述记录变化，不自动推断病情变化。</text></view>
    <view class="detail-disclaimer">月报用于周期回顾，不替代医生诊断或功能评估。</view>
  </view>
</template>

<script>
import MonthExecutionCalendar from '../../components/month-execution-calendar/month-execution-calendar.vue'
import ReportLineChart from '../../components/report-line-chart/report-line-chart.vue'
import { getMonthlyDetailData } from '../../services/report-dashboard.js'
import { exportReport } from '../../utils/exporter.js'
import SecondaryPageHeader from '../../components/secondary-page-header/secondary-page-header.vue'
import { getCurrentMemberLabel } from '../../services/members.js'
export default {
  components: { MonthExecutionCalendar, ReportLineChart, SecondaryPageHeader },
  data() { return { monthOffset: 0, report: getMonthlyDetailData(0), memberLabel: getCurrentMemberLabel() } },
  computed: {
    trendInterpretation() {
      const values = this.report.weeklyTrend.values || []
      if (values.length < 2) return '当前有效周期不足，暂不比较变化。'
      const first = values[0]; const last = values[values.length - 1]; const delta = last - first
      return `近 ${values.length} 个周期动作达标率由 ${first}% 变为 ${last}%，${delta > 0 ? '上升' : delta < 0 ? '下降' : '持平'}${delta ? ` ${Math.abs(delta)} 个百分点` : ''}。`
    }
  },
  methods: {
    back() { uni.navigateBack() },
    changeMonth(direction) { if (direction > 0 && !this.report.canNext) return; this.monthOffset += direction; this.report = getMonthlyDetailData(this.monthOffset) },
    openDay(item) { uni.navigateTo({ url: `/pages/daily/index?date=${item.isoDate}` }) },
    exportPdf() { exportReport('pdf', { ...this.report, completion: this.report.adherence, maxAngle: 0 }, this.report.sessions, '').catch(error => uni.showToast({ title: error.message || '导出失败', icon: 'none' })) }
  }
}
</script>

<style scoped>
.detail-page{padding-bottom:50rpx}.detail-nav{height:86rpx;display:flex;align-items:center}.detail-nav>view{flex:1;margin-left:15rpx}.detail-nav>view text{display:block;color:#1c4037;font-size:32rpx;font-weight:600}.detail-nav>view text:last-child{margin-top:3rpx;color:#8a9792;font-size:24rpx;font-weight:400}.period-switch{margin-top:8rpx;padding:16rpx 12rpx;border:1rpx solid rgba(19,71,57,.06);border-radius:22rpx;background:#fff;display:flex;align-items:center;justify-content:space-between}.period-switch>view{text-align:center}.period-switch>view text{display:block;color:#294d43;font-size:28rpx;font-weight:600}.period-switch>view text:last-child{margin-top:4rpx;color:#8b9792;font-size:24rpx;font-weight:400}.period-arrow{width:58rpx;height:58rpx;color:#315f53;display:flex;align-items:center;justify-content:center;font-size:42rpx}.period-arrow[disabled]{opacity:.2}
.month-hero{margin-top:18rpx;padding:27rpx 24rpx;border-radius:28rpx;background:#174f42;display:grid;grid-template-columns:repeat(3,1fr)}.month-hero>view{padding-left:20rpx;border-left:1rpx solid rgba(255,255,255,.14)}.month-hero>view:first-child{padding-left:0;border:0}.month-hero>view>view{display:flex;align-items:baseline}.month-hero>view>view text:first-child{color:#d9ee7f;font-size:44rpx;line-height:1.05;font-weight:700;letter-spacing:-1rpx}.month-hero>view>view text:last-child{margin-left:4rpx;color:#d9ee7f;font-size:24rpx;font-weight:500}.month-hero>view>text{display:block;margin-top:9rpx;color:#a8c1b9;font-size:24rpx;font-weight:400}
.calendar-card,.chart-card,.problem-card,.feedback-card{box-shadow:none}.calendar-card{padding:15rpx 22rpx 22rpx}.chart-card{padding:14rpx 16rpx 20rpx}.chart-interpretation{display:block;margin-top:10rpx;padding:18rpx 4rpx 0;border-top:1rpx solid #e8ecea;color:#5d7069;font-size:28rpx;line-height:1.5;font-weight:400}.problem-card{padding:8rpx 22rpx}.problem-row{height:82rpx;display:grid;grid-template-columns:190rpx 1fr 72rpx;align-items:center;gap:14rpx}.problem-row>text{color:#405c54;font-size:28rpx}.problem-row>text:last-child{text-align:right;color:#8a6a48;font-size:24rpx}.problem-row>view{height:9rpx;border-radius:9rpx;background:#edf0ee;overflow:hidden}.problem-row>view view{height:100%;border-radius:9rpx;background:#e4a146}
.feedback-card{padding:24rpx}.label{display:block;color:#7d8b86;font-size:28rpx}.dominant{margin-top:9rpx;display:flex;align-items:baseline}.dominant text{font-size:28rpx;color:#314f47}.dominant text:last-child{margin-left:8rpx;color:#173f35;font-size:34rpx;font-weight:700}.feedback-divider{height:1rpx;margin:22rpx 0;background:#e6ebe8}.feedback-list{margin-top:10rpx}.feedback-list>view{min-height:52rpx;display:flex;justify-content:space-between}.feedback-list text{color:#435e56;font-size:28rpx}.feedback-change{margin-top:18rpx;display:flex;gap:15rpx}.feedback-change view{width:6rpx;height:44rpx;border-radius:6rpx;background:#d3dad6}.feedback-change text{flex:1;color:#415b53;font-size:28rpx;line-height:1.5}.feedback-note{display:block;margin-top:17rpx;color:#929d99;font-size:24rpx}.detail-disclaimer{margin-top:22rpx;color:#8d9894;font-size:24rpx;text-align:center}
</style>
