<template>
  <view class="family-home">
    <app-page-header class="main-header" :title="`${greeting}，王女士`" :subtitle="`正在关注：${member.relationship} · ${member.name}`">
      <template #action>
        <button class="notification-button pressable" @tap="openNotifications"><text class="notification-label">消息</text><text v-if="unreadCount" class="notification-badge">{{ unreadCount }}</text></button>
      </template>
    </app-page-header>

    <view class="home-content">
    <view class="device-strip card pressable" @tap="goMaintenance">
      <view class="device-logo">R</view>
      <view class="device-copy">
        <view class="device-name-line"><text>RehabMotion</text><text>/ {{ device.deviceName }}</text></view>
        <view class="connection-line">
          <view class="status-dot" :class="{ offline: !device.connected }"></view>
          <text>{{ device.connected ? '已连接' : '未连接' }}</text>
        </view>
      </view>
      <view class="device-battery"><text>电量</text><text>{{ device.battery }}%</text></view>
    </view>

    <view class="today-card">
      <view class="card-heading light-heading">
        <view>
          <text class="card-kicker">TODAY</text>
          <text class="card-title light-title">今日康复训练</text>
        </view>
        <button class="today-detail pressable" @tap="goTodayRecords">查看详情 <text>›</text></button>
      </view>

      <view class="today-progress-row">
        <view class="today-fraction">
          <text>{{ todayCompleted }}</text>
          <text>/ {{ todayPlanTotal }}</text>
          <text>项</text>
        </view>
        <view class="today-progress-copy">
          <text>已完成</text>
          <text>{{ todayProgress }}%</text>
        </view>
      </view>
      <view class="progress-track"><view :style="{ width: todayProgress + '%' }"></view></view>

      <view class="today-metrics">
        <view>
          <text class="metric-label">累计训练</text>
          <view class="metric-number"><text>{{ today.minutes }}</text><text>min</text></view>
        </view>
        <view>
          <text class="metric-label">完成动作</text>
          <view class="metric-number"><text>{{ todayActionCount }}</text><text>次</text></view>
        </view>
        <view>
          <text class="metric-label">动作达标率</text>
          <view class="metric-number"><text>{{ today.qualified }}</text><text>%</text></view>
        </view>
      </view>

      <view class="latest-session">
        <view class="latest-copy">
          <text class="latest-label">最近完成</text>
          <text class="latest-name">{{ latestExercise }}</text>
        </view>
        <view class="feedback-row">
          <text>✓ 强度{{ todayIntensity }}</text>
          <text>☺ 疼痛{{ todayPain }}</text>
        </view>
      </view>
    </view>

    <view class="summary-card card">
      <view class="card-heading week-heading">
        <view>
          <text class="card-kicker green-kicker">THIS WEEK</text>
          <text class="card-title">本周训练</text>
        </view>
        <button class="text-link" @tap="goWeekRecords">全部记录</button>
      </view>

      <view class="week-metrics-row">
        <view class="week-main-metric">
          <text class="summary-label">完成次数</text>
          <view class="week-fraction"><text>{{ weeklyCompleted }}</text><text>/ {{ weeklyPlanned }}</text><text>次</text></view>
        </view>
        <view class="week-main-metric quality-metric">
          <text class="summary-label">动作达标率</text>
          <view class="week-quality"><text>{{ weeklyQualified }}</text><text>%</text></view>
        </view>
      </view>

      <view class="week-calendar">
        <view v-for="day in weekCheckins" :key="day.label" class="calendar-day">
          <text class="weekday-label">{{ day.label }}</text>
          <view :class="['calendar-date', { today: day.isToday }]">
            <text class="date-number">{{ day.date }}</text>
            <view :class="['calendar-status', day.state]"><text v-if="day.state === 'completed'">✓</text></view>
          </view>
        </view>
      </view>
      <view class="calendar-legend">
        <view><view class="legend-status completed"><text>✓</text></view><text>已完成</text></view>
        <view><view class="legend-status partial"></view><text>部分完成</text></view>
        <view><view class="legend-status missed"></view><text>未完成</text></view>
        <view><view class="legend-status unplanned"></view><text>未计划</text></view>
      </view>

    </view>

    <view class="attention-card card" :class="{ normal: !attentionItems.length }">
      <view class="card-heading attention-heading">
        <view>
          <text class="card-kicker" :class="attentionItems.length ? 'warm-kicker' : 'green-kicker'">ATTENTION</text>
          <text class="card-title">关注事项</text>
        </view>
        <text v-if="attentionItems.length" class="attention-count">{{ attentionItems.length }} 项</text>
      </view>

      <view v-if="primaryAttention" class="attention-list">
        <view class="attention-primary pressable" @tap="openAttention(primaryAttention)">
          <view class="attention-copy"><text>{{ primaryAttention.title }}</text><text>{{ primaryAttention.detail }}</text></view>
          <text class="attention-action">查看建议 ›</text>
        </view>
      </view>
      <view v-else class="normal-state">
        <view class="normal-check">✓</view>
        <view><text>本周训练状态正常</text><text>暂时没有需要家属特别关注的问题</text></view>
      </view>
    </view>

    <view class="medical-note"><text class="note-icon">i</text><text>数据仅反映居家训练执行情况，不替代医生诊断或治疗建议。</text></view>
    </view>
  </view>
