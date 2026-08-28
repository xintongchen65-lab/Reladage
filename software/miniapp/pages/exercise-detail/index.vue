<template>
  <view v-if="exercise" class="page-shell detail-page">
    <view class="safe-top"></view>
    <view class="top-nav">
      <button class="back-button pressable" @tap="back">‹</button>
      <text class="nav-title">动作详情</text>
      <view class="nav-spacer"></view>
    </view>

    <view class="detail-heading">
      <view><text class="detail-region">{{ exercise.region }} · {{ exercise.joint }}</text><text class="detail-title">{{ exercise.name }}</text></view>
      <text class="support-pill" :class="{ guide: !exercise.deviceSupported }">{{ exercise.deviceSupported ? '设备计数' : '动作指导' }}</text>
    </view>

    <view class="demo-card card">
      <video v-if="detail.demoVideo" class="demo-video" :src="detail.demoVideo" controls :show-center-play-btn="true"></video>
      <image v-else class="detail-image" :src="exercise.image" mode="aspectFit"></image>
      <view class="demo-caption"><text>{{ detail.demoVideo ? '动作动画演示' : '高清动作示意图' }}</text><text v-if="!detail.demoVideo">动画接口已预留</text></view>
    </view>

    <view class="summary-card card">
      <text class="summary-title">训练目的</text>
      <text class="summary-copy">{{ detail.summary }}</text>
      <view class="default-dose"><text>参考初始设置</text><text>{{ exercise.dose }}</text></view>
    </view>

    <view class="section-head"><text class="section-title">动作步骤</text><text class="section-link">缓慢、受控</text></view>
    <view class="steps-card card">
      <view v-for="(step, index) in detail.steps" :key="step" class="step-row">
        <text class="step-number">{{ index + 1 }}</text><text>{{ step }}</text>
      </view>
    </view>

    <view class="section-head"><text class="section-title">动作要点</text></view>
    <view class="tips-card card">
      <view v-for="tip in detail.tips" :key="tip"><text>✓</text><text>{{ tip }}</text></view>
    </view>

    <view class="caution-card"><text>!</text><view><text>注意事项</text><text>{{ detail.caution }}</text></view></view>
    <text class="medical-boundary">动作说明仅用于居家训练辅助，不替代医生或康复师的个体化评估。</text>

    <button class="primary-button detail-back pressable" @tap="back">返回训练设置</button>
  </view>
</template>

<script>
import { exerciseCatalog } from '../../data/catalog.js'
import { getExerciseDetail } from '../../data/exercise-details.js'

export default {
  data() { return { exercise: null, detail: null } },
  onLoad(options) {
    this.exercise = exerciseCatalog.find(item => item.id === Number(options.id)) || null
    this.detail = getExerciseDetail(options.id)
    if (!this.exercise || !this.detail) {
      uni.showToast({ title: '未找到动作资料', icon: 'none' })
      setTimeout(() => uni.navigateBack(), 500)
    }
  },
  methods: { back() { uni.navigateBack() } }
}
</script>

<style scoped>
.detail-page { padding-bottom: calc(38rpx + env(safe-area-inset-bottom)); }
.detail-heading { padding: 26rpx 3rpx 20rpx; display: flex; align-items: flex-end; justify-content: space-between; gap: 18rpx; }
.detail-region { display: block; color: #2f7867; font-size: 24rpx; font-weight: 600; }
.detail-title { display: block; margin-top: 8rpx; color: #173d34; font-size: 44rpx; line-height: 1.15; font-weight: 700; }
.support-pill { flex: none; padding: 8rpx 12rpx; border-radius: 999rpx; background: #e4efe8; color: #2f7867; font-size: 22rpx; font-weight: 500; }
.support-pill.guide { background: #edf0ee; color: #78857f; }
.demo-card { overflow: hidden; box-shadow: none; }
.detail-image,.demo-video { width: 100%; height: 510rpx; background: linear-gradient(145deg,#fafcfb,#edf3f0); }
.demo-caption { min-height: 66rpx; padding: 0 21rpx; border-top: 1rpx solid #e6ece9; display: flex; align-items: center; justify-content: space-between; }
.demo-caption text { color: #355a50; font-size: 26rpx; font-weight: 600; }
.demo-caption text:last-child { color: #929d99; font-size: 22rpx; font-weight: 400; }
.summary-card { margin-top: 18rpx; padding: 24rpx; box-shadow: none; }
.summary-title { display: block; color: #23483e; font-size: 28rpx; font-weight: 600; }
.summary-copy { display: block; margin-top: 10rpx; color: #687a74; font-size: 28rpx; line-height: 1.6; font-weight: 400; }
.default-dose { margin-top: 18rpx; padding-top: 17rpx; border-top: 1rpx solid #e6ece9; display: flex; align-items: center; justify-content: space-between; }
.default-dose text { color: #83908b; font-size: 24rpx; font-weight: 400; }
.default-dose text:last-child { color: #1f574a; font-size: 28rpx; font-weight: 600; font-variant-numeric: tabular-nums; }
.steps-card { padding: 0 23rpx; box-shadow: none; }
.step-row { min-height: 112rpx; padding: 18rpx 0; border-bottom: 1rpx solid #e8edea; display: flex; align-items: flex-start; gap: 16rpx; }
.step-row:last-child { border-bottom: 0; }
.step-number { flex: none; width: 42rpx; height: 42rpx; border-radius: 14rpx; background: #174f42; color: #d9ee7f; display: flex; align-items: center; justify-content: center; font-size: 24rpx; font-weight: 700; font-variant-numeric: tabular-nums; }
.step-row > text:last-child { flex: 1; color: #536860; font-size: 28rpx; line-height: 1.55; font-weight: 400; }
.tips-card { padding: 10rpx 23rpx; box-shadow: none; }
.tips-card > view { min-height: 72rpx; display: flex; align-items: center; gap: 13rpx; }
.tips-card > view text:first-child { color: #2f7867; font-size: 26rpx; font-weight: 700; }
.tips-card > view text:last-child { color: #52675f; font-size: 28rpx; font-weight: 400; }
.caution-card { margin-top: 22rpx; padding: 21rpx; border-radius: 21rpx; background: #f7e9e2; display: flex; align-items: flex-start; gap: 14rpx; }
.caution-card > text { flex: none; width: 38rpx; height: 38rpx; border-radius: 50%; background: #d08a6e; color: #fff; display: flex; align-items: center; justify-content: center; font-size: 24rpx; font-weight: 700; }
.caution-card view { flex: 1; }
.caution-card view text { display: block; color: #8e5947; font-size: 28rpx; font-weight: 600; }
.caution-card view text:last-child { margin-top: 7rpx; color: #866d64; font-size: 26rpx; line-height: 1.55; font-weight: 400; }
.medical-boundary { display: block; margin: 14rpx 8rpx 0; color: #929d99; font-size: 22rpx; line-height: 1.5; font-weight: 400; }
.detail-back { margin-top: 24rpx; }
</style>
