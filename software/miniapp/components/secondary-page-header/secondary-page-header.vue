<template>
  <view class="secondary-header" :class="{ dark }" :style="headerStyle">
    <view class="secondary-title-row" :style="titleRowStyle">
      <button v-if="showBack" class="secondary-back pressable" @tap="$emit('back')">‹</button>
      <text class="secondary-title">{{ title }}</text>
    </view>
    <view v-if="subtitle || $slots.action" class="secondary-meta-row">
      <text class="secondary-subtitle">{{ subtitle }}</text>
      <view v-if="$slots.action" class="secondary-action"><slot name="action" /></view>
    </view>
  </view>
</template>

<script>
export default {
  props: {
    title: { type: String, required: true },
    subtitle: { type: String, default: '' },
    showBack: { type: Boolean, default: true },
    dark: { type: Boolean, default: false }
  },
  emits: ['back'],
  data() {
    return {
      headerStyle: { paddingTop: '24px' },
      titleRowStyle: { minHeight: '32px', paddingRight: '96px' }
    }
  },
  mounted() {
    this.fitWeChatCapsule()
  },
  methods: {
    fitWeChatCapsule() {
      const system = uni.getSystemInfoSync()
      const fallbackTop = system.statusBarHeight || 24
      if (typeof uni.getMenuButtonBoundingClientRect !== 'function') {
        this.headerStyle = { paddingTop: `${fallbackTop}px` }
        return
      }
      const capsule = uni.getMenuButtonBoundingClientRect()
      if (!capsule || !capsule.height) {
        this.headerStyle = { paddingTop: `${fallbackTop}px` }
        return
      }
      this.headerStyle = { paddingTop: `${capsule.top}px` }
      this.titleRowStyle = {
        minHeight: `${capsule.height}px`,
        paddingRight: `${Math.max(96, system.windowWidth - capsule.left + 10)}px`
      }
    }
  }
}
</script>

<style scoped>
.secondary-header { padding-bottom: 22rpx; }
.secondary-title-row { display: flex; align-items: center; gap: 15rpx; }
.secondary-back { flex: none; width: 58rpx; height: 58rpx; border-radius: 18rpx; background: #fff; color: #174f42; display: flex; align-items: center; justify-content: center; font-size: 44rpx; line-height: 1; font-weight: 500; box-shadow: 0 6rpx 18rpx rgba(28, 68, 57, .05); }
.secondary-title { min-width: 0; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; color: #173f35; font-size: 34rpx; line-height: 1.2; font-weight: 600; }
.secondary-meta-row { min-height: 48rpx; margin-top: 10rpx; padding-left: 73rpx; display: flex; align-items: center; justify-content: space-between; gap: 18rpx; }
.secondary-subtitle { min-width: 0; flex: 1; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; color: #87938f; font-size: 26rpx; font-weight: 400; }
.secondary-action { flex: none; min-height: 44rpx; display: flex; align-items: center; }
.dark .secondary-back { background: rgba(255, 255, 255, .08); color: #fff; box-shadow: none; }
.dark .secondary-title { color: #fff; }
.dark .secondary-subtitle { color: #9ebbb2; }
</style>
