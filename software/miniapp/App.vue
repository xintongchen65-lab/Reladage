<script>
import { ensureDeviceSeed } from './services/device.js'

export default {
  onLaunch() {
    ensureDeviceSeed()
  }
}
</script>

<style>
page { min-height: 100%; background: #f3f6f4; color: #17352e; font-family: -apple-system, BlinkMacSystemFont, "PingFang SC", "Microsoft YaHei", sans-serif; font-size: 28rpx; }
view, text, button, input, textarea, scroll-view, slider, switch { box-sizing: border-box; }
button { padding: 0; margin: 0; border: 0; line-height: 1; background: transparent; }
button::after { border: 0; }
.safe-top { height: calc(var(--status-bar-height) + 16rpx); }
.page-shell { min-height: 100vh; padding: 0 30rpx 54rpx; }
.page-heading { display: flex; align-items: center; justify-content: space-between; padding: 19rpx 0 30rpx; }
.page-title { display: block; color: #133c32; font-size: 48rpx; line-height: 1.2; font-weight: 700; letter-spacing: -1rpx; }
.page-subtitle { display: block; margin-top: 8rpx; color: #87938f; font-size: 28rpx; }
.card { background: #fff; border: 1rpx solid rgba(19, 71, 57, .06); border-radius: 30rpx; box-shadow: 0 10rpx 30rpx rgba(30, 70, 59, .05); }
.section-head { display: flex; align-items: center; justify-content: space-between; padding: 40rpx 2rpx 20rpx; }
.section-title { color: #1c4037; font-size: 34rpx; font-weight: 700; }
.section-link { color: #267261; font-size: 28rpx; }
.muted { color: #8d9894; }
.pressable { transition: transform .16s ease, opacity .16s ease; }
.pressable:active { transform: scale(.98); opacity: .84; }
.header-chip { flex: none; height: 48rpx; padding: 0 17rpx; border-radius: 999rpx; background: #e7f0eb; color: #245f50; display: inline-flex; align-items: center; justify-content: center; font-size: 26rpx; line-height: 1; font-weight: 500; white-space: nowrap; }
.back-button { width: 68rpx; height: 68rpx; border-radius: 22rpx; background: #fff; color: #214b40; display: flex; align-items: center; justify-content: center; font-size: 44rpx; box-shadow: 0 8rpx 22rpx rgba(30, 68, 58, .06); }
.primary-button { height: 92rpx; border-radius: 25rpx; background: #d9ee7f; color: #173a31; display: flex; align-items: center; justify-content: center; font-size: 32rpx; font-weight: 700; }
.nav-title { color: #1b4036; font-size: 32rpx; font-weight: 700; }
.top-nav { height: 86rpx; display: flex; align-items: center; justify-content: space-between; }
.nav-spacer { width: 68rpx; }
.status-dot { width: 12rpx; height: 12rpx; border-radius: 50%; }

/* RehabMotion type system v2 */
page {
  font-size: 28rpx;
  font-weight: 400;
  line-height: 1.5;
  font-variant-numeric: tabular-nums;
}
.page-shell { padding-bottom: 42rpx; }
.page-heading { padding: 14rpx 0 22rpx; }
.page-title { font-size: 48rpx; font-weight: 700; line-height: 1.1; letter-spacing: -1rpx; }
.page-subtitle { margin-top: 6rpx; font-size: 28rpx; font-weight: 400; line-height: 1.5; }
.section-head { padding: 28rpx 2rpx 16rpx; }
.section-title { font-size: 32rpx; font-weight: 600; }
.section-link { font-size: 28rpx; font-weight: 500; }
.primary-button { font-size: 32rpx; font-weight: 600; }
.nav-title { font-size: 32rpx; font-weight: 600; }
.metric-value {
  font-size: 48rpx;
  font-weight: 700;
  line-height: 1.05;
  letter-spacing: -1rpx;
  font-variant-numeric: tabular-nums;
}
.numeric { font-variant-numeric: tabular-nums; }

</style>
