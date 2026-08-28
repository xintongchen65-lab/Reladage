<template>
  <view class="page-shell professional-page">
    <secondary-page-header title="专业数据" subtitle="供医生 / 康复师复核" @back="back"><template #action><button class="export-text header-chip pressable" @tap="chooseExport">导出</button></template></secondary-page-header>

    <view class="professional-hero">
      <view class="hero-label"><text>专业训练数据</text><text>数据完整度 {{ report.dataCompleteness }}%</text></view>
      <text class="hero-title">居家训练执行评估</text>
      <view class="hero-grid"><view><text>{{ report.adherence }}%</text><text>训练依从率</text></view><view><text>{{ report.execution }}%</text><text>处方剂量完成</text></view><view><text>{{ report.qualified }}%</text><text>动作达标率</text></view><view><text>{{ report.maxAngle }}°</text><text>最大活动角度</text></view></view>
    </view>

    <view class="section-head"><text class="section-title">关节活动度</text><text class="section-link">+{{ report.angleGain }}°</text></view>
    <view class="chart-card card"><view class="chart-summary"><text>{{ report.baselineAngle }}° 周期初</text><text>{{ report.maxAngle }}° 本周峰值</text></view><report-line-chart canvas-id="professionalRomChart" :values="angleValues" :labels="angleLabels" suffix="°" :min-value="60" :max-value="100"/><text>单周角度变化用于复核执行趋势，不单独作为疗效结论。</text></view>

    <view class="section-head"><text class="section-title">动作质量指标</text><text class="section-link">本周均值</text></view>
    <view class="motion-metrics-card card"><view><text>{{ report.avgSpeed }}</text><text>°/s</text><text>平均动作速度</text></view><view><text>{{ report.stability }}</text><text>%</text><text>角度稳定性</text></view><view><text>{{ report.lrRomDiff }}</text><text>°</text><text>左右活动度差</text></view><view><text>{{ report.compensationCount }}</text><text>次</text><text>代偿提示</text></view><text class="motion-metrics-note">动作速度逐步放缓，稳定性提高，左右差异较周期初缩小。</text></view>

    <view class="section-head"><text class="section-title">处方执行量</text><text class="section-link">近7天</text></view>
    <view class="dose-card card"><view><text>{{ report.completedSessions }}/{{ report.plannedSessions }}</text><text>完成训练</text></view><view><text>{{ report.completedReps }}/{{ report.plannedReps }}</text><text>动作剂量</text></view><view><text>{{ report.totalMinutes }}</text><text>训练分钟</text></view><view><text>{{ report.deviceInterruptions }}</text><text>设备中断</text></view></view>

    <view class="section-head"><text class="section-title">动作问题构成</text><text class="section-link">传感器判定</text></view>
    <view class="reason-card card"><view v-for="item in report.reasons" :key="item.id"><view><text>{{ item.label }}</text><text>{{ item.count }} 次 · {{ item.percent }}%</text></view><view><view :style="{ width: item.percent + '%' }"></view></view></view></view>

    <view class="section-head"><text class="section-title">症状与安全信号</text></view>
    <view class="safety-card card"><view><text>{{ report.avgPainBefore }}</text><text>/10</text><text>训练前疼痛</text></view><view><text>{{ report.avgPainAfter }}</text><text>/10</text><text>训练后疼痛</text></view><view><text>{{ report.highPainSessions }}</text><text>次</text><text>疼痛 ≥4</text></view><view><text>{{ report.warningSessions }}</text><text>次</text><text>设备异常</text></view></view>

    <view class="section-head"><text class="section-title">逐次训练明细</text><text class="section-link">左右滑动</text></view>
    <scroll-view class="table-scroll card" scroll-x :show-scrollbar="false"><view class="data-table"><view class="table-row header"><text>日期</text><text>动作</text><text>剂量</text><text>达标率</text><text>最大角度</text><text>疼痛</text></view><view v-for="item in report.sessions" :key="item.date" class="table-row"><text>{{ item.date }}</text><text>{{ item.completed ? item.exercise : '未训练' }}</text><text>{{ item.completedReps }}/{{ item.plannedReps }}</text><text>{{ item.completed ? item.qualified + '%' : '—' }}</text><text>{{ item.completed ? item.maxAngle + '°' : '—' }}</text><text>{{ item.completed ? item.painAfter + '/10' : '—' }}</text></view></view></scroll-view>

    <view class="section-head"><text class="section-title">医生 / 康复师备注</text><text class="section-link" @tap="saveNotes">保存</text></view>
    <view class="notes-card card"><textarea v-model="notes" maxlength="300" placeholder="填写复核结论、随访问题或处方调整记录"></textarea><text>{{ notes.length }}/300</text></view>
    <view class="professional-note">本页数据不能替代查体、功能量表和患者主诉。</view>
  </view>
</template>

<script>
import ReportLineChart from '../../components/report-line-chart/report-line-chart.vue'
import { buildWeeklyReport } from '../../services/report-data.js'
import { getDoctorNotes, saveDoctorNotes } from '../../services/device.js'
import { exportReport } from '../../utils/exporter.js'
import SecondaryPageHeader from '../../components/secondary-page-header/secondary-page-header.vue'
export default {
  components: { ReportLineChart, SecondaryPageHeader },
  data() { return { report: buildWeeklyReport(), notes: getDoctorNotes() } },
  computed: { angleValues() { return this.report.chronological.filter(item => item.completed).map(item => item.maxAngle) }, angleLabels() { return this.report.chronological.filter(item => item.completed).map(item => item.dayLabel) } },
  methods: {
    back() { uni.navigateBack() }, saveNotes() { saveDoctorNotes(this.notes); uni.showToast({ title: '备注已保存', icon: 'success' }) },
    chooseExport() { uni.showActionSheet({ itemList: ['导出 CSV 逐次数据', '导出 PDF 专业周报'], success: result => exportReport(result.tapIndex ? 'pdf' : 'csv', this.report, this.report.sessions, this.notes).catch(error => uni.showToast({ title: error.message || '导出失败', icon: 'none' })) }) }
  }
}
</script>

