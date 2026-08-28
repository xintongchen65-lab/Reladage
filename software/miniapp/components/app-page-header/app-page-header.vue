<template>
  <view class="app-page-header" :style="headerStyle">
    <view class="title-row" :style="titleRowStyle">
      <text class="header-title">{{ title }}</text>
    </view>
    <view class="meta-row">
      <text class="header-subtitle">{{ subtitle }}</text>
      <view v-if="$slots.action" class="header-action"><slot name="action" /></view>
    </view>
  </view>
</template>

<script>
export default {
  props: {
    title: { type: String, required: true },
    subtitle: { type: String, default: '' }
  },
  data() {
    return {
      headerStyle: { paddingTop: '24px' },
      titleRowStyle: { minHeight: '32px', paddingRight: '96px' }
    }
  },
  mounted() { this.fitWeChatCapsule() },
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
.app-page-header { padding-right: 30rpx; padding-left: 30rpx; }
.title-row { display: flex; align-items: center; }
.header-title { color: #133c32; font-size: 44rpx; line-height: 1.1; font-weight: 700; letter-spacing: -1rpx; }
.meta-row { min-height: 64rpx; padding: 8rpx 0 14rpx; display: flex; align-items: center; justify-content: space-between; gap: 20rpx; }
.header-subtitle { min-width: 0; flex: 1; overflow: hidden; color: #87938f; font-size: 24rpx; line-height: 1.45; font-weight: 400; text-overflow: ellipsis; white-space: nowrap; }
.header-action { flex: none; min-height: 44rpx; display: flex; align-items: center; }
</style>
