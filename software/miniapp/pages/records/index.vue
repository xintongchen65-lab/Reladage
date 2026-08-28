<template>
  <view class="page-shell records-page">
    <secondary-page-header title="训练记录" :subtitle="memberLabel" @back="back">
      <template #action>
        <picker mode="date" :start="dateBounds.min" :end="dateBounds.max" @change="chooseDate">
          <view class="date-entry header-chip pressable">选择日期</view>
        </picker>
      </template>
    </secondary-page-header>

    <view class="records-summary">
      <view><text>{{ completedDays }}</text><text>次完成</text></view>
      <view><text>{{ partialDays }}</text><text>次中途结束</text></view>
      <view><text>{{ visibleRecords.length }}</text><text>条计划记录</text></view>
    </view>

    <view class="section-head"><text class="section-title">按日期查看</text><text class="section-link">点击查看详情</text></view>
    <view class="record-list card">
      <view v-for="record in visibleRecords" :key="record.date" class="record-row pressable" @tap="openRecord(record.date)">
        <view :class="['record-status', record.status]"><text v-if="record.status === 'completed'">✓</text><text v-else-if="record.status === 'partial'">·</text></view>
        <view class="record-copy">
          <view class="record-title-line"><text>{{ dateLabel(record.date) }}</text><text>{{ statusLabel(record.status) }}</text></view>
          <text class="record-exercises">{{ exerciseNames(record) }}</text>
          <text class="record-meta">{{ record.minutes || 0 }} min · {{ record.completedReps || 0 }} 次动作 · 达标率 {{ record.qualified || 0 }}%</text>
        </view>
        <view class="row-arrow"></view>
      </view>
    </view>

    <view v-if="!visibleRecords.length" class="empty-state card"><text>暂无训练记录</text><text>设备同步训练数据后，会按日期显示在这里。</text></view>
    <view class="records-note">每日记录用于查看具体训练执行；周期变化请前往“报告”查看。</view>
  </view>
</template>

<script>
import SecondaryPageHeader from '../../components/secondary-page-header/secondary-page-header.vue'
import { formatTrainingDate, getTrainingDateBounds, getTrainingRecords, todayISO } from '../../services/training-records.js'
import { getCurrentMemberLabel } from '../../services/members.js'

export default {
  components: { SecondaryPageHeader },
  data() {
    return {
      records: getTrainingRecords(),
      dateBounds: getTrainingDateBounds(),
      memberLabel: getCurrentMemberLabel()
    }
  },
  computed: {
    visibleRecords() {
      return this.records.filter(item => item.planned || item.sessions.length)
    },
    completedDays() {
      return this.visibleRecords.filter(item => item.status === 'completed').length
    },
    partialDays() {
      return this.visibleRecords.filter(item => item.status === 'partial').length
    }
  },
  onShow() {
    this.records = getTrainingRecords()
    this.dateBounds = getTrainingDateBounds()
    this.memberLabel = getCurrentMemberLabel()
  },
  methods: {
    back() {
      uni.navigateBack()
    },
    chooseDate(event) {
      this.openRecord(event.detail.value)
    },
    openRecord(date) {
      uni.navigateTo({ url: `/pages/daily/index?date=${date}` })
    },
    dateLabel(date) {
      return date === todayISO() ? `今天 · ${formatTrainingDate(date)}` : formatTrainingDate(date)
    },
    statusLabel(status) {
      return { completed: '已完成', partial: '中途结束', not_started: '未开始', unplanned: '未安排' }[status] || '待同步'
    },
    exerciseNames(record) {
      const names = record.sessions.map(item => item.name)
      return names.length ? names.join('、') : '当天无训练项目'
    }
  }
}
</script>

<style scoped>
.records-page { padding-bottom: 54rpx; }
.records-summary { padding: 25rpx 26rpx; border-radius: 28rpx; background: #174f42; display: grid; grid-template-columns: repeat(3, 1fr); }
.records-summary > view { padding-left: 20rpx; border-left: 1rpx solid rgba(255,255,255,.13); }
.records-summary > view:first-child { padding-left: 0; border-left: 0; }
.records-summary text { display: block; color: #d9ee7f; font-size: 42rpx; line-height: 1.05; font-weight: 700; font-variant-numeric: tabular-nums; }
.records-summary text:last-child { margin-top: 8rpx; color: #a7c2ba; font-size: 24rpx; font-weight: 400; }
.record-list { padding: 0 23rpx; overflow: hidden; box-shadow: none; }
.record-row { min-height: 150rpx; padding: 21rpx 0; border-bottom: 1rpx solid #e7ece9; display: flex; align-items: center; }
.record-row:last-child { border-bottom: 0; }
.record-status { flex: none; width: 48rpx; height: 48rpx; border: 3rpx solid #c8d1cd; border-radius: 50%; color: #8a9792; display: flex; align-items: center; justify-content: center; font-size: 28rpx; font-weight: 600; }
.record-status.completed { border-color: #2f7867; background: #2f7867; color: #fff; }
.record-status.partial { border-color: #d49b43; color: #d49b43; font-size: 38rpx; }
.record-copy { min-width: 0; flex: 1; margin-left: 17rpx; }
.record-title-line { display: flex; align-items: center; justify-content: space-between; gap: 14rpx; }
.record-title-line text:first-child { min-width: 0; flex: 1; color: #294b42; font-size: 28rpx; font-weight: 600; }
.record-title-line text:last-child { flex: none; color: #71827b; font-size: 24rpx; font-weight: 500; }
.record-exercises { display: block; margin-top: 8rpx; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; color: #62756d; font-size: 26rpx; font-weight: 400; }
.record-meta { display: block; margin-top: 6rpx; color: #939e9a; font-size: 24rpx; font-weight: 400; font-variant-numeric: tabular-nums; }
.row-arrow { flex: none; width: 12rpx; height: 12rpx; margin-left: 14rpx; border-right: 3rpx solid #9ba6a1; border-top: 3rpx solid #9ba6a1; transform: rotate(45deg); }
.empty-state { padding: 54rpx 30rpx; text-align: center; box-shadow: none; }
.empty-state text { display: block; color: #31584e; font-size: 30rpx; font-weight: 600; }
.empty-state text:last-child { margin-top: 8rpx; color: #8b9792; font-size: 26rpx; font-weight: 400; }
.records-note { margin-top: 24rpx; color: #8b9692; font-size: 24rpx; line-height: 1.55; text-align: center; }
</style>
