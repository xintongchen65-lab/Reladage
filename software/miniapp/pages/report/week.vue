<template>
  <view class="page-shell detail-page">
    <secondary-page-header title="完整周报" :subtitle="memberLabel" @back="back"><template #action><button class="export-text header-chip pressable" @tap="exportPdf">导出</button></template></secondary-page-header>

    <view class="period-switch">
      <button class="period-arrow pressable" :disabled="!report.canPrevious" @tap="changeWeek(-1)">‹</button>
      <view><text>{{ report.period }}</text><text>{{ report.isCurrent ? '本周 · 进行中' : '历史周报' }} · 按当周方案统计</text></view>
      <button class="period-arrow pressable" :disabled="!report.canNext" @tap="changeWeek(1)">›</button>
    </view>

    <view class="week-summary-card">
      <view><view><text>{{ report.completedSessions }}</text><text>/{{ report.plannedSessions }}次</text></view><text>完成训练</text></view>
      <view><view><text>{{ report.qualified }}</text><text>%</text></view><text>动作达标率</text></view>
      <view><view><text>{{ report.totalMinutes }}</text><text>min</text></view><text>累计训练</text></view>
    </view>

    <view class="section-head"><text class="section-title">本周结论</text></view>
    <view class="plain-card card conclusion-card"><text>{{ report.summary }}</text><text>{{ report.interpretation }}</text><view class="conclusion-note"><view></view><text>主要动作问题：{{ report.mainProblem }}</text></view></view>

    <view class="section-head"><text class="section-title">训练完成情况</text><text class="section-link">{{ report.incompleteBreakdown }}</text></view>
    <view class="plain-card card execution-card">
      <view v-for="item in plannedSessions" :key="item.isoDate" class="execution-row">
        <view class="execution-copy"><text>{{ formatDate(item.isoDate) }} · {{ item.exercise }}</text><text>{{ sessionMeta(item) }}</text></view>
        <view class="state-chip" :class="item.status">{{ item.statusLabel }}</view>
      </view>
    </view>

    <view class="section-head"><text class="section-title">主要动作问题 TOP 3</text><text class="section-link">有效训练记录</text></view>
    <view class="plain-card card problem-card">
      <view v-for="item in report.problems" :key="item.label" class="problem-row"><text>{{ item.label }}</text><view><view :style="{ width: item.percent + '%' }"></view></view><text>{{ item.count }} 次</text></view>
    </view>

    <view class="section-head"><text class="section-title">训练后反馈</text></view>
    <view class="plain-card card feedback-card">
      <text class="feedback-label">训练强度</text><view class="feedback-main"><text>多数为</text><text>{{ report.feedback.dominantIntensity }}</text></view>
      <view class="feedback-divider"></view><text class="feedback-label">疼痛反馈</text>
      <view class="feedback-list"><view v-for="item in report.feedback.pain" :key="item.label"><text>{{ item.label }}</text><text>{{ item.count }} 次</text></view></view>
      <view class="feedback-change"><view></view><text>{{ report.feedback.comparison }}</text></view>
      <text class="feedback-note">只陈述记录变化，不自动推断病情变化。</text>
    </view>
    <view class="detail-disclaimer">本报告只反映居家训练执行情况，不替代医生诊断或处方。</view>
  </view>
</template>

<script>
import { getWeeklyDetailData } from '../../services/report-dashboard.js'
import { exportReport } from '../../utils/exporter.js'
import SecondaryPageHeader from '../../components/secondary-page-header/secondary-page-header.vue'
import { getCurrentMemberLabel } from '../../services/members.js'
export default {
  components: { SecondaryPageHeader },
  data() { return { weekOffset: 0, report: getWeeklyDetailData(0), memberLabel: getCurrentMemberLabel() } },
  computed: { plannedSessions() { return this.report.sessions.filter(item => item.planned && item.status !== 'scheduled' && item.status !== 'cancelled') } },
  methods: {
    back() { uni.navigateBack() },
    changeWeek(direction) {
      if (direction < 0 && !this.report.canPrevious || direction > 0 && !this.report.canNext) return
      this.weekOffset += direction
      this.report = getWeeklyDetailData(this.weekOffset)
    },
    formatDate(value) { const parts = String(value).split('-'); return `${Number(parts[1])}月${Number(parts[2])}日` },
    sessionMeta(item) { return item.status === 'completed' || item.status === 'partial' ? `${item.statusLabel} · ${item.completedReps}/${item.plannedReps} 次` : item.statusLabel },
    exportPdf() { exportReport('pdf', this.report, this.report.sessions, '').catch(error => uni.showToast({ title: error.message || '导出失败', icon: 'none' })) }
  }
}
</script>

