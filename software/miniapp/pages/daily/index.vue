<template>
  <view class="page-shell daily-page">
    <secondary-page-header title="每日训练详情" :subtitle="memberLabel" @back="back" />

    <view class="date-switcher">
      <button class="date-arrow pressable" :disabled="!canOlder" @tap="shiftDay(-1)"><view></view></button>
      <picker mode="date" :value="selectedDate" :start="minDate" :end="maxDate" @change="chooseDate">
        <view class="date-copy pressable">
          <text>{{ relativeDateLabel }}</text>
          <text>{{ formattedDate }}</text>
          <text class="date-picker-link">选择其他日期</text>
        </view>
      </picker>
      <button class="date-arrow next pressable" :disabled="!canNewer" @tap="shiftDay(1)"><view></view></button>
    </view>

    <template v-if="currentRecord">
      <view class="daily-summary">
        <view class="summary-head">
          <view><text>DAILY</text><text>当日训练完成情况</text></view>
          <text :class="['status-pill', currentRecord.status]">{{ dayStatusLabel }}</text>
        </view>

        <view class="summary-progress">
          <view><text>{{ completedTaskCount }}</text><text>/ {{ sessions.length }}</text><text>项</text></view>
          <view><text>计划完成</text><text>{{ completionPercent }}%</text></view>
        </view>
        <view class="progress-track"><view :style="{ width: completionPercent + '%' }"></view></view>

        <view class="summary-metrics">
          <view><text>累计训练</text><view><text>{{ currentRecord.minutes || 0 }}</text><text>min</text></view></view>
          <view><text>完成动作</text><view><text>{{ currentRecord.completedReps || 0 }}</text><text>次</text></view></view>
          <view><text>动作达标率</text><view><text>{{ currentRecord.qualified || 0 }}</text><text>%</text></view></view>
        </view>
      </view>

      <view class="section-head project-heading">
        <text class="section-title">训练项目</text>
        <text class="section-link">共 {{ sessions.length }} 项</text>
      </view>
      <view class="session-list card">
        <view v-for="item in sessions" :key="item.id" class="session-row">
          <view :class="['session-mark', item.status]"><text v-if="item.status === 'completed'">✓</text><text v-else-if="item.status === 'partial'">·</text></view>
          <view class="session-copy">
            <view class="session-title"><text>{{ item.name }}</text><text>{{ statusText(item.status) }}</text></view>
            <view class="session-result">
              <text>完成 {{ item.completedReps }} / {{ item.plannedReps }} 次</text>
              <text v-if="item.completedReps">达标 {{ item.qualifiedReps }} 次</text>
              <text v-if="item.maxAngle">最大角度 {{ item.maxAngle }}°</text>
            </view>
          </view>
        </view>
      </view>

      <view class="section-head feedback-heading">
        <text class="section-title">训练后反馈</text>
      </view>
      <view class="feedback-card card">
        <view><text>训练强度</text><text>{{ intensityText }}</text></view>
        <view><text>疼痛反馈</text><text :class="{ warm: Number(currentRecord.painAfter || 0) >= 3 }">{{ painText }}</text></view>
        <view><text>训练中断</text><text>{{ currentRecord.interrupted || 0 }} 次</text></view>
        <view v-if="primaryIssue" class="feedback-note"><text>动作提示</text><text>{{ primaryIssue }}</text></view>
      </view>
    </template>

    <view v-else class="empty-day card">
      <view class="empty-mark"><view></view></view>
      <text>当天没有训练记录</text>
      <text>这一天未安排训练，或设备尚未同步训练数据。</text>
    </view>

    <view class="medical-note">本页展示居家训练执行记录，不替代医生诊断或治疗建议。</view>
  </view>
</template>

<script>
import { getTrainingRecords, todayISO } from '../../services/training-records.js'
import SecondaryPageHeader from '../../components/secondary-page-header/secondary-page-header.vue'
import { getCurrentMemberLabel } from '../../services/members.js'

const pad = value => String(value).padStart(2, '0')
const toDate = value => new Date(value + 'T00:00:00')
const toDateString = date => `${date.getFullYear()}-${pad(date.getMonth() + 1)}-${pad(date.getDate())}`

