<template>
  <view class="mini-trend" aria-label="四周动作达标率趋势图">
    <view class="plot">
      <view class="guide-line"></view>
      <view class="line line-one"></view>
      <view class="line line-two"></view>
      <view class="line line-three"></view>
      <view
        v-for="(value, index) in values"
        :key="`${value}-${index}`"
        class="point-group"
        :class="`point-${index + 1}`"
      >
        <text class="point-value">{{ value }}%</text>
        <view class="point"></view>
      </view>
    </view>
    <view class="chart-labels">
      <text v-for="label in labels" :key="label">{{ label }}</text>
    </view>
  </view>
</template>

<script>
export default {
  name: 'ReportMiniTrend',
  props: {
    values: { type: Array, default: () => [77, 81, 84, 87] },
    labels: { type: Array, default: () => ['第1周', '第2周', '第3周', '第4周'] }
  }
}
</script>

<style scoped>
.mini-trend{height:214rpx;margin-top:22rpx;overflow:hidden}.plot{position:relative;height:154rpx;margin:0 16rpx}.guide-line{position:absolute;left:0;right:0;bottom:18rpx;height:1rpx;background:#dde5e1}.line{position:absolute;z-index:1;height:4rpx;border-radius:4rpx;background:#2c846b;transform-origin:left center}.line-one{left:4%;top:102rpx;width:31.8%;transform:rotate(-7.1deg)}.line-two{left:35%;top:78rpx;width:31.5%;transform:rotate(-5.3deg)}.line-three{left:66%;top:60rpx;width:30.6%;transform:rotate(-5.1deg)}.point-group{position:absolute;z-index:2;display:flex;flex-direction:column;align-items:center;transform:translateX(-50%)}.point-1{left:4%;top:73rpx}.point-2{left:35%;top:49rpx}.point-3{left:66%;top:31rpx}.point-4{left:96%;top:14rpx}.point-value{margin-bottom:8rpx;color:#50615b;font-size:24rpx;line-height:1;font-weight:500;font-variant-numeric:tabular-nums}.point{width:15rpx;height:15rpx;box-sizing:border-box;border:4rpx solid #25765f;border-radius:50%;background:#f5f6f3}.chart-labels{display:grid;grid-template-columns:repeat(4,1fr);align-items:center;margin-top:12rpx;color:#84908b}.chart-labels text{font-size:24rpx;line-height:1.2;font-weight:400;text-align:center}
</style>