<style scoped>
.detail-page{padding-bottom:50rpx}.detail-nav{height:86rpx;display:flex;align-items:center}.detail-nav>view{flex:1;margin-left:15rpx}.detail-nav>view text{display:block;color:#1c4037;font-size:32rpx;font-weight:600}.detail-nav>view text:last-child{margin-top:3rpx;color:#8a9792;font-size:24rpx;font-weight:400}
.period-switch{margin-top:8rpx;padding:16rpx 12rpx;border:1rpx solid rgba(19,71,57,.06);border-radius:22rpx;background:#fff;display:flex;align-items:center;justify-content:space-between}.period-switch>view{text-align:center}.period-switch>view text{display:block;color:#294d43;font-size:28rpx;font-weight:600}.period-switch>view text:last-child{margin-top:4rpx;color:#8b9792;font-size:24rpx;font-weight:400}.period-arrow{width:58rpx;height:58rpx;color:#315f53;display:flex;align-items:center;justify-content:center;font-size:42rpx;font-weight:400}.period-arrow[disabled]{opacity:.2}
.week-summary-card{margin-top:18rpx;padding:27rpx 24rpx;border-radius:28rpx;background:#174f42;display:grid;grid-template-columns:repeat(3,1fr)}.week-summary-card>view{padding-left:20rpx;border-left:1rpx solid rgba(255,255,255,.14)}.week-summary-card>view:first-child{padding-left:0;border:0}.week-summary-card>view>view{display:flex;align-items:baseline}.week-summary-card>view>view text:first-child{color:#d9ee7f;font-size:46rpx;line-height:1.05;font-weight:700;letter-spacing:-1rpx}.week-summary-card>view>view text:last-child{margin-left:4rpx;color:#d9ee7f;font-size:24rpx;font-weight:500}.week-summary-card>view>text{display:block;margin-top:9rpx;color:#a8c1b9;font-size:24rpx;font-weight:400}
.plain-card{box-shadow:none}.conclusion-card{padding:24rpx}.conclusion-card>text{display:block;color:#244b40;font-size:30rpx;font-weight:600}.conclusion-card>text:nth-child(2){margin-top:10rpx;color:#61756e;font-size:28rpx;line-height:1.55;font-weight:400}.conclusion-note{margin-top:18rpx;padding-top:18rpx;border-top:1rpx solid #e6ebe8;display:flex;align-items:center;gap:12rpx}.conclusion-note view{width:9rpx;height:9rpx;border-radius:50%;background:#d69236}.conclusion-note text{color:#705f4e;font-size:28rpx;font-weight:500}
.execution-card{padding:0 22rpx}.execution-row{min-height:98rpx;border-bottom:1rpx solid #e8edea;display:flex;align-items:center;gap:16rpx}.execution-row:last-child{border:0}.execution-copy{min-width:0;flex:1}.execution-copy text{display:block;overflow:hidden;color:#38564e;font-size:28rpx;font-weight:500;text-overflow:ellipsis;white-space:nowrap}.execution-copy text:last-child{margin-top:6rpx;color:#899590;font-size:24rpx;font-weight:400}.state-chip{flex:none;min-width:104rpx;height:46rpx;padding:0 12rpx;border-radius:999rpx;background:#e6efea;color:#356959;display:flex;align-items:center;justify-content:center;font-size:24rpx;font-weight:500}.state-chip.partial{background:#fff1dc;color:#a66a1d}.state-chip.not_started{background:#f0f2f1;color:#7f8b87}
.problem-card{padding:8rpx 22rpx}.problem-row{height:82rpx;display:grid;grid-template-columns:190rpx 1fr 72rpx;align-items:center;gap:14rpx}.problem-row>text{color:#405c54;font-size:28rpx}.problem-row>text:last-child{text-align:right;color:#8a6a48;font-size:24rpx;font-weight:500}.problem-row>view{height:9rpx;border-radius:9rpx;background:#edf0ee;overflow:hidden}.problem-row>view view{height:100%;border-radius:9rpx;background:#e4a146}
.feedback-card{padding:24rpx}.feedback-label{display:block;color:#7d8b86;font-size:28rpx}.feedback-main{margin-top:9rpx;display:flex;align-items:baseline}.feedback-main text{color:#314f47;font-size:28rpx}.feedback-main text:last-child{margin-left:8rpx;color:#173f35;font-size:34rpx;font-weight:700}.feedback-divider{height:1rpx;margin:22rpx 0;background:#e6ebe8}.feedback-list{margin-top:10rpx}.feedback-list>view{min-height:52rpx;display:flex;justify-content:space-between}.feedback-list text{color:#435e56;font-size:28rpx}.feedback-change{margin-top:18rpx;display:flex;align-items:flex-start;gap:15rpx}.feedback-change view{width:6rpx;height:44rpx;border-radius:6rpx;background:#d3dad6}.feedback-change text{flex:1;color:#415b53;font-size:28rpx;line-height:1.5}.feedback-note{display:block;margin-top:17rpx;color:#929d99;font-size:24rpx}.detail-disclaimer{margin-top:22rpx;color:#8d9894;font-size:24rpx;text-align:center}
</style>