export default {
  components: { SecondaryPageHeader },
  data() {
    return {
      records: getTrainingRecords(),
      selectedDate: '',
      memberLabel: getCurrentMemberLabel()
    }
  },
  computed: {
    sortedRecords() {
      return [...this.records].sort((a, b) => String(b.date).localeCompare(String(a.date)))
    },
    maxDate() {
      return todayISO()
    },
    minDate() {
      return this.sortedRecords[this.sortedRecords.length - 1]?.date || this.maxDate
    },
    currentRecord() {
      return this.records.find(item => item.date === this.selectedDate) || null
    },
    canOlder() {
      return Boolean(this.selectedDate && this.selectedDate > this.minDate)
    },
    canNewer() {
      return Boolean(this.selectedDate && this.selectedDate < this.maxDate)
    },
    relativeDateLabel() {
      return this.selectedDate === todayISO() ? '今天' : '历史记录'
    },
    formattedDate() {
      if (!this.selectedDate) return ''
      const date = toDate(this.selectedDate)
      const weekdays = ['星期日', '星期一', '星期二', '星期三', '星期四', '星期五', '星期六']
      return `${date.getMonth() + 1}月${date.getDate()}日 · ${weekdays[date.getDay()]}`
    },
    sessions() {
      return Array.isArray(this.currentRecord?.sessions) ? this.currentRecord.sessions : []
    },
    completedTaskCount() {
      return this.sessions.filter(item => item.status === 'completed').length
    },
    completionPercent() {
      if (!this.sessions.length) return 0
      return Math.round(this.completedTaskCount / this.sessions.length * 100)
    },
    dayStatusLabel() {
      if (this.completedTaskCount === this.sessions.length && this.sessions.length) return '已完成'
      if (this.sessions.some(item => item.completedReps > 0)) return '部分完成'
      return this.currentRecord.planned ? '未开始' : '未安排'
    },
    intensityText() {
      const fatigue = Number(this.currentRecord?.fatigue || 0)
      if (!fatigue) return '未填写'
      if (fatigue <= 2) return '合适'
      if (fatigue === 3) return '稍有疲劳'
      return '强度偏高'
    },
    painText() {
      const pain = Number(this.currentRecord?.painAfter || 0)
      if (!pain) return '无明显疼痛'
      if (pain <= 2) return '轻微'
      if (pain <= 4) return '需要关注'
      return '建议暂停并咨询'
    },
    primaryIssue() {
      const reasons = this.currentRecord?.reasons || {}
      if (Number(reasons.paceFast || 0)) return `动作过快 ${reasons.paceFast} 次，下次训练可适当放慢节奏。`
      if (Number(reasons.holdShort || 0)) return `保持时间不足 ${reasons.holdShort} 次。`
      if (Number(reasons.romLow || 0)) return `动作幅度不足 ${reasons.romLow} 次。`
      return ''
    }
  },
  onLoad(options) {
    const requested = options && options.date
    this.selectedDate = requested && requested >= this.minDate && requested <= this.maxDate ? requested : this.maxDate
  },
  onShow() {
    this.records = getTrainingRecords()
    this.memberLabel = getCurrentMemberLabel()
    if (!this.selectedDate) this.selectedDate = this.maxDate
  },
  methods: {
    back() {
      uni.navigateBack()
    },
    chooseDate(event) {
      this.selectedDate = event.detail.value
    },
    shiftDay(offset) {
      const date = toDate(this.selectedDate)
      date.setDate(date.getDate() + offset)
      const value = toDateString(date)
      if (value >= this.minDate && value <= this.maxDate) this.selectedDate = value
    },
    statusText(status) {
      if (status === 'completed') return '已完成'
      if (status === 'partial') return '部分完成'
      return '未开始'
    }
  }
}
</script>