</template>

<script>
import AppPageHeader from '../../components/app-page-header/app-page-header.vue'
import { getDeviceSnapshot, getPrescription } from '../../services/device.js'
import { getTrainingRecords, todayISO } from '../../services/training-records.js'
import { getReportHomeData } from '../../services/report-dashboard.js'
import { getUnreadMessageCount } from '../../services/messages.js'
import { getCurrentMember } from '../../services/members.js'

export default {
  components: { AppPageHeader },
  data() {
    return {
      device: getDeviceSnapshot(),
      config: getPrescription(),
      records: getTrainingRecords(),
      dashboard: getReportHomeData(),
      unreadCount: getUnreadMessageCount(),
      member: getCurrentMember(),
      now: new Date()
    }
  },
  computed: {
    greeting() {
      const hour = this.now.getHours()
      if (hour < 6) return '晚上好'
      if (hour < 11) return '早上好'
      if (hour < 14) return '中午好'
      if (hour < 18) return '下午好'
      return '晚上好'
    },
    today() {
      return this.records.find(item => item.date === todayISO()) || { date: todayISO(), completion: 0, qualified: 0, minutes: 0, completed: false, sessions: [] }
    },
    todayPlanTotal() {
      return this.today.sessions.length
    },
    todayCompleted() {
      return this.today.sessions.filter(item => item.status === 'completed').length
    },
    todayProgress() {
      return this.todayPlanTotal ? Math.round(this.todayCompleted / this.todayPlanTotal * 100) : 0
    },
    todayActionCount() {
      if (Number.isFinite(Number(this.today.completedReps))) return Number(this.today.completedReps)
      const planned = (this.config.sets || 0) * (this.config.reps || 0)
      return Math.round(planned * (this.today.completion || 0) / 100)
    },
    latestExercise() {
      const completed = this.today.sessions.filter(item => item.status === 'completed')
      return completed[completed.length - 1]?.name || '暂无已完成训练'
    },
    todayIntensity() {
      const fatigue = Number(this.today.fatigue || 0)
      if (!fatigue) return '未填写'
      return fatigue <= 2 ? '合适' : fatigue === 3 ? '稍高' : '偏高'
    },
    todayPain() {
      const pain = Number(this.today.painAfter || 0)
      if (!pain) return '无明显'
      return pain <= 2 ? '轻微' : pain <= 4 ? '需关注' : '较明显'
    },
    weeklyRecords() {
      return this.dashboard.week.sessions
    },
    weeklyPlanned() {
      return this.dashboard.week.plannedSessions
    },
    weeklyCompleted() {
      return this.dashboard.week.completedSessions
    },
    weeklyQualified() {
      return this.dashboard.week.qualified
    },
    weekCheckins() {
      const labels = ['一', '二', '三', '四', '五', '六', '日']
      return this.dashboard.week.sessions.map((record, index) => {
        let state = 'unplanned'
        if (record.status === 'completed') state = 'completed'
        else if (record.status === 'partial') state = 'partial'
        else if (record.status === 'not_started') state = 'missed'
        return { label: labels[index], date: Number(record.isoDate.slice(-2)), state, isToday: record.isoDate === todayISO() }
      })
    },
    primaryAttention() {
      return this.attentionItems[0] || null
    },
    attentionItems() {
      const items = []
      const latestTwo = this.records.filter(item => item.status === 'completed' || item.status === 'partial').slice(0, 2)
      const lowQuality = latestTwo.filter(item => item.qualified < 90).length
      if (lowQuality) {
        items.push({
          id: 'quality',
          type: 'session',
          date: latestTwo.find(item => item.qualified < 90)?.date,
          title: '动作稳定性需要关注',
          detail: `近 2 次训练有 ${lowQuality} 次动作达标率低于 90%`
        })
      }
      const missedRecords = this.weeklyRecords.filter(item => item.status === 'not_started')
      if (missedRecords.length) {
        items.push({
          id: 'missed',
          type: 'history',
          date: missedRecords[0].isoDate,
          title: '本周训练计划有漏项',
          detail: `本周有 ${missedRecords.length} 次训练未完成`
        })
      }
      return items.slice(0, 2)
    }
  },
  onShow() {
    this.device = getDeviceSnapshot()
    this.config = getPrescription()
    this.records = getTrainingRecords()
    this.dashboard = getReportHomeData()
    this.unreadCount = getUnreadMessageCount()
    this.member = getCurrentMember()
    this.now = new Date()
  },
  methods: {
    goMaintenance() {
      uni.navigateTo({ url: '/pages/maintenance/index' })
    },
    goTodayRecords() {
      const date = this.today.date ? `?date=${this.today.date}` : ''
      uni.navigateTo({ url: '/pages/daily/index' + date })
    },
    goWeekRecords() {
      uni.navigateTo({ url: '/pages/records/index' })
    },
    openNotifications() {
      uni.navigateTo({ url: '/pages/alerts/index' })
    },
    openAttention(item) {
      if (item.type === 'device') return this.goMaintenance()
      const query = item.date ? `?date=${item.date}` : ''
      uni.navigateTo({ url: '/pages/daily/index' + query })
    }
  }
}
</script>