<style scoped>
.professional-page{padding-bottom:50rpx}.detail-nav{height:86rpx;display:flex;align-items:center}.detail-nav>view{flex:1;margin-left:15rpx}.detail-nav>view text{display:block;color:#1c4037;font-size:32rpx;font-weight:600}.detail-nav>view text:last-child{margin-top:3rpx;color:#8a9792;font-size:24rpx;font-weight:400}.professional-hero{margin-top:10rpx;padding:26rpx;border-radius:29rpx;background:#174f42}.hero-label{display:flex;justify-content:space-between}.hero-label text{color:#d9ee7f;font-size:24rpx;font-weight:600}.hero-label text:last-child{color:#a8c1b9;font-weight:400}.hero-title{display:block;margin-top:11rpx;color:#fff;font-size:36rpx;font-weight:600}.hero-grid{margin-top:25rpx;display:grid;grid-template-columns:repeat(2,1fr);row-gap:20rpx}.hero-grid>view{padding-left:18rpx;border-left:1rpx solid rgba(255,255,255,.13)}.hero-grid>view:nth-child(odd){padding-left:0;border:0}.hero-grid text{display:block;color:#d9ee7f;font-size:42rpx;font-weight:700}.hero-grid text:last-child{margin-top:7rpx;color:#a8c1b9;font-size:24rpx;font-weight:400}
.chart-card,.motion-metrics-card,.dose-card,.reason-card,.safety-card,.table-scroll,.notes-card{box-shadow:none}.chart-card{padding:20rpx}.chart-summary{display:flex;justify-content:space-between}.chart-summary text{color:#335c50;font-size:24rpx;font-weight:600}.chart-card>text{display:block;color:#87938e;font-size:24rpx;line-height:1.45}.motion-metrics-card{padding:21rpx;display:grid;grid-template-columns:repeat(2,1fr);gap:1rpx;background:#e3eae6}.motion-metrics-card>view{padding:19rpx;background:#fff}.motion-metrics-card>view text{display:inline;color:#174f42;font-size:36rpx;font-weight:700;font-variant-numeric:tabular-nums}.motion-metrics-card>view text:nth-child(2){margin-left:4rpx;font-size:24rpx;font-weight:500}.motion-metrics-card>view text:last-child{display:block;margin:8rpx 0 0;color:#899590;font-size:24rpx;font-weight:400}.motion-metrics-note{grid-column:1/-1;display:block;padding:18rpx 19rpx 4rpx;background:#fff;color:#5f756d;font-size:28rpx;line-height:1.5;font-weight:400}.dose-card{padding:21rpx;display:grid;grid-template-columns:repeat(2,1fr);gap:1rpx;background:#e3eae6}.dose-card>view{padding:19rpx;background:#fff}.dose-card text{display:block;color:#174f42;font-size:36rpx;font-weight:700}.dose-card text:last-child{margin-top:8rpx;color:#899590;font-size:24rpx;font-weight:400}.reason-card{padding:8rpx 22rpx}.reason-card>view{padding:16rpx 0}.reason-card>view>view:first-child{display:flex;justify-content:space-between}.reason-card text{color:#415f56;font-size:28rpx}.reason-card text:last-child{color:#7c8a85;font-size:24rpx}.reason-card>view>view:last-child{height:9rpx;margin-top:9rpx;border-radius:9rpx;background:#e8eeeb;overflow:hidden}.reason-card>view>view:last-child view{height:100%;background:#638a62}.safety-card{padding:22rpx;display:grid;grid-template-columns:repeat(4,1fr)}.safety-card>view{padding-left:14rpx;border-left:1rpx solid #e3e9e6}.safety-card>view:first-child{padding-left:0;border:0}.safety-card text{display:inline;color:#174f42;font-size:34rpx;font-weight:700}.safety-card text:nth-child(2){font-size:24rpx;font-weight:500}.safety-card text:last-child{display:block;margin-top:8rpx;color:#899590;font-size:24rpx;font-weight:400}.table-scroll{overflow:hidden}.data-table{width:950rpx;padding:0 18rpx}.table-row{min-height:75rpx;border-bottom:1rpx solid #e7ece9;display:grid;grid-template-columns:100rpx 190rpx repeat(4,150rpx);align-items:center}.table-row text{color:#50675f;font-size:24rpx;text-align:center}.table-row text:first-child,.table-row text:nth-child(2){text-align:left}.table-row.header text{color:#7d8b86;font-weight:600}.notes-card{padding:20rpx}.notes-card textarea{width:100%;min-height:150rpx;padding:16rpx;border-radius:17rpx;background:#eef3f0;color:#425f56;font-size:28rpx;line-height:1.55}.notes-card>text{display:block;margin-top:7rpx;color:#95a09c;font-size:24rpx;text-align:right}.professional-note{margin-top:22rpx;color:#8d9894;font-size:24rpx;text-align:center}
</style>