<style scoped>
.daily-page { padding-bottom: 54rpx; }
.daily-nav { min-height: 92rpx; display: flex; align-items: center; gap: 16rpx; }
.nav-copy { min-width: 0; flex: 1; }
.nav-copy text { display: block; color: #173f35; font-size: 34rpx; line-height: 1.2; font-weight: 600; }
.nav-copy text:last-child { margin-top: 5rpx; color: #8a9792; font-size: 24rpx; font-weight: 400; }
.date-switcher { min-height: 134rpx; margin: 12rpx 0 18rpx; padding: 0 10rpx; display: flex; align-items: center; justify-content: space-between; }
.date-arrow { width: 62rpx; height: 62rpx; border-radius: 20rpx; background: #fff; display: flex; align-items: center; justify-content: center; box-shadow: 0 7rpx 18rpx rgba(28, 68, 57, .05); }
.date-arrow[disabled] { opacity: .32; }
.date-arrow view { width: 14rpx; height: 14rpx; border-left: 3rpx solid #315c50; border-bottom: 3rpx solid #315c50; transform: rotate(45deg); }
.date-arrow.next view { transform: rotate(225deg); }
.date-copy { text-align: center; }
.date-copy text { display: block; color: #174f42; font-size: 34rpx; line-height: 1.2; font-weight: 600; }
.date-copy text:last-child { margin-top: 7rpx; color: #81908a; font-size: 26rpx; font-weight: 400; }
.date-copy .date-picker-link { margin-top: 9rpx; color: #2f7867; font-size: 24rpx; line-height: 1.2; font-weight: 500; }
.daily-summary { padding: 28rpx; border-radius: 30rpx; background: #174f42; color: #fff; box-shadow: 0 13rpx 30rpx rgba(23, 79, 66, .11); }
.summary-head { display: flex; align-items: flex-start; justify-content: space-between; }
.summary-head > view text { display: block; color: #9ebbb2; font-size: 22rpx; line-height: 1; font-weight: 600; letter-spacing: 2rpx; }
.summary-head > view text:last-child { margin-top: 10rpx; color: #fff; font-size: 30rpx; line-height: 1.25; letter-spacing: 0; }
.status-pill { padding: 9rpx 14rpx; border-radius: 999rpx; background: rgba(217, 238, 127, .13); color: #d9ee7f; font-size: 24rpx; font-weight: 500; }
.summary-progress { margin-top: 27rpx; display: flex; align-items: flex-end; justify-content: space-between; }
.summary-progress > view:first-child { display: flex; align-items: baseline; }
.summary-progress > view:first-child text:first-child { font-size: 60rpx; line-height: 1; font-weight: 700; letter-spacing: -2rpx; }
.summary-progress > view:first-child text:nth-child(2) { margin-left: 7rpx; color: #d9ee7f; font-size: 34rpx; font-weight: 600; }
.summary-progress > view:first-child text:last-child { margin-left: 7rpx; color: #a7c2ba; font-size: 24rpx; font-weight: 400; }
.summary-progress > view:last-child { padding-bottom: 3rpx; text-align: right; }
.summary-progress > view:last-child text { display: block; color: #a7c2ba; font-size: 24rpx; font-weight: 400; }
.summary-progress > view:last-child text:last-child { margin-top: 5rpx; color: #d9ee7f; font-size: 32rpx; line-height: 1; font-weight: 700; }
.progress-track { height: 9rpx; margin-top: 17rpx; overflow: hidden; border-radius: 9rpx; background: rgba(255, 255, 255, .13); }
.progress-track view { height: 100%; border-radius: 9rpx; background: #d9ee7f; }
.summary-metrics { margin-top: 25rpx; display: grid; grid-template-columns: repeat(3, 1fr); }
.summary-metrics > view { padding-left: 20rpx; border-left: 1rpx solid rgba(255, 255, 255, .13); }
.summary-metrics > view:first-child { padding-left: 0; border-left: 0; }
.summary-metrics > view > text { display: block; color: #9ebbb2; font-size: 24rpx; font-weight: 400; }
.summary-metrics > view > view { margin-top: 8rpx; display: flex; align-items: baseline; }
.summary-metrics > view > view text:first-child { color: #fff; font-size: 44rpx; line-height: 1.05; font-weight: 700; letter-spacing: -1rpx; }
.summary-metrics > view > view text:last-child { margin-left: 6rpx; color: #b2c8c1; font-size: 22rpx; font-weight: 500; }
.project-heading, .feedback-heading { padding-top: 30rpx; }
.session-list { padding: 0 23rpx; overflow: hidden; box-shadow: none; }
.session-row { min-height: 134rpx; padding: 20rpx 0; border-bottom: 1rpx solid #e7ece9; display: flex; align-items: center; }
.session-row:last-child { border-bottom: 0; }
.session-mark { flex: none; width: 48rpx; height: 48rpx; border: 3rpx solid #c8d1cd; border-radius: 50%; color: #fff; display: flex; align-items: center; justify-content: center; font-size: 24rpx; font-weight: 600; }
.session-mark.completed { border-color: #2f7867; background: #2f7867; }
.session-mark.partial { border-color: #d49b43; color: #d49b43; font-size: 38rpx; }
.session-copy { min-width: 0; flex: 1; margin-left: 17rpx; }
.session-title { display: flex; align-items: center; justify-content: space-between; gap: 15rpx; }
.session-title text:first-child { min-width: 0; flex: 1; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; color: #294b42; font-size: 28rpx; font-weight: 600; }
.session-title text:last-child { flex: none; color: #7f8e88; font-size: 24rpx; font-weight: 500; }
.session-result { margin-top: 9rpx; display: flex; flex-wrap: wrap; gap: 7rpx 18rpx; }
.session-result text { color: #85928d; font-size: 24rpx; font-weight: 400; font-variant-numeric: tabular-nums; }
.feedback-card { padding: 0 23rpx; box-shadow: none; }
.feedback-card > view { min-height: 80rpx; border-bottom: 1rpx solid #e8edea; display: flex; align-items: center; justify-content: space-between; gap: 20rpx; }
.feedback-card > view:last-child { border-bottom: 0; }
.feedback-card > view text { color: #6f8079; font-size: 28rpx; font-weight: 400; }
.feedback-card > view text:last-child { color: #2f5e52; font-weight: 600; }
.feedback-card > view text.warm { color: #b0634d; }
.feedback-card .feedback-note { min-height: 102rpx; align-items: flex-start; padding: 17rpx 0; }
.feedback-note text:last-child { max-width: 72%; line-height: 1.5; text-align: right; }
.empty-day { margin-top: 20rpx; padding: 68rpx 34rpx; text-align: center; box-shadow: none; }
.empty-mark { width: 78rpx; height: 78rpx; margin: 0 auto 20rpx; border-radius: 24rpx; background: #e7efeb; display: flex; align-items: center; justify-content: center; }
.empty-mark view { width: 28rpx; height: 28rpx; border: 4rpx solid #729080; border-radius: 50%; }
.empty-day > text { display: block; color: #31584e; font-size: 30rpx; font-weight: 600; }
.empty-day > text:last-child { margin-top: 9rpx; color: #8b9792; font-size: 26rpx; line-height: 1.5; font-weight: 400; }
.medical-note { margin-top: 24rpx; color: #8b9692; font-size: 24rpx; line-height: 1.55; text-align: center; }
</style>