<style scoped>
.family-home { min-height: 100vh; background: #f5f6f3; padding-bottom: calc(42rpx + env(safe-area-inset-bottom)); }
.home-content { padding: 10rpx 30rpx 0; }
.family-home { padding-bottom: 52rpx; }
.family-header { min-height: 132rpx; padding: 17rpx 2rpx 21rpx; display: flex; align-items: center; justify-content: space-between; }
.family-heading text { display: block; }
.greeting { color: #163d33; font-size: 44rpx; line-height: 1.08; font-weight: 700; letter-spacing: -1rpx; }
.focus-person { margin-top: 12rpx; color: #778680; font-size: 28rpx; font-weight: 400; }
.notification-button { position: relative; width: 72rpx; height: 72rpx; border-radius: 23rpx; background: #fff; display: flex; align-items: center; justify-content: center; box-shadow: 0 8rpx 24rpx rgba(32, 72, 61, .07); }
.bell-shape { position: relative; width: 29rpx; height: 30rpx; border: 4rpx solid #345e52; border-top-left-radius: 15rpx; border-top-right-radius: 15rpx; border-bottom: 0; }
.bell-shape::after { content: ''; position: absolute; left: -7rpx; bottom: -5rpx; width: 35rpx; height: 4rpx; border-radius: 4rpx; background: #345e52; }
.bell-shape view { position: absolute; left: 8rpx; bottom: -11rpx; width: 7rpx; height: 7rpx; border-radius: 50%; background: #345e52; }
.notification-badge { position: absolute; right: -3rpx; top: -3rpx; min-width: 30rpx; height: 30rpx; padding: 0 7rpx; border: 4rpx solid #f3f6f4; border-radius: 999rpx; background: #c66e58; color: #fff; display: flex; align-items: center; justify-content: center; font-size: 22rpx; line-height: 1; font-weight: 600; font-variant-numeric: tabular-nums; }

.device-strip { padding: 20rpx 21rpx; display: flex; align-items: center; }
.device-logo { flex: none; width: 58rpx; height: 58rpx; border-radius: 18rpx 18rpx 18rpx 6rpx; background: #174f42; color: #d9ee7f; display: flex; align-items: center; justify-content: center; font-size: 30rpx; font-weight: 700; transform: rotate(-4deg); }
.device-copy { min-width: 0; flex: 1; margin-left: 17rpx; }
.device-copy > text { display: block; color: #23483e; font-size: 30rpx; font-weight: 600; }
.connection-line { margin-top: 6rpx; color: #81908a; display: flex; align-items: center; gap: 8rpx; font-size: 24rpx; font-weight: 400; }
.connection-line .status-dot { background: #729753; box-shadow: 0 0 0 5rpx rgba(114, 151, 83, .12); }
.connection-line .status-dot.offline { background: #bf705c; box-shadow: 0 0 0 5rpx rgba(191, 112, 92, .12); }
.battery-copy { padding-left: 16rpx; text-align: right; }
.battery-copy text { display: block; color: #225247; font-size: 30rpx; line-height: 1.05; font-weight: 700; letter-spacing: -1rpx; font-variant-numeric: tabular-nums; }
.battery-copy text:last-child { margin-top: 6rpx; color: #98a29e; font-size: 22rpx; font-weight: 400; letter-spacing: 0; }
.strip-arrow { margin-left: 12rpx; color: #95a09c; font-size: 40rpx; }

.today-card { position: relative; margin-top: 20rpx; padding: 30rpx; overflow: hidden; border-radius: 34rpx; background: #174f42; color: #fff; box-shadow: 0 18rpx 42rpx rgba(23, 79, 66, .17); }
.today-card::after { content: ''; position: absolute; right: -120rpx; top: -155rpx; width: 330rpx; height: 330rpx; border: 2rpx solid rgba(217, 238, 127, .13); border-radius: 50%; }
.card-heading { position: relative; z-index: 1; display: flex; align-items: flex-start; justify-content: space-between; }
.card-heading text { display: block; }
.card-kicker { color: #8b9994; font-size: 22rpx; line-height: 1; font-weight: 600; letter-spacing: 2rpx; }
.green-kicker { color: #668c61; }
.warm-kicker { color: #b96b55; }
.card-title { margin-top: 10rpx; color: #21483d; font-size: 32rpx; line-height: 1.2; font-weight: 600; }
.light-heading .card-kicker { color: #9dbbb2; }
.light-title { color: #fff; }
.today-progress-row { position: relative; z-index: 1; margin-top: 28rpx; display: flex; align-items: flex-end; justify-content: space-between; }
.today-fraction { display: flex; align-items: baseline; color: #fff; }
.today-fraction text:first-child { font-size: 64rpx; line-height: 1; font-weight: 700; letter-spacing: -2rpx; }
.today-fraction text:nth-child(2) { margin-left: 8rpx; color: #d9ee7f; font-size: 36rpx; font-weight: 600; }
.today-fraction text:last-child { margin-left: 8rpx; color: #a9c3bb; font-size: 26rpx; font-weight: 400; }
.today-progress-copy { padding-bottom: 5rpx; text-align: right; }
.today-progress-copy text { display: block; color: #a9c3bb; font-size: 24rpx; font-weight: 400; }
.today-progress-copy text:last-child { margin-top: 5rpx; color: #d9ee7f; font-size: 32rpx; line-height: 1; font-weight: 700; font-variant-numeric: tabular-nums; }
.progress-track { position: relative; z-index: 1; height: 10rpx; margin-top: 18rpx; overflow: hidden; border-radius: 10rpx; background: rgba(255, 255, 255, .12); }
.progress-track view { height: 100%; border-radius: 10rpx; background: #d9ee7f; }
.today-metrics { position: relative; z-index: 1; margin-top: 25rpx; display: grid; grid-template-columns: repeat(2, 1fr); }
.today-metrics > view { padding-right: 25rpx; border-right: 1rpx solid rgba(255, 255, 255, .13); }
.today-metrics > view:last-child { padding-left: 28rpx; border-right: 0; }
.metric-label { display: block; color: #9ebbb2; font-size: 24rpx; font-weight: 400; }
.metric-number { margin-top: 9rpx; display: flex; align-items: baseline; }
.metric-number text:first-child { color: #fff; font-size: 48rpx; line-height: 1.05; font-weight: 700; letter-spacing: -1rpx; font-variant-numeric: tabular-nums; }
.metric-number text:last-child { margin-left: 7rpx; color: #b2c8c1; font-size: 24rpx; font-weight: 500; }
.latest-session { position: relative; z-index: 1; margin-top: 25rpx; padding: 22rpx; border-radius: 23rpx; background: rgba(255, 255, 255, .08); }
.latest-label { display: block; color: #9ebbb2; font-size: 22rpx; font-weight: 400; }
.latest-name { display: block; margin-top: 7rpx; color: #fff; font-size: 28rpx; font-weight: 600; }
.feedback-row { margin-top: 17rpx; display: flex; gap: 10rpx; }
.feedback-row text { padding: 8rpx 13rpx; border-radius: 999rpx; background: rgba(217, 238, 127, .13); color: #dcebb0; font-size: 24rpx; font-weight: 500; }
.feedback-row text:last-child { background: rgba(247, 214, 190, .13); color: #f0cbbb; }

.summary-card, .attention-card { margin-top: 20rpx; padding: 28rpx; }
.text-link { color: #267261; font-size: 26rpx; font-weight: 500; }
.summary-label { display: block; color: #7f8e88; font-size: 26rpx; font-weight: 400; }
.week-fraction { margin-top: 8rpx; display: flex; align-items: baseline; color: #1f4f43; }
.week-fraction text:first-child { font-size: 48rpx; line-height: 1.05; font-weight: 700; letter-spacing: -1rpx; }
.week-fraction text:nth-child(2) { margin-left: 7rpx; font-size: 32rpx; font-weight: 600; }
.week-fraction text:last-child { margin-left: 7rpx; color: #7d8d87; font-size: 24rpx; font-weight: 400; }
.week-calendar { margin-top: 25rpx; padding: 18rpx 10rpx 16rpx; border-radius: 22rpx; background: #f7f9f7; display: grid; grid-template-columns: repeat(7, 1fr); }
.calendar-day { min-width: 0; display: flex; flex-direction: column; align-items: center; }
.weekday-label { color: #6f7e78; font-size: 24rpx; line-height: 1; font-weight: 500; }
.calendar-date { width: 66rpx; height: 86rpx; margin-top: 12rpx; border-radius: 18rpx; display: flex; flex-direction: column; align-items: center; justify-content: center; }
.calendar-date.today { background: #729753; box-shadow: 0 8rpx 18rpx rgba(114, 151, 83, .2); }
.date-number { color: #293f38; font-size: 28rpx; line-height: 1; font-weight: 500; font-variant-numeric: tabular-nums; }
.calendar-date.today .date-number { color: #fff; font-weight: 600; }
.calendar-status { width: 24rpx; height: 24rpx; margin-top: 11rpx; border-radius: 50%; display: flex; align-items: center; justify-content: center; font-size: 18rpx; line-height: 1; font-weight: 600; }
.calendar-status.completed { background: #729753; color: #fff; }
.calendar-status.partial { border: 3rpx solid #8aa760; border-left-color: #dbeadd; background: #fff; }
.calendar-status.missed { border: 3rpx solid #efc84b; border-left-color: #f7eed0; background: #fff; }
.calendar-status.unplanned { border: 3rpx solid #ccd4d0; background: transparent; }
.calendar-date.today .calendar-status { border-color: rgba(255, 255, 255, .95); border-left-color: rgba(255, 255, 255, .38); background: transparent; color: #fff; }
.calendar-date.today .calendar-status.completed { border: 0; background: #fff; color: #729753; }
.calendar-legend { margin-top: 15rpx; display: grid; grid-template-columns: repeat(4, 1fr); gap: 8rpx; }
.calendar-legend > view { min-width: 0; display: flex; align-items: center; justify-content: center; gap: 7rpx; color: #83908b; font-size: 22rpx; line-height: 1.2; font-weight: 400; white-space: nowrap; }
.legend-status { flex: none; width: 22rpx; height: 22rpx; border-radius: 50%; display: flex; align-items: center; justify-content: center; font-size: 16rpx; line-height: 1; font-weight: 600; }
.legend-status.completed { background: #729753; color: #fff; }
.legend-status.partial { border: 3rpx solid #8aa760; border-left-color: #dbeadd; }
.legend-status.missed { border: 3rpx solid #efc84b; border-left-color: #f7eed0; }
.legend-status.unplanned { border: 3rpx solid #ccd4d0; }

.attention-card { border-color: rgba(181, 98, 76, .16); background: #fffaf7; }
.attention-card.normal { border-color: rgba(19, 71, 57, .06); background: #fff; }
.attention-heading { align-items: center; }
.attention-count { padding: 8rpx 13rpx; border-radius: 999rpx; background: #f5e2da; color: #a85f4d; font-size: 24rpx; font-weight: 600; font-variant-numeric: tabular-nums; }
.attention-list { margin-top: 20rpx; }
.attention-item { min-height: 102rpx; padding: 16rpx 0; border-top: 1rpx solid #f0e5e0; display: flex; align-items: center; }
.attention-item:first-child { border-top: 0; }
.attention-mark { flex: none; width: 48rpx; height: 48rpx; border-radius: 16rpx; background: #f4ddd5; color: #ad604e; display: flex; align-items: center; justify-content: center; font-size: 28rpx; font-weight: 700; }
.attention-copy { min-width: 0; flex: 1; margin-left: 16rpx; }
.attention-copy text { display: block; color: #4b423e; font-size: 28rpx; font-weight: 600; }
.attention-copy text:last-child { margin-top: 7rpx; color: #978b86; font-size: 24rpx; font-weight: 400; }
.attention-arrow { margin-left: 10rpx; color: #b8aaa5; font-size: 40rpx; }
.normal-state { margin-top: 22rpx; padding: 21rpx; border-radius: 22rpx; background: #edf4e6; display: flex; align-items: center; }
.normal-check { flex: none; width: 52rpx; height: 52rpx; border-radius: 17rpx; background: #d9e9cc; color: #5f8452; display: flex; align-items: center; justify-content: center; font-size: 30rpx; font-weight: 700; }
.normal-state > view:last-child { margin-left: 16rpx; }
.normal-state text { display: block; color: #3f654e; font-size: 28rpx; font-weight: 600; }
.normal-state text:last-child { margin-top: 6rpx; color: #829181; font-size: 24rpx; font-weight: 400; }
.medical-note { margin: 24rpx 6rpx 0; display: flex; align-items: center; justify-content: center; gap: 11rpx; color: #909b97; font-size: 24rpx; line-height: 1.5; font-weight: 400; }
.note-icon { flex: none; width: 29rpx; height: 29rpx; border: 2rpx solid #909b97; border-radius: 50%; display: flex; align-items: center; justify-content: center; font-size: 22rpx; }

/* Compact dark today card v3 */
.today-card { padding: 24rpx 27rpx 22rpx; border-radius: 29rpx; background: #174f42; box-shadow: 0 13rpx 31rpx rgba(19, 45, 32, .14); }
.today-card::after { width: 230rpx; height: 230rpx; right: -105rpx; top: -130rpx; }
.light-heading .card-kicker { color: #d9ee7f; }
.light-title { margin-top: 7rpx; color: #fff; }
.today-detail { height: 52rpx; padding: 0 16rpx; border: 2rpx solid rgba(217, 238, 127, .72); border-radius: 999rpx; color: #d9ee7f; display: flex; align-items: center; gap: 7rpx; font-size: 28rpx; font-weight: 600; }
.today-detail text { font-size: 28rpx; }
.today-progress-row { margin-top: 17rpx; }
.today-fraction text:first-child { font-size: 56rpx; }
.today-fraction text:nth-child(2) { color: #fff; font-size: 32rpx; }
.today-progress-copy { padding-bottom: 2rpx; }
.today-progress-copy text:last-child { color: #fff; font-size: 32rpx; }
.progress-track { height: 8rpx; margin-top: 11rpx; }
.progress-track view { background: #d9ee7f; }
.today-metrics { margin-top: 14rpx; padding: 14rpx 0; border-top: 1rpx solid rgba(255, 255, 255, .09); border-bottom: 1rpx solid rgba(255, 255, 255, .09); grid-template-columns: repeat(3, 1fr); }
.today-metrics > view, .today-metrics > view:last-child { padding: 0 12rpx; border-right: 1rpx solid rgba(255, 255, 255, .13); text-align: center; }
.today-metrics > view:last-child { border-right: 0; }
.metric-number { margin-top: 0; justify-content: center; }
.metric-number text:first-child { font-size: 36rpx; }
.metric-number text:last-child { font-size: 24rpx; }
.metric-label { margin-top: 5rpx; font-size: 24rpx; }
.latest-session { margin-top: 13rpx; padding: 0; border: 0; border-radius: 0; background: transparent; display: flex; align-items: center; justify-content: space-between; }
.latest-copy { min-width: 0; flex: 1; }
.latest-label { color: #d9ee7f; font-size: 24rpx; }
.latest-name { margin-top: 4rpx; font-size: 28rpx; font-weight: 600; }
.feedback-row { flex: none; margin-top: 0; margin-left: 12rpx; gap: 7rpx; }
.feedback-row text { padding: 6rpx 10rpx; background: rgba(217, 238, 127, .1); color: #d9ee7f; font-size: 24rpx; font-weight: 500; }
.feedback-row text:last-child { background: rgba(217, 238, 127, .08); color: #d9ee7f; }
.summary-card { padding-bottom: 25rpx; }
.week-metrics-row { margin-top: 23rpx; display: grid; grid-template-columns: repeat(2, 1fr); align-items: end; }
.week-main-metric { min-width: 0; }
.quality-metric { padding-left: 28rpx; border-left: 1rpx solid #e6ece9; text-align: right; }
.week-quality { margin-top: 8rpx; display: flex; align-items: baseline; justify-content: flex-end; color: #174f42; }
.week-quality text:first-child { font-size: 48rpx; line-height: 1.05; font-weight: 700; letter-spacing: -1rpx; font-variant-numeric: tabular-nums; }
.week-quality text:last-child { margin-left: 3rpx; font-size: 28rpx; font-weight: 600; }
.week-calendar { margin-top: 20rpx; }


/* Unified home palette v4 */
.today-metrics > view,
.today-metrics > view:last-child { padding: 0 17rpx; text-align: left; }
.today-metrics > view:first-child { padding-left: 0; }
.today-metrics > view:last-child { padding-right: 0; }
.metric-label { margin-top: 0; color: #a7c2ba; }
.metric-number { margin-top: 7rpx; justify-content: flex-start; }
.quality-metric { text-align: left; }
.week-quality { justify-content: flex-start; color: #174f42; }
.calendar-date.today,
.calendar-status.completed,
.legend-status.completed { background: #729753; }
.calendar-status.partial,
.legend-status.partial { border-color: #8aa760; border-left-color: #dfe8d7; }
.calendar-date.today .calendar-status.completed { color: #729753; }


/* Weekly card subtle gradient v5 */
.summary-card {
  border-color: rgba(114, 151, 83, .12);
  background: linear-gradient(145deg, #ffffff 0%, #f8faf5 52%, #edf3e7 100%);
}


/* Today card divider and bubble refinement v6 */
.today-metrics {
  border-top: 0;
  border-bottom: 0;
}
.latest-session {
  padding: 14rpx 16rpx;
  border: 0;
  border-radius: 18rpx;
  background: rgba(255, 255, 255, .08);
}


/* Header gradients and teal calendar v7 */
.summary-card,
.attention-card,
.attention-card.normal {
  border-color: rgba(19, 71, 57, .06);
  background: #fff;
}

.week-heading,
.attention-heading {
  margin: -8rpx -8rpx 0;
  padding: 18rpx;
  border-radius: 20rpx;
}

.week-heading {
  background: linear-gradient(90deg, rgba(217, 238, 127, .72) 0%, rgba(229, 240, 190, .66) 56%, rgba(255, 255, 255, 0) 100%);
}

.attention-heading {
  background: linear-gradient(90deg, rgba(247, 207, 189, .72) 0%, rgba(250, 231, 221, .64) 56%, rgba(255, 255, 255, 0) 100%);
}

.week-calendar {
  background: #f3f7f5;
}

.calendar-date.today,
.calendar-status.completed,
.legend-status.completed {
  background: #2f7867;
}

.calendar-date.today {
  box-shadow: 0 8rpx 18rpx rgba(47, 120, 103, .20);
}

.calendar-status.partial,
.legend-status.partial {
  border-color: #79a99b;
  border-left-color: #dce9e5;
}

.calendar-status.missed,
.legend-status.missed {
  border-color: #d9a441;
  border-left-color: #f3e5bf;
}

.calendar-status.unplanned,
.legend-status.unplanned {
  border-color: #c7d0cc;
}

.calendar-date.today .calendar-status.completed {
  color: #2f7867;
}


/* Refined heading gradients v8 */
.week-heading,
.attention-heading {
  margin: -4rpx -4rpx 0;
  padding: 16rpx 18rpx;
  border-radius: 18rpx;
}

.week-heading {
  background: linear-gradient(110deg, #dcebe6 0%, #edf4f1 48%, #ffffff 100%);
}

.attention-heading {
  background: linear-gradient(110deg, #f5e9e2 0%, #fbf3ef 48%, #ffffff 100%);
}


/* Filled today detail bubble v9 */
.today-detail {
  border-color: #d9ee7f;
  background: #d9ee7f;
  color: #174f42;
}


/* Home hierarchy refinement v10 */
.family-header {
  min-height: 106rpx;
  padding: 8rpx 2rpx 13rpx;
}

.greeting {
  font-size: 42rpx;
  line-height: 1.12;
  font-weight: 600;
  letter-spacing: 0;
}

.focus-person {
  margin-top: 7rpx;
  color: #8e9a95;
  font-size: 24rpx;
}

.notification-button {
  width: 64rpx;
  height: 64rpx;
  border-radius: 18rpx;
  box-shadow: none;
}

.device-strip {
  min-height: 76rpx;
  padding: 13rpx 17rpx;
  border: 1rpx solid rgba(19, 71, 57, .06);
  border-radius: 22rpx;
  box-shadow: none;
}

.device-logo {
  width: 48rpx;
  height: 48rpx;
  border-radius: 14rpx 14rpx 14rpx 5rpx;
  font-size: 26rpx;
}

.device-copy { margin-left: 14rpx; }
.device-name-line { display: flex; align-items: baseline; gap: 8rpx; white-space: nowrap; }
.device-name-line text:first-child { color: #23483e; font-size: 28rpx; font-weight: 600; }
.device-name-line text:last-child { color: #89958f; font-size: 24rpx; font-weight: 400; }
.connection-line { margin-top: 3rpx; gap: 7rpx; font-size: 22rpx; }
.connection-line .status-dot { box-shadow: none; }
.device-battery {
  flex: none;
  margin-left: 16rpx;
  color: #225247;
  font-size: 30rpx;
  line-height: 1;
  font-weight: 700;
  letter-spacing: -1rpx;
  font-variant-numeric: tabular-nums;
}

.today-card {
  margin-top: 16rpx;
  box-shadow: 0 11rpx 27rpx rgba(23, 79, 66, .10);
}

.today-progress-row { margin-top: 20rpx; }
.today-fraction text:first-child { font-size: 72rpx; }
.today-fraction text:nth-child(2) { font-size: 38rpx; }
.today-progress-copy { padding-bottom: 4rpx; }
.today-progress-copy text { font-size: 22rpx; }
.today-progress-copy text:last-child {
  margin-top: 4rpx;
  color: #a7c2ba;
  font-size: 28rpx;
  font-weight: 600;
}
.progress-track { margin-top: 14rpx; }

.today-metrics {
  margin-top: 18rpx;
  padding: 18rpx 0 16rpx;
  grid-template-columns: repeat(3, minmax(0, 1fr));
  column-gap: 24rpx;
}
.today-metrics > view,
.today-metrics > view:last-child {
  min-width: 0;
  padding: 0;
  border-right: 0;
}
.metric-number { margin-top: 8rpx; }
.metric-number text:first-child { font-size: 44rpx; }
.latest-session {
  margin-top: 12rpx;
  padding: 14rpx 0 2rpx;
  border-radius: 0;
  background: transparent;
}

.summary-card,
.attention-card {
  margin-top: 18rpx;
  padding: 26rpx;
  border: 1rpx solid rgba(19, 71, 57, .055);
  border-radius: 24rpx;
  box-shadow: none;
}

.summary-card { background: #fff; }
.week-heading {
  margin: 0;
  padding: 0;
  border-radius: 0;
  background: transparent;
}
.week-metrics-row {
  margin-top: 25rpx;
  column-gap: 32rpx;
}
.quality-metric {
  padding-left: 0;
  border-left: 0;
}

.week-calendar {
  margin-top: 24rpx;
  padding: 0;
  border-radius: 0;
  background: transparent;
}
.calendar-date {
  width: 60rpx;
  height: 76rpx;
  margin-top: 9rpx;
  border-radius: 0;
}
.calendar-date.today {
  background: transparent;
  box-shadow: none;
}
.calendar-date.today .date-number {
  width: 44rpx;
  height: 44rpx;
  border-radius: 14rpx;
  background: #e3efe9;
  color: #174f42;
  display: flex;
  align-items: center;
  justify-content: center;
}
.calendar-status { margin-top: 8rpx; }
.calendar-date.today .calendar-status.completed {
  border: 0;
  background: #2f7867;
  color: #fff;
}
.calendar-date.today .calendar-status.partial {
  border-color: #79a99b;
  border-left-color: #dce9e5;
  background: #fff;
}
.calendar-date.today .calendar-status.missed {
  border-color: #d9a441;
  border-left-color: #f3e5bf;
  background: #fff;
}
.calendar-date.today .calendar-status.unplanned {
  border-color: #c7d0cc;
  background: transparent;
}
.calendar-legend { margin-top: 12rpx; }

.attention-card,
.attention-card.normal {
  background: linear-gradient(180deg, rgba(247, 235, 228, .58) 0%, #fff 112rpx);
}
.attention-heading {
  margin: 0;
  padding: 0;
  border-radius: 0;
  background: transparent;
}
.attention-count {
  padding: 5rpx 10rpx;
  background: #f1ddd4;
  font-size: 22rpx;
}
.attention-list { margin-top: 23rpx; }
.attention-primary { display: flex; align-items: center; }
.attention-copy { margin-left: 0; }
.attention-copy text { color: #3f4743; font-size: 28rpx; font-weight: 600; }
.attention-copy text:last-child { margin-top: 7rpx; color: #8d9691; font-size: 24rpx; font-weight: 400; }
.attention-action {
  flex: none;
  margin-left: 18rpx;
  color: #267261;
  font-size: 24rpx;
  font-weight: 600;
  white-space: nowrap;
}
.normal-state {
  margin-top: 22rpx;
  padding: 0;
  border-radius: 0;
  background: transparent;
}


/* Home alignment refinement v11 */
.device-battery {
  display: flex;
  align-items: baseline;
  gap: 7rpx;
}
.device-battery text:first-child {
  color: #8b9893;
  font-size: 22rpx;
  font-weight: 500;
}
.device-battery text:last-child {
  color: #225247;
  font-size: 30rpx;
  line-height: 1;
  font-weight: 700;
  letter-spacing: -1rpx;
  font-variant-numeric: tabular-nums;
}

.today-metrics > view,
.today-metrics > view:last-child {
  position: relative;
}
.today-metrics > view:not(:last-child)::after {
  content: '';
  position: absolute;
  right: -12rpx;
  top: 13rpx;
  width: 1rpx;
  height: 52rpx;
  background: rgba(255, 255, 255, .13);
}
.latest-session {
  margin-top: 7rpx;
  padding-top: 18rpx;
  border-top: 1rpx solid rgba(255, 255, 255, .13);
}

.week-calendar {
  display: flex;
  align-items: flex-start;
  justify-content: space-between;
}
.calendar-day {
  flex: none;
  width: 60rpx;
}
.calendar-date {
  width: 60rpx;
  height: 84rpx;
  justify-content: flex-start;
}
.date-number,
.calendar-date.today .date-number {
  width: 44rpx;
  height: 44rpx;
  border-radius: 14rpx;
  display: flex;
  align-items: center;
  justify-content: center;
}
.calendar-status { margin-top: 12rpx; }


/* Attention section divider v12 */
.attention-list {
  margin-top: 18rpx;
  padding-top: 18rpx;
  border-top: 1rpx solid rgba(166, 116, 96, .16);
}


/* Weekly metric divider v13 */
.week-main-metric { position: relative; }
.week-main-metric:first-child::after {
  content: '';
  position: absolute;
  right: -16rpx;
  top: 13rpx;
  width: 1rpx;
  height: 52rpx;
  background: rgba(47, 120, 103, .14);
}


/* Normalized weekly calendar layout v14 */
.summary-card {
  padding: 26rpx 26rpx 24rpx;
  border-radius: 24rpx;
}
.week-metrics-row {
  grid-template-columns: repeat(2, minmax(0, 1fr));
  column-gap: 32rpx;
}

.week-calendar {
  margin-top: 28rpx;
  display: flex;
  align-items: flex-start;
  justify-content: space-between;
}
.calendar-day {
  flex: none;
  width: 60rpx;
  min-height: 126rpx;
  align-items: center;
}
.weekday-label {
  flex: none;
  width: 44rpx;
  height: 32rpx;
  display: flex;
  align-items: center;
  justify-content: center;
  line-height: 32rpx;
}
.calendar-date {
  flex: none;
  width: 44rpx;
  height: 84rpx;
  margin-top: 10rpx;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: flex-start;
}
.date-number,
.calendar-date.today .date-number {
  flex: none;
  width: 44rpx;
  height: 44rpx;
}
.calendar-status {
  flex: none;
  margin-top: 16rpx;
}
.calendar-day:first-child .weekday-label,
.calendar-day:first-child .calendar-date {
  align-self: flex-start;
}
.calendar-day:last-child .weekday-label,
.calendar-day:last-child .calendar-date {
  align-self: flex-end;
}
.calendar-legend {
  margin-top: 26rpx;
  margin-bottom: 0;
}

.notification-button { position: relative; width: auto; height: 44rpx; padding: 0; border-radius: 0; background: transparent; color: #17644f; display: flex; align-items: center; gap: 8rpx; box-shadow: none; }
.notification-label { font-size: 28rpx; line-height: 1; font-weight: 500; }
.notification-badge { position: static; min-width: 30rpx; height: 30rpx; padding: 0 7rpx; border: 0; background: #c66e58; font-size: 22rpx; }
</style>
