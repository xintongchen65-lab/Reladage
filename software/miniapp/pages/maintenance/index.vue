<template>
  <view class="page-shell maintenance-page">
    <secondary-page-header title="设备维护" :subtitle="device.deviceName" @back="back"><template #action><button class="mode-entry header-chip pressable" @tap="openMode">关节模式</button></template></secondary-page-header>
    <view class="device-identity"><view class="device-cube"><view></view></view><view><text>{{ device.deviceName }}</text><text>{{ device.connected ? '已连接' : '未连接' }}</text></view><view class="online-dot"></view></view>

    <view class="section-head"><text class="section-title">IMU 在线状态</text><text class="section-link">4 / 4 在线</text></view>
    <view class="imu-grid">
      <view v-for="item in device.imu" :key="item.id" class="imu-card card"><view class="imu-head"><text>{{ item.id }}</text><view :class="{ offline: !item.online }"></view></view><text class="imu-role">{{ item.role }}</text><view class="signal-row"><text>信号 {{ item.signal }}%</text><text>{{ item.online ? '在线' : '离线' }}</text></view><view class="signal-track"><view :style="{ width: item.signal + '%' }"></view></view></view>
    </view>

    <view class="section-head"><text class="section-title">标定结果</text><text class="section-link pressable" @tap="calibrateAll">全部重标</text></view>
    <view class="calibration-card card">
      <view v-for="item in device.imu" :key="item.id" class="calibration-row"><view class="calibration-check">{{ item.calibrated ? '✓' : '!' }}</view><text>IMU {{ item.id }} 零位标定</text><text :class="{ failed: !item.calibrated }">{{ item.calibrated ? '通过' : '待标定' }}</text></view>
    </view>

    <view class="section-head"><text class="section-title">设备信息</text></view>
    <view class="info-card card">
      <view><text>固件版本</text><text>{{ device.firmware }}</text></view><view><text>设备电量</text><text>{{ device.battery }}%</text></view><view><text>存储介质</text><text>{{ device.storage.medium }}</text></view><view><text>剩余空间</text><text>{{ device.storage.free_gb }} / {{ device.storage.total_gb }} GB</text></view><view><text>最近训练文件</text><text>{{ device.storage.last_file }}</text></view><view><text>存储状态</text><text class="healthy">正常</text></view>
    </view>

    <view class="storage-card"><view class="storage-top"><view><text>存储使用情况</text><text>训练记录约可继续保存 1,200 次</text></view><text>{{ storageUsed }}%</text></view><view class="storage-track"><view :style="{ width: storageUsed + '%' }"></view></view><view class="storage-legend"><text>已用 {{ usedSpace }} GB</text><text>剩余 {{ device.storage.free_gb }} GB</text></view></view>
    <button class="maintenance-button pressable" @tap="checkFirmware"><text>检查固件更新</text><text>{{ device.firmware }} ›</text></button>
  </view>
</template>

<script>
import { getDeviceSnapshot, calibrateImu, checkFirmwareUpdate } from '../../services/device.js'
import SecondaryPageHeader from '../../components/secondary-page-header/secondary-page-header.vue'

export default {
  components: { SecondaryPageHeader },
  data() { return { device: getDeviceSnapshot() } },
  computed: { usedSpace() { return (this.device.storage.total_gb - this.device.storage.free_gb).toFixed(1) }, storageUsed() { return Math.round((this.device.storage.total_gb - this.device.storage.free_gb) / this.device.storage.total_gb * 100) } },
  methods: {
    back() { uni.navigateBack() },
    openMode() { uni.navigateTo({ url: '/pages/mode/index' }) },
    calibrateAll() { uni.showModal({ title: '重新标定全部 IMU？', content: '请按当前关节模式完成佩戴并保持起始位。', confirmText: '开始标定', success: result => { if (!result.confirm) return; calibrateImu('all').then(() => uni.showToast({ title: '标定请求已发送', icon: 'success' })).catch(error => uni.showToast({ title: error.message || '发送失败', icon: 'none' })) } }) },
    checkFirmware() { uni.showLoading({ title: '正在检查' }); checkFirmwareUpdate().then(result => { uni.hideLoading(); uni.showToast({ title: result.updateAvailable ? '发现新版本' : '已是最新版本', icon: 'none' }) }).catch(error => { uni.hideLoading(); uni.showToast({ title: error.message || '检查失败', icon: 'none' }) }) }
  }
}
</script>

