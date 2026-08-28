<template>
  <view class="assessment-page page-shell">
    <view class="safe-top"></view>
    <view class="assessment-nav">
      <button class="back-button pressable" @tap="back">‹</button>
      <text>疼痛自评</text>
      <view class="nav-spacer"></view>
    </view>

    <view class="assessment-head">
      <text class="assessment-title">此刻，你感觉怎么样？</text>
      <text class="assessment-copy">请按照真实感受选择，帮助康复师及时调整训练强度。</text>
    </view>

    <view class="score-card card">
      <text class="score-number" :style="{ color: scoreColor }">{{ pain }}</text>
      <text class="score-of">/ 10</text>
      <text class="score-label">{{ painLabel }}</text>
      <slider class="pain-slider" :value="pain" min="0" max="10" step="1" activeColor="#D0E878" backgroundColor="#E6EAE7" block-color="#174F42" block-size="28" @changing="changePain" @change="changePain" />
      <view class="scale-copy"><text>无疼痛</text><text>难以忍受</text></view>
      <view class="face-row">
        <view v-for="face in faces" :key="face.value" class="face-item" :class="{ active: pain === face.value }" @tap="pain = face.value">
          <text class="face">{{ face.icon }}</text><text>{{ face.text }}</text>
        </view>
      </view>
    </view>

    <view class="form-section">
      <text class="form-title">疼痛主要在哪里？</text>
      <view class="area-grid">
        <view v-for="item in areas" :key="item" class="area-chip pressable" :class="{ selected: area === item }" @tap="area = item">{{ item }}</view>
      </view>
    </view>

    <view class="form-section">
      <text class="form-title">疼痛更接近哪一种？</text>
      <view class="type-row">
        <view v-for="item in painTypes" :key="item" class="type-chip pressable" :class="{ selected: painType === item }" @tap="painType = item">{{ item }}</view>
      </view>
    </view>

    <view v-if="pain >= 7" class="warning-card"><text class="warning-mark">!</text><text>疼痛程度较高，建议暂停今日训练并尽快联系康复师。</text></view>

    <button class="primary-button submit-button pressable" @tap="submit">保存评估</button>
  </view>
</template>

<script>
import { getAssessment, saveAssessment } from '../../utils/storage.js'

export default {
  data() {
    const current = getAssessment()
    return {
      pain: current.pain,
      area: current.area,
      painType: current.painType || '酸胀',
      areas: ['膝关节前侧', '膝关节内侧', '膝关节外侧', '大腿前侧', '小腿后侧', '其他'],
      painTypes: ['酸胀', '刺痛', '牵拉感', '灼热', '麻木'],
      faces: [{ value: 0, icon: '◡', text: '轻松' }, { value: 3, icon: '—', text: '轻微' }, { value: 6, icon: '⌒', text: '明显' }, { value: 9, icon: '﹏', text: '严重' }]
    }
  },
  computed: {
    painLabel() {
      if (this.pain === 0) return '没有疼痛'
      if (this.pain <= 3) return '轻微疼痛，不影响活动'
      if (this.pain <= 6) return '中度疼痛，需要留意'
      return '重度疼痛，建议停止训练'
    },
    scoreColor() { return this.pain <= 3 ? '#5E8B52' : this.pain <= 6 ? '#C28839' : '#C65E52' }
  },
  methods: {
    changePain(e) { this.pain = Number(e.detail.value) },
    back() { uni.navigateBack() },
    submit() {
      saveAssessment({ pain: this.pain, area: this.area, painType: this.painType, updatedAt: '刚刚' })
      uni.showToast({ title: '评估已保存', icon: 'success' })
      setTimeout(() => uni.navigateBack(), 700)
    }
  }
}
</script>

<style scoped>
.assessment-page { padding-bottom: calc(42rpx + env(safe-area-inset-bottom)); }
.assessment-nav { height: 88rpx; display: flex; align-items: center; justify-content: space-between; color: #1e4239; font-size: 32rpx; font-weight: 700; }.nav-spacer { width: 70rpx; }
.assessment-head { padding: 36rpx 4rpx 32rpx; }.assessment-title { display: block; color: #173d34; font-size: 48rpx; font-weight: 700; }.assessment-copy { display: block; width: 600rpx; margin-top: 15rpx; color: #84918c; font-size: 30rpx; line-height: 1.6; }
.score-card { padding: 39rpx 34rpx 30rpx; text-align: center; }.score-number { font-size: 90rpx; line-height: 1; font-weight: 700; }.score-of { margin-left: 5rpx; color: #9ba5a1; font-size: 34rpx; }.score-label { display: block; margin-top: 13rpx; color: #597068; font-size: 28rpx; }
.pain-slider { margin: 38rpx 4rpx 0; }.scale-copy { padding: 0 8rpx; display: flex; justify-content: space-between; color: #a0aaa6; font-size: 24rpx; }
.face-row { margin-top: 30rpx; display: flex; justify-content: space-between; }.face-item { width: 103rpx; padding: 13rpx 0; border-radius: 18rpx; display: flex; flex-direction: column; align-items: center; gap: 4rpx; color: #9aa5a1; font-size: 24rpx; }.face-item.active { background: #eff4e1; color: #60783c; }.face { color: #526d64; font-size: 40rpx; }
.form-section { padding-top: 37rpx; }.form-title { display: block; margin-bottom: 20rpx; color: #25483f; font-size: 32rpx; font-weight: 700; }.area-grid { display: grid; grid-template-columns: repeat(2,1fr); gap: 15rpx; }.area-chip,.type-chip { height: 72rpx; border: 2rpx solid #e5eae7; border-radius: 21rpx; background: #fff; display: flex; align-items: center; justify-content: center; color: #687a74; font-size: 28rpx; }.area-chip.selected,.type-chip.selected { border-color: #87a25a; background: #edf4d9; color: #365d50; font-weight: 600; }
.type-row { display: flex; flex-wrap: wrap; gap: 13rpx; }.type-chip { height: 62rpx; padding: 0 22rpx; }
.warning-card { margin-top: 28rpx; padding: 22rpx; border-radius: 20rpx; background: #fce7df; color: #a25b4d; display: flex; align-items: center; gap: 15rpx; font-size: 28rpx; line-height: 1.5; }.warning-mark { flex: none; width: 31rpx; height: 31rpx; border: 2rpx solid #ba6657; border-radius: 50%; display: flex; align-items: center; justify-content: center; }
.submit-button { margin-top: 39rpx; }
</style>

