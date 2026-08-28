<template>
  <view class="role-page">
    <view class="role-safe" :style="safeStyle">
      <view class="role-top" :style="topStyle">
        <view class="brand-mark">R</view>
        <view class="brand-copy">
          <text class="brand-name">RehabMotion</text>
          <text class="brand-caption">居家智能康复</text>
        </view>
      </view>

      <view class="role-main">
        <view class="intro">
          <text class="intro-kicker">选择使用端</text>
          <text class="intro-title">今天想从哪里开始？</text>
          <text class="intro-subtitle">两端共享康复数据，各自呈现最需要的信息。</text>
        </view>

        <view class="role-list">
          <button class="role-card role-card--family pressable" @tap="enterFamily">
            <view class="role-card__glow"></view>
            <view class="role-icon role-icon--family">
              <view class="family-roof"></view>
              <view class="family-body"><view></view></view>
            </view>
            <view class="role-copy">
              <text class="role-label">家属端</text>
              <text class="role-description">查看今日训练、本周坚持情况和关注事项</text>
              <view class="role-tags">
                <text>训练总览</text><text>康复报告</text>
              </view>
            </view>
            <view class="role-arrow"></view>
          </button>

          <button class="role-card role-card--therapist pressable" @tap="enterTherapist">
            <view class="role-icon role-icon--therapist">
              <view class="medical-line medical-line--vertical"></view>
              <view class="medical-line medical-line--horizontal"></view>
            </view>
            <view class="role-copy">
              <text class="role-label">康复师端</text>
              <text class="role-description">管理患者、复核异常并确认训练方案</text>
              <view class="role-tags role-tags--warm">
                <text>患者管理</text><text>方案审核</text>
              </view>
            </view>
            <view class="role-arrow"></view>
          </button>
        </view>

        <view class="privacy-note">
          <view class="privacy-dot"></view>
          <text>仅展示已授权的家庭成员与康复数据</text>
        </view>
      </view>
    </view>
  </view>
</template>

<script>
export default {
  data() {
    return {
      safeStyle: { paddingTop: '24px' },
      topStyle: { paddingRight: '108px' }
    }
  },
  onLoad() {
    this.fitCapsule()
  },
  methods: {
    fitCapsule() {
      let info = {}
      try {
        info = typeof uni.getWindowInfo === 'function'
          ? uni.getWindowInfo()
          : uni.getSystemInfoSync()
      } catch {}
      const statusTop = Number(info.statusBarHeight || (info.safeArea && info.safeArea.top) || 24)
      let capsule = null
      try {
        capsule = typeof uni.getMenuButtonBoundingClientRect === 'function'
          ? uni.getMenuButtonBoundingClientRect()
          : null
      } catch {}
      this.safeStyle = { paddingTop: Math.max(20, capsule && capsule.top ? capsule.top : statusTop + 6) + 'px' }
      this.topStyle = {
        minHeight: Math.max(32, capsule && capsule.height ? capsule.height : 32) + 'px',
        paddingRight: Math.max(100, capsule && capsule.left ? Number(info.windowWidth || 375) - capsule.left + 10 : 100) + 'px'
      }
    },
    enterFamily() {
      uni.switchTab({ url: '/pages/index/index' })
    },
    enterTherapist() {
      uni.reLaunch({ url: '/pages-therapist/home/index' })
    }
  }
}
</script>

