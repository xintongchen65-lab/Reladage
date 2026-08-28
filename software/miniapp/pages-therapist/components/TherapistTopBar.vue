<template>
  <view class="therapist-topbar">
    <view class="therapist-topbar__inner">
      <view class="therapist-brand">
        <view class="therapist-brand__mark">R</view>
        <view class="therapist-brand__copy">
          <text class="therapist-brand__name">RehabMotion</text>
          <text class="therapist-brand__caption">康复师工作台</text>
        </view>
      </view>
      <view class="therapist-avatar">李</view>
    </view>
  </view>
</template>

<style scoped>
.therapist-topbar { height: var(--therapist-header-height); padding-top: var(--therapist-safe-top); background: rgba(246,248,245,.96); }
.therapist-topbar__inner { height: var(--therapist-header-inner-height); padding: 0 var(--therapist-capsule-right) 0 30rpx; display: flex; align-items: center; justify-content: space-between; }
.therapist-brand { min-width: 0; display: flex; align-items: center; gap: 14rpx; }
.therapist-brand__mark { width: 54rpx; height: 54rpx; flex: none; border-radius: 17rpx 17rpx 17rpx 6rpx; background: #175849; color: #d9ee7f; display: flex; align-items: center; justify-content: center; font-size: 30rpx; line-height: 1; font-weight: 700; box-shadow: 0 8rpx 18rpx rgba(23,79,66,.10); }
.therapist-brand__copy { min-width: 0; }
.therapist-brand__name, .therapist-brand__caption { display: block; white-space: nowrap; }
.therapist-brand__name { color: #17483d; font-size: 29rpx; line-height: 1.05; font-weight: 600; letter-spacing: -1rpx; }
.therapist-brand__caption { margin-top: 5rpx; color: #8b9793; font-size: 20rpx; line-height: 1; font-weight: 400; }
.therapist-avatar { width: 50rpx; height: 50rpx; flex: none; border-radius: 50%; background: #e3ede8; color: #2f7867; display: flex; align-items: center; justify-content: center; font-size: 24rpx; line-height: 1; font-weight: 600; }
</style>
