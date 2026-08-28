<template>
  <view class="page-shell mode-page">
    <secondary-page-header title="关节模式" subtitle="切换训练关节与佩戴方式" @back="back" />
    <view class="mode-head"><text>选择本次训练关节</text><text>切换后将写入设备配置，并提示正确佩戴位置。</text></view>

    <view class="mode-list">
      <view v-for="item in modes" :key="item.id" class="mode-card card pressable" :class="{ selected: selected === item.id, disabled: !item.enabled }" @tap="selectMode(item)">
        <view class="mode-symbol" :style="{ backgroundColor: item.color, color: item.accent }"><text>{{ item.icon }}</text><view class="joint-dot"></view></view>
        <view class="mode-copy"><view class="mode-name-row"><text class="mode-name">{{ item.name }}</text><text v-if="!item.enabled" class="later-tag">后续扩展</text></view><text class="mode-subtitle">{{ item.subtitle }}</text><text class="mode-state">{{ item.enabled ? '设备已支持' : '固件与佩戴方案待确认' }}</text></view>
        <view class="radio"><view v-if="selected === item.id"></view></view>
      </view>
    </view>

    <view class="wear-card card">
      <view class="wear-head"><view class="wear-icon">⌁</view><view><text>佩戴方式</text><text>{{ current.name }} · {{ current.subtitle }}</text></view></view>
      <view class="wear-body"><view v-for="(tip, index) in current.wear" :key="tip" class="wear-step"><text>{{ index + 1 }}</text><text>{{ tip }}</text></view></view>
      <view v-if="!current.enabled" class="extension-note">当前仅展示规划，不会向设备下发手腕模式。</view>
    </view>

    <button class="primary-button confirm pressable" :class="{ disabled: !current.enabled }" @tap="confirmMode">{{ current.enabled ? '保存并写入设备' : '该模式暂未开放' }}</button>
  </view>
</template>

<script>
import { jointModes } from '../../data/catalog.js'
import { getDeviceSnapshot, switchJointMode } from '../../services/device.js'
import SecondaryPageHeader from '../../components/secondary-page-header/secondary-page-header.vue'

export default {
  components: { SecondaryPageHeader },
  data() { return { modes: jointModes, selected: getDeviceSnapshot().joint_id || 'elbow' } },
  computed: { current() { return this.modes.find(item => item.id === this.selected) || this.modes[0] } },
  methods: {
    back() { uni.navigateBack() },
    selectMode(item) { this.selected = item.id; if (!item.enabled) uni.showToast({ title: '可查看佩戴规划，暂不能写入设备', icon: 'none' }) },
    confirmMode() {
      if (!this.current.enabled) return
      uni.showLoading({ title: '正在写入' })
      switchJointMode(this.current.id).then(() => {
        uni.hideLoading(); uni.showToast({ title: '设备模式已切换', icon: 'success' }); setTimeout(() => uni.navigateBack(), 700)
      }).catch(error => { uni.hideLoading(); uni.showToast({ title: error.message, icon: 'none' }) })
    }
  }
}
</script>

<style scoped>
.mode-page { padding-bottom: calc(44rpx + env(safe-area-inset-bottom)); }.mode-head { padding: 34rpx 4rpx 29rpx; }.mode-head text { display: block; color: #173d34; font-size: 44rpx; font-weight: 700; }.mode-head text:last-child { width: 600rpx; margin-top: 12rpx; color: #85928d; font-size: 28rpx; line-height: 1.55; font-weight: 400; }.mode-list { display: flex; flex-direction: column; gap: 16rpx; }.mode-card { min-height: 152rpx; padding: 22rpx; display: flex; align-items: center; }.mode-card.selected { border: 2rpx solid #8bab61; background: #fbfdf7; }.mode-card.disabled { opacity: .78; }.mode-symbol { position: relative; flex: none; width: 103rpx; height: 103rpx; border-radius: 29rpx; display: flex; align-items: center; justify-content: center; font-size: 30rpx; font-weight: 700; }.joint-dot { position: absolute; right: 18rpx; bottom: 20rpx; width: 16rpx; height: 16rpx; border: 4rpx solid currentColor; border-radius: 50%; }.mode-copy { min-width: 0; flex: 1; margin-left: 22rpx; }.mode-name-row { display: flex; align-items: center; gap: 12rpx; }.mode-name { color: #1d4339; font-size: 34rpx; font-weight: 700; }.later-tag { padding: 6rpx 11rpx; border-radius: 999rpx; background: #f3e8dd; color: #a16a53; font-size: 22rpx; }.mode-subtitle { display: block; margin-top: 7rpx; color: #647b74; font-size: 28rpx; }.mode-state { display: block; margin-top: 8rpx; color: #9aa4a0; font-size: 24rpx; }.radio { flex: none; width: 37rpx; height: 37rpx; border: 3rpx solid #cad4cf; border-radius: 50%; display: flex; align-items: center; justify-content: center; }.mode-card.selected .radio { border-color: #729653; }.radio view { width: 19rpx; height: 19rpx; border-radius: 50%; background: #729653; }
.wear-card { margin-top: 24rpx; padding: 28rpx; }.wear-head { display: flex; align-items: center; gap: 17rpx; }.wear-icon { width: 57rpx; height: 57rpx; border-radius: 18rpx; background: #e4efe9; color: #48766a; display: flex; align-items: center; justify-content: center; font-size: 32rpx; }.wear-head text { display: block; color: #274a41; font-size: 32rpx; font-weight: 700; }.wear-head text:last-child { margin-top: 5rpx; color: #8b9692; font-size: 24rpx; font-weight: 400; }.wear-body { margin-top: 24rpx; display: flex; flex-direction: column; gap: 18rpx; }.wear-step { display: flex; align-items: center; gap: 16rpx; color: #5d716b; font-size: 28rpx; line-height: 1.5; }.wear-step text:first-child { flex: none; width: 36rpx; height: 36rpx; border-radius: 12rpx; background: #eef3e4; color: #718345; display: flex; align-items: center; justify-content: center; font-size: 24rpx; font-weight: 700; }.extension-note { margin-top: 22rpx; padding: 18rpx; border-radius: 17rpx; background: #fff1e8; color: #a56852; font-size: 26rpx; }.confirm { margin-top: 28rpx; }.confirm.disabled { background: #dce2df; color: #8c9793; }.protocol-note { display: block; margin-top: 15rpx; text-align: center; color: #a0aaa6; font-size: 22rpx; }

/* RehabMotion type system v2 */
.mode-head { padding-top: 24rpx; padding-bottom: 20rpx; }
.mode-head text { font-size: 44rpx; font-weight: 700; line-height: 1.1; }
.mode-head text:last-child { font-size: 28rpx; font-weight: 400; line-height: 1.5; }
.mode-symbol { font-weight: 700; }
.mode-name, .wear-head text { font-weight: 600; }
.mode-subtitle, .later-tag { font-weight: 500; }
.wear-step { font-size: 28rpx; font-weight: 400; }
.wear-step text:first-child { font-weight: 600; font-variant-numeric: tabular-nums; }

</style>