<style scoped>
.role-page { position: relative; width: 100%; height: 100vh; overflow: hidden; background: #f3f6f4; color: #173f35; }
.role-page::before { content: ''; position: absolute; top: -220rpx; right: -190rpx; width: 620rpx; height: 620rpx; border-radius: 50%; background: radial-gradient(circle, rgba(217,238,127,.34), rgba(217,238,127,0) 68%); }
.role-page::after { content: ''; position: absolute; bottom: -290rpx; left: -280rpx; width: 700rpx; height: 700rpx; border: 2rpx solid rgba(23,79,66,.07); border-radius: 50%; box-shadow: 0 0 0 72rpx rgba(23,79,66,.025), 0 0 0 148rpx rgba(23,79,66,.016); }
.role-safe { position: relative; z-index: 1; height: 100%; }
.role-top { height: 76rpx; padding: 0 30rpx; display: flex; align-items: center; gap: 16rpx; }
.brand-mark { width: 56rpx; height: 56rpx; flex: none; border-radius: 17rpx 17rpx 17rpx 6rpx; background: #175849; color: #d9ee7f; display: flex; align-items: center; justify-content: center; font-size: 31rpx; line-height: 1; font-weight: 700; box-shadow: 0 10rpx 24rpx rgba(23,79,66,.12); }
.brand-copy { min-width: 0; }
.brand-name, .brand-caption { display: block; }
.brand-name { color: #17483d; font-size: 30rpx; line-height: 1.1; font-weight: 600; letter-spacing: -1rpx; }
.brand-caption { margin-top: 5rpx; color: #8b9893; font-size: 21rpx; line-height: 1; font-weight: 400; }
.role-main { height: calc(100% - 76rpx); padding: 44rpx 30rpx calc(32rpx + env(safe-area-inset-bottom)); display: flex; flex-direction: column; }
.intro-kicker, .intro-title, .intro-subtitle, .role-label, .role-description { display: block; }
.intro-kicker { color: #5b8176; font-size: 24rpx; line-height: 1; font-weight: 500; letter-spacing: 2rpx; }
.intro-title { margin-top: 17rpx; color: #123e33; font-size: 52rpx; line-height: 1.16; font-weight: 700; letter-spacing: -2rpx; }
.intro-subtitle { margin-top: 15rpx; color: #85928d; font-size: 28rpx; line-height: 1.55; font-weight: 400; }
.role-list { display: flex; flex-direction: column; gap: 24rpx; margin-top: 54rpx; }
.role-card { position: relative; width: 100%; min-height: 222rpx; padding: 30rpx 62rpx 30rpx 28rpx; border: 1rpx solid rgba(23,79,66,.07); border-radius: 32rpx; display: flex; align-items: center; text-align: left; overflow: hidden; }
.role-card--family { background: #174f42; color: #fff; box-shadow: 0 18rpx 38rpx rgba(23,79,66,.14); }
.role-card--therapist { background: rgba(255,255,255,.94); color: #173f35; box-shadow: 0 12rpx 32rpx rgba(31,67,57,.06); }
.role-card__glow { position: absolute; top: -86rpx; right: -60rpx; width: 260rpx; height: 260rpx; border: 2rpx solid rgba(217,238,127,.16); border-radius: 50%; box-shadow: 0 0 0 34rpx rgba(217,238,127,.035); }
.role-icon { position: relative; width: 92rpx; height: 92rpx; flex: none; border-radius: 29rpx 29rpx 29rpx 10rpx; display: flex; align-items: center; justify-content: center; }
.role-icon--family { background: #d9ee7f; }
.role-icon--therapist { background: #e8f0eb; }
.family-roof { position: absolute; top: 24rpx; width: 39rpx; height: 39rpx; border-top: 5rpx solid #174f42; border-left: 5rpx solid #174f42; transform: rotate(45deg); border-radius: 4rpx; }
.family-body { position: absolute; bottom: 21rpx; width: 43rpx; height: 34rpx; border: 5rpx solid #174f42; border-top: 0; border-radius: 2rpx 2rpx 8rpx 8rpx; }
.family-body view { position: absolute; right: 12rpx; bottom: 0; width: 11rpx; height: 20rpx; border-radius: 5rpx 5rpx 0 0; background: #174f42; }
.medical-line { position: absolute; border-radius: 999rpx; background: #2f7867; }
.medical-line--vertical { width: 14rpx; height: 48rpx; }
.medical-line--horizontal { width: 48rpx; height: 14rpx; }
.role-copy { min-width: 0; flex: 1; margin-left: 25rpx; }
.role-label { color: inherit; font-size: 36rpx; line-height: 1.2; font-weight: 600; }
.role-description { margin-top: 10rpx; color: #b4cac3; font-size: 26rpx; line-height: 1.45; font-weight: 400; }
.role-card--therapist .role-description { color: #82908b; }
.role-tags { margin-top: 18rpx; display: flex; gap: 12rpx; }
.role-tags text { min-height: 40rpx; padding: 0 14rpx; border-radius: 999rpx; background: rgba(217,238,127,.12); color: #d9ee7f; display: inline-flex; align-items: center; font-size: 22rpx; line-height: 1; font-weight: 500; }
.role-tags--warm text { background: #edf3ef; color: #4f766b; }
.role-arrow { position: absolute; right: 29rpx; width: 17rpx; height: 17rpx; border-top: 3rpx solid currentColor; border-right: 3rpx solid currentColor; transform: rotate(45deg); opacity: .6; }
.privacy-note { margin-top: auto; display: flex; align-items: center; justify-content: center; gap: 11rpx; color: #8d9995; font-size: 23rpx; line-height: 1.4; }
.privacy-dot { width: 10rpx; height: 10rpx; border-radius: 50%; background: #78a260; box-shadow: 0 0 0 5rpx rgba(120,162,96,.10); }
@media (min-width: 700px) {
  .role-main { width: 720px; margin: 0 auto; }
}
</style>
