<template>
  <view class="page-shell library-page">
    <view class="safe-top"></view>
    <view class="top-nav">
      <button class="back-button pressable" @tap="back">‹</button>
      <text class="nav-title">训练动作库</text>
      <view class="nav-spacer"></view>
    </view>

    <view class="library-head">
      <text>8 个常用康复动作</text>
      <text>每个动作使用独立示意图。设备支持的动作可进入实时训练，其余动作保留为训练指导与后续扩展。</text>
    </view>

    <view class="filter-tabs">
      <text :class="{ active: filter === '全部' }" @tap="filter = '全部'">全部</text>
      <text :class="{ active: filter === '上肢' }" @tap="filter = '上肢'">上肢</text>
      <text :class="{ active: filter === '下肢' }" @tap="filter = '下肢'">下肢</text>
    </view>

    <view class="exercise-grid">
      <view
        v-for="item in filteredExercises"
        :key="item.id"
        class="exercise-card card pressable"
        @tap="showExercise(item)"
      >
        <view class="visual-wrap">
          <image class="exercise-image" :src="item.image" mode="aspectFit"></image>
          <text class="exercise-number">{{ String(item.id).padStart(2, '0') }}</text>
          <text class="support-tag" :class="{ later: !item.deviceSupported }">
            {{ item.deviceSupported ? '设备支持' : '动作指导' }}
          </text>
        </view>
        <view class="exercise-copy">
          <text class="exercise-name">{{ item.name }}</text>
          <text class="exercise-goal">{{ item.goal }}</text>
          <view class="exercise-meta">
            <text>{{ item.region }} · {{ item.joint }}</text>
            <text>{{ item.dose }}</text>
          </view>
        </view>
      </view>
    </view>

    <view class="science-note">
      <text class="info-mark">i</text>
      <text>图片仅用于动作科普和界面演示。训练强度与动作选择应遵循医生或康复师建议，不替代诊断或治疗。</text>
    </view>
  </view>
</template>

<script>
import { exerciseCatalog } from '../../data/catalog.js'

export default {
  data() {
    return {
      filter: '全部',
      exercises: exerciseCatalog
    }
  },
  computed: {
    filteredExercises() {
      return this.filter === '全部'
        ? this.exercises
        : this.exercises.filter(item => item.region === this.filter)
    }
  },
  methods: {
    back() {
      uni.navigateBack()
    },
    showExercise(item) {
      uni.previewImage({
        current: item.image,
        urls: [item.image],
        longPressActions: {
          itemList: ['查看动作说明'],
          success: () => this.openExercise(item)
        }
      })
    },
    openExercise(item) {
      uni.showModal({
        title: item.name,
        content: `${item.goal}\n\n建议剂量：${item.dose}\n${item.deviceSupported ? '当前可配合设备进行实时计数。' : '当前作为动作指导，设备算法后续扩展。'}`,
        cancelText: '关闭',
        confirmText: item.deviceSupported ? '去训练' : '知道了',
        success: result => {
          if (result.confirm && item.deviceSupported) {
            uni.navigateTo({ url: '/pages/training/index' })
          }
        }
      })
    }
  }
}
</script>

<style scoped>
.library-page { padding-bottom: 55rpx; }
.library-head { padding: 31rpx 4rpx 25rpx; }
.library-head text { display: block; color: #173d34; font-size: 44rpx; font-weight: 700; }
.library-head text:last-child { width: 630rpx; margin-top: 11rpx; color: #86938e; font-size: 28rpx; line-height: 1.55; font-weight: 400; }
.filter-tabs { padding: 6rpx; border-radius: 21rpx; background: #e5ebe8; display: grid; grid-template-columns: repeat(3, 1fr); }
.filter-tabs text { height: 56rpx; border-radius: 16rpx; color: #87938f; display: flex; align-items: center; justify-content: center; font-size: 28rpx; }
.filter-tabs text.active { background: #fff; color: #24594c; font-weight: 700; box-shadow: 0 4rpx 11rpx rgba(34, 72, 62, .07); }
.exercise-grid { margin-top: 22rpx; display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 15rpx; }
.exercise-card { overflow: hidden; }
.visual-wrap { position: relative; height: 245rpx; background: linear-gradient(145deg, #fbfcfb, #edf3f0); }
.exercise-image { width: 100%; height: 100%; }
.exercise-number { position: absolute; top: 13rpx; left: 13rpx; width: 48rpx; height: 40rpx; border-radius: 13rpx; background: rgba(32, 80, 68, .9); color: #fff; display: flex; align-items: center; justify-content: center; font-size: 24rpx; font-weight: 700; }
.support-tag { position: absolute; right: 12rpx; top: 13rpx; padding: 7rpx 11rpx; border-radius: 999rpx; background: rgba(225, 239, 210, .94); color: #647f3c; font-size: 22rpx; }
.support-tag.later { background: rgba(235, 239, 237, .94); color: #7e8b86; }
.exercise-copy { padding: 18rpx 17rpx 19rpx; }
.exercise-name { display: block; min-height: 61rpx; color: #22473d; font-size: 28rpx; line-height: 1.38; font-weight: 700; }
.exercise-goal { display: -webkit-box; min-height: 48rpx; margin-top: 7rpx; overflow: hidden; color: #8b9792; font-size: 24rpx; line-height: 1.45; -webkit-line-clamp: 2; -webkit-box-orient: vertical; }
.exercise-meta { margin-top: 14rpx; padding-top: 13rpx; border-top: 1rpx solid #e6ece9; color: #557269; font-size: 22rpx; line-height: 1.5; }
.exercise-meta text { display: block; }
.science-note { margin-top: 24rpx; padding: 20rpx; border-radius: 20rpx; background: #e8edea; color: #7d8b86; display: flex; align-items: flex-start; gap: 13rpx; font-size: 24rpx; line-height: 1.55; }
.info-mark { flex: none; width: 27rpx; height: 27rpx; border: 2rpx solid #7e8d87; border-radius: 50%; display: flex; align-items: center; justify-content: center; font-size: 22rpx; }

/* RehabMotion type system v2 */
.library-head { padding-top: 22rpx; padding-bottom: 18rpx; }
.library-head text { font-size: 44rpx; font-weight: 700; line-height: 1.1; }
.library-head text:last-child { font-size: 28rpx; font-weight: 400; line-height: 1.5; }
.filter-tabs text { font-weight: 500; }
.exercise-number { font-weight: 700; font-variant-numeric: tabular-nums; }
.support-tag { font-weight: 500; }
.exercise-name { font-size: 30rpx; font-weight: 600; }
.exercise-goal { font-size: 24rpx; font-weight: 400; }
.exercise-meta { font-size: 22rpx; font-weight: 400; font-variant-numeric: tabular-nums; }
.science-note { font-size: 24rpx; font-weight: 400; }

</style>
