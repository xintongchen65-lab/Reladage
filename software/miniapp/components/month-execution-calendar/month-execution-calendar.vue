<template>
  <view class="month-calendar">
    <view class="weekday-row"><text v-for="day in weekdays" :key="day">{{ day }}</text></view>
    <view class="date-grid">
      <view v-for="(item, index) in days" :key="item.isoDate || `empty-${index}`" class="date-cell" :class="{ empty: item.empty, future: item.future }" @tap="selectDay(item)">
        <text v-if="!item.empty" class="date-number">{{ item.dateLabel }}</text>
        <view v-if="!item.empty" class="state-mark" :class="item.status"><text v-if="item.status === 'completed'">✓</text><text v-else-if="item.status === 'partial'">◐</text><text v-else-if="item.status === 'not_started'">—</text></view>
      </view>
    </view>
    <view class="legend">
      <view><view class="legend-mark completed">✓</view><text>已完成</text></view>
      <view><view class="legend-mark partial">◐</view><text>中途结束</text></view>
      <view><view class="legend-mark not_started">—</view><text>未开始</text></view>
      <view><view class="legend-mark unplanned"></view><text>未安排</text></view>
    </view>
  </view>
</template>

<script>
export default {
  props: { days: { type: Array, default: () => [] } },
  emits: ['select'],
  data() { return { weekdays: ['一', '二', '三', '四', '五', '六', '日'] } },
  methods: { selectDay(item) { if (item && !item.empty && !item.future) this.$emit('select', item) } }
}
</script>

<style scoped>
.month-calendar { padding: 6rpx 0 0; }
.weekday-row,.date-grid { display: grid; grid-template-columns: repeat(7, 1fr); }
.weekday-row text { height: 58rpx; color: #899590; display: flex; align-items: center; justify-content: center; font-size: 24rpx; font-weight: 500; }
.date-cell { height: 88rpx; display: flex; flex-direction: column; align-items: center; justify-content: flex-start; }
.date-cell.empty { visibility: hidden; }
.date-number { height: 38rpx; color: #294b42; display: flex; align-items: center; justify-content: center; font-size: 26rpx; font-weight: 500; font-variant-numeric: tabular-nums; }
.date-cell.future .date-number { color: #b4bdb9; }
.state-mark { width: 28rpx; height: 28rpx; margin-top: 8rpx; border: 2rpx solid #d5dcda; border-radius: 50%; color: #fff; display: flex; align-items: center; justify-content: center; font-size: 20rpx; line-height: 1; font-weight: 600; }
.state-mark.completed { border-color: #2f7867; background: #2f7867; }
.state-mark.partial { border-color: #d9a047; background: #fff7e8; color: #bc7c1d; }
.state-mark.not_started { border-color: #d7deda; background: #f3f5f4; color: #98a29e; }
.state-mark.scheduled,.state-mark.unplanned { background: #fff; }
.date-cell.future .state-mark { opacity: .55; }
.legend { margin-top: 22rpx; padding-top: 22rpx; border-top: 1rpx solid #e8ecea; display: flex; align-items: center; justify-content: space-between; }
.legend>view { display: flex; align-items: center; gap: 7rpx; }
.legend-mark { width: 25rpx; height: 25rpx; border: 2rpx solid #d5dcda; border-radius: 50%; color: #fff; display: flex; align-items: center; justify-content: center; font-size: 17rpx; line-height: 1; font-weight: 600; }
.legend-mark.completed { border-color: #2f7867; background: #2f7867; }.legend-mark.partial { border-color: #d9a047; background: #fff7e8; color: #bc7c1d; }.legend-mark.not_started { background: #f3f5f4; color: #98a29e; }.legend text { color: #7f8c87; font-size: 22rpx; font-weight: 400; }
</style>