<style scoped>
.maintenance-page { padding-bottom: 60rpx; }.device-identity { padding: 30rpx 5rpx 24rpx; display: flex; align-items: center; }.device-cube { width: 91rpx; height: 91rpx; border-radius: 28rpx; background: #174f42; display: flex; align-items: center; justify-content: center; transform: rotate(-4deg); }.device-cube view { width: 36rpx; height: 46rpx; border: 7rpx solid #d9ee7f; border-radius: 8rpx; }.device-identity>view:nth-child(2) { flex: 1; margin-left: 21rpx; }.device-identity>view:nth-child(2) text { display: block; color: #1e4339; font-size: 34rpx; font-weight: 700; }.device-identity>view:nth-child(2) text:last-child { margin-top: 7rpx; color: #8d9894; font-size: 24rpx; font-weight: 400; }.online-dot { width: 18rpx; height: 18rpx; border: 5rpx solid #dce8df; border-radius: 50%; background: #67a06c; }.imu-grid { display: grid; grid-template-columns: repeat(2,1fr); gap: 14rpx; }.imu-card { padding: 21rpx; }.imu-head { display: flex; align-items: center; justify-content: space-between; }.imu-head text { width: 43rpx; height: 43rpx; border-radius: 14rpx; background: #e1eee8; color: #3c7163; display: flex; align-items: center; justify-content: center; font-size: 28rpx; font-weight: 700; }.imu-head view { width: 12rpx; height: 12rpx; border-radius: 50%; background: #62a169; box-shadow: 0 0 0 6rpx rgba(98,161,105,.12); }.imu-head view.offline { background: #c56b57; }.imu-role { display: block; margin-top: 15rpx; color: #2c5047; font-size: 28rpx; font-weight: 600; }.signal-row { margin-top: 12rpx; display: flex; justify-content: space-between; color: #99a39f; font-size: 22rpx; }.signal-track { height: 6rpx; margin-top: 9rpx; border-radius: 6rpx; background: #e5ebe8; overflow: hidden; }.signal-track view { height: 100%; background: #779a64; }.calibration-card { padding: 0 25rpx; }.calibration-row { min-height: 88rpx; border-bottom: 1rpx solid #edf0ee; display: flex; align-items: center; }.calibration-row:last-child { border: 0; }.calibration-check { width: 37rpx; height: 37rpx; border-radius: 13rpx; background: #e0eee3; color: #5c8963; display: flex; align-items: center; justify-content: center; font-size: 24rpx; font-weight: 700; }.calibration-row>text:nth-child(2) { flex: 1; margin-left: 15rpx; color: #35564d; font-size: 28rpx; }.calibration-row>text:last-child { color: #65866a; font-size: 24rpx; }.calibration-row .failed { color: #b06654; }.info-card { padding: 0 26rpx; }.info-card view { min-height: 86rpx; border-bottom: 1rpx solid #edf0ee; display: flex; align-items: center; justify-content: space-between; }.info-card view:last-child { border: 0; }.info-card text { color: #85918d; font-size: 28rpx; }.info-card text:last-child { color: #2d5148; font-weight: 600; }.info-card .healthy { padding: 7rpx 13rpx; border-radius: 999rpx; background: #e2efe4; color: #5c8263; font-size: 24rpx; }.storage-card { margin-top: 20rpx; padding: 26rpx; border-radius: 27rpx; background: #e5eee9; }.storage-top { display: flex; justify-content: space-between; }.storage-top text { display: block; color: #31564c; font-size: 28rpx; font-weight: 600; }.storage-top view text:last-child { margin-top: 5rpx; color: #899690; font-size: 22rpx; font-weight: 400; }.storage-top>text { font-size: 32rpx; }.storage-track { height: 10rpx; margin-top: 21rpx; border-radius: 8rpx; background: #cbd8d2; overflow: hidden; }.storage-track view { height: 100%; background: #2f7867; }.storage-legend { margin-top: 10rpx; display: flex; justify-content: space-between; color: #82908a; font-size: 22rpx; }.maintenance-button { width: 100%; height: 86rpx; margin-top: 20rpx; padding: 0 24rpx; border-radius: 24rpx; background: #fff; display: flex; align-items: center; justify-content: space-between; color: #2c5147; font-size: 28rpx; font-weight: 600; }.maintenance-button text:last-child { color: #8e9995; font-size: 24rpx; font-weight: 400; }.warning-note { margin-top: 20rpx; padding: 20rpx; border-radius: 20rpx; background: #f6e7df; color: #98604f; display: flex; align-items: flex-start; gap: 13rpx; font-size: 24rpx; line-height: 1.55; }.warning-note text:first-child { flex: none; width: 27rpx; height: 27rpx; border: 2rpx solid #a66754; border-radius: 50%; display: flex; align-items: center; justify-content: center; font-size: 22rpx; }

.mode-entry { min-width: 116rpx; }

/* RehabMotion type system v2 */
.device-identity { padding-top: 22rpx; padding-bottom: 18rpx; }
.device-identity > view:nth-child(2) text, .imu-role, .storage-top text, .maintenance-button { font-weight: 600; }
.imu-head text { font-weight: 700; }
.calibration-row > text:last-child,
.info-card text:last-child,
.signal-row { font-variant-numeric: tabular-nums; }
.storage-top > text {
  font-size: 48rpx;
  font-weight: 700;
  line-height: 1.05;
  letter-spacing: -1rpx;
  font-variant-numeric: tabular-nums;
}

</style>
