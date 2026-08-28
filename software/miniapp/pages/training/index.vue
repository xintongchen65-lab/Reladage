<template>
  <view class="training-page">
    <secondary-page-header title="实时训练" :subtitle="device.connected ? '设备已连接' : '设备未连接'" dark @back="exit" />

    <view class="session-head"><view><text class="session-mode">{{ modeName }} · {{ exerciseName }}</text><text class="session-dose">第 {{ packet.set_index }} / {{ packet.target_sets }} 组 · 目标 {{ packet.target_count }} 次</text></view><view class="state-tag" :class="packet.training_state.toLowerCase()">{{ stateLabel }}</view></view>
    <view class="overall-track"><view :style="{ width: packet.overall_completion_percent + '%' }"></view></view>

    <view v-if="warningText" class="warning-banner"><text>!</text><text>{{ warningText }}</text></view>

    <view class="angle-stage">
      <view class="stage-grid"></view>
      <view class="angle-column">
        <text class="side-label">AB · 左侧</text>
        <view class="gauge" :style="{ '--angle': leftGauge }"><view><text>{{ rounded(packet.left_angle_deg) }}</text><text>°</text></view></view>
        <text class="rom-label">最大 {{ rounded(packet.left_rom_deg) }}°</text>
      </view>
      <view class="stage-divider"></view>
      <view class="angle-column">
        <text class="side-label">CD · 右侧</text>
        <view class="gauge right" :style="{ '--angle': rightGauge }"><view><text>{{ rounded(packet.right_angle_deg) }}</text><text>°</text></view></view>
        <text class="rom-label">最大 {{ rounded(packet.right_rom_deg) }}°</text>
      </view>
      <view class="target-line"><text>目标角度 {{ config.target_angle_deg }}°</text></view>
    </view>

    <view class="quality-card" :class="quality.tone"><view class="quality-icon">{{ quality.tone === 'good' ? '✓' : '!' }}</view><view><text>{{ quality.label }}</text><text>{{ qualityTip }}</text></view><text class="diff">差值 {{ rounded(packet.lr_rom_diff_deg) }}°</text></view>

    <view class="count-grid">
      <view class="count-card"><text>左侧次数</text><view><text>{{ packet.left_count }}</text><text>/ {{ packet.target_count }}</text></view></view>
      <view class="count-card"><text>右侧次数</text><view><text>{{ packet.right_count }}</text><text>/ {{ packet.target_count }}</text></view></view>
      <view class="count-card progress-card"><text>本组进度</text><view><text>{{ packet.completion_percent }}</text><text>%</text></view></view>
    </view>

    <view class="control-area">
      <button class="minor-control pressable" @tap="stop"><text>■</text><text>结束</text></button>
      <button class="main-control pressable" @tap="mainAction"><text>{{ mainIcon }}</text><text>{{ mainText }}</text></button>
      <button class="minor-control pressable" @tap="calibrate"><text>◎</text><text>标定</text></button>
    </view>
  </view>
</template>

<script>
import { jointModes, qualityMap, warningMap } from '../../data/catalog.js'
import { getPrescription, getDeviceSnapshot, subscribeRealtime, sendTrainingCommand, calibrateImu } from '../../services/device.js'
import SecondaryPageHeader from '../../components/secondary-page-header/secondary-page-header.vue'

const emptyPacket = { set_index: 1, target_sets: 3, target_count: 10, left_angle_deg: 0, right_angle_deg: 0, left_rom_deg: 0, right_rom_deg: 0, lr_rom_diff_deg: 0, left_count: 0, right_count: 0, completion_percent: 0, overall_completion_percent: 0, training_state: 'IDLE', quality: 'GOOD', warning: 'none' }

export default {
  components: { SecondaryPageHeader },
  data() { return { config: getPrescription(), device: getDeviceSnapshot(), packet: { ...emptyPacket }, unsubscribe: null } },
  computed: {
    modeName() { const mode = jointModes.find(item => item.id === this.config.joint_id); return mode ? mode.name : '手肘' },
    exerciseName() { return this.config.exercise === 'knee_flexion' ? '膝关节屈伸' : '肘关节屈伸' },
    stateLabel() { return { IDLE: '待开始', READY: '待佩戴确认', RUNNING: '训练中', PAUSED: '已暂停', REST: '组间休息', FINISHED: '已完成', STOPPED: '已结束', ERROR: '异常' }[this.packet.training_state] || this.packet.training_state },
    quality() { return qualityMap[this.packet.quality] || qualityMap.GOOD },
    qualityTip() { return { GOOD: '保持当前节奏，动作幅度稳定', ROM_LOW: '尝试缓慢达到有效角度阈值', ASYMMETRY: '两侧动作幅度差异较大，请放慢' }[this.packet.quality] || '请根据设备提示调整动作' },
    warningText() { return warningMap[this.packet.warning] || '' },
    leftGauge() { return `${Math.min(100, this.packet.left_angle_deg / 120 * 100)}%` },
    rightGauge() { return `${Math.min(100, this.packet.right_angle_deg / 120 * 100)}%` },
    mainIcon() { return this.packet.training_state === 'RUNNING' ? 'Ⅱ' : '▶' },
    mainText() { return this.packet.training_state === 'RUNNING' ? '暂停' : this.packet.training_state === 'PAUSED' ? '继续' : '开始' }
  },
  onLoad() { this.unsubscribe = subscribeRealtime(packet => { this.packet = { ...emptyPacket, ...packet } }) },
  onUnload() { if (this.unsubscribe) this.unsubscribe() },
  methods: {
    rounded(value) { return Math.round(Number(value || 0)) },
    mainAction() {
      const command = this.packet.training_state === 'RUNNING' ? 'PAUSE' : this.packet.training_state === 'PAUSED' ? 'RESUME' : 'START'
      sendTrainingCommand(command)
    },
    stop() { uni.showModal({ title: '结束本次训练？', content: '设备会结束本次训练并保存已完成记录。', confirmText: '结束训练', success: result => { if (result.confirm) sendTrainingCommand('STOP') } }) },
    calibrate() { if (this.packet.training_state === 'RUNNING') { uni.showToast({ title: '请先暂停训练再标定', icon: 'none' }); return } calibrateImu('all').then(() => uni.showToast({ title: '标定请求已发送', icon: 'success' })).catch(error => uni.showToast({ title: error.message || '发送失败', icon: 'none' })) },
    exit() { if (this.packet.training_state !== 'RUNNING') { uni.navigateBack(); return } uni.showModal({ title: '训练正在进行', content: '退出前将暂停设备计数。', confirmText: '暂停并退出', success: result => { if (result.confirm) { sendTrainingCommand('PAUSE'); uni.navigateBack() } } }) }
  }
}
</script>

<style scoped>
.training-page { min-height: 100vh; padding: 0 28rpx calc(25rpx + env(safe-area-inset-bottom)); background: #133f35; color: #fff; }.training-nav .back-button,.training-nav .more { background: rgba(255,255,255,.1); color: #fff; box-shadow: none; }.more { width: 68rpx; height: 68rpx; border-radius: 22rpx; font-size: 28rpx; letter-spacing: 3rpx; }.live-title { display: flex; flex-direction: column; align-items: center; }.live-title>text:first-child { font-size: 32rpx; font-weight: 700; }.live-title>text:last-child { margin-top: 6rpx; color: #a8c4bb; font-size: 22rpx; display: flex; align-items: center; gap: 8rpx; }.live-title .status-dot { background: #d9ee7f; }.session-head { padding: 23rpx 4rpx 18rpx; display: flex; align-items: center; justify-content: space-between; }.session-mode { display: block; font-size: 34rpx; font-weight: 700; }.session-dose { display: block; margin-top: 8rpx; color: #a8c0b9; font-size: 24rpx; }.state-tag { padding: 10rpx 16rpx; border-radius: 999rpx; background: rgba(255,255,255,.1); color: #c3d5d0; font-size: 24rpx; }.state-tag.running { background: #d9ee7f; color: #1c493e; }.state-tag.paused { background: #f4dfb8; color: #805e2f; }.overall-track { height: 8rpx; border-radius: 8rpx; background: rgba(255,255,255,.1); overflow: hidden; }.overall-track view { height: 100%; border-radius: 8rpx; background: #d9ee7f; transition: width .3s; }.warning-banner { margin-top: 14rpx; padding: 15rpx 18rpx; border-radius: 17rpx; background: #f4d9cf; color: #954e41; display: flex; align-items: center; gap: 12rpx; font-size: 26rpx; }
.angle-stage { position: relative; height: 420rpx; margin-top: 22rpx; overflow: hidden; border-radius: 35rpx; background: #eaf1ed; color: #174a3e; display: flex; align-items: center; justify-content: space-around; }.stage-grid { position: absolute; inset: 0; opacity: .35; background-image: linear-gradient(rgba(35,90,75,.08) 1rpx, transparent 1rpx),linear-gradient(90deg,rgba(35,90,75,.08) 1rpx,transparent 1rpx); background-size: 55rpx 55rpx; }.angle-column { z-index: 1; width: 45%; display: flex; flex-direction: column; align-items: center; }.side-label { color: #55766c; font-size: 28rpx; font-weight: 600; }.gauge { --angle: 0%; width: 190rpx; height: 190rpx; margin-top: 17rpx; border: 15rpx solid #cfddd6; border-top-color: #2d7968; border-right-color: #2d7968; border-radius: 50%; display: flex; align-items: center; justify-content: center; transform: rotate(14deg); }.gauge.right { border-top-color: #7e9850; border-right-color: #7e9850; }.gauge view { transform: rotate(-14deg); display: flex; align-items: baseline; }.gauge view text:first-child { font-size: 72rpx; line-height: 1; font-weight: 700; }.gauge view text:last-child { font-size: 32rpx; }.rom-label { margin-top: 16rpx; color: #688078; font-size: 24rpx; }.stage-divider { z-index: 1; width: 1rpx; height: 245rpx; background: #cbd9d3; }.target-line { position: absolute; left: 50%; bottom: 18rpx; transform: translateX(-50%); padding: 9rpx 15rpx; border-radius: 999rpx; background: #d8e4de; color: #59756c; font-size: 22rpx; white-space: nowrap; }
.quality-card { margin-top: 16rpx; padding: 19rpx 21rpx; border-radius: 23rpx; background: #e6f0dc; color: #335b4e; display: flex; align-items: center; }.quality-card.warn { background: #f7e8da; color: #875944; }.quality-icon { flex: none; width: 50rpx; height: 50rpx; border-radius: 16rpx; background: rgba(255,255,255,.65); display: flex; align-items: center; justify-content: center; font-weight: 700; }.quality-card>view:nth-child(2) { flex: 1; margin-left: 15rpx; }.quality-card>view:nth-child(2) text { display: block; font-size: 28rpx; font-weight: 700; }.quality-card>view:nth-child(2) text:last-child { margin-top: 4rpx; opacity: .75; font-size: 22rpx; font-weight: 400; }.diff { font-size: 24rpx; }.count-grid { margin-top: 15rpx; display: grid; grid-template-columns: repeat(3,1fr); gap: 11rpx; }.count-card { padding: 17rpx 14rpx; border-radius: 21rpx; background: rgba(255,255,255,.08); }.count-card>text { color: #9cb9b0; font-size: 22rpx; }.count-card view { margin-top: 8rpx; display: flex; align-items: baseline; }.count-card view text:first-child { font-size: 36rpx; font-weight: 700; }.count-card view text:last-child { margin-left: 5rpx; color: #8facA3; font-size: 22rpx; }.progress-card { background: rgba(217,238,127,.13); }
.control-area { margin-top: 22rpx; display: flex; align-items: center; justify-content: space-around; }.minor-control { display: flex; flex-direction: column; align-items: center; gap: 7rpx; color: #a5bdb6; font-size: 22rpx; }.minor-control text:first-child { width: 56rpx; height: 56rpx; border-radius: 50%; background: rgba(255,255,255,.1); display: flex; align-items: center; justify-content: center; color: #fff; font-size: 28rpx; }.main-control { width: 151rpx; height: 71rpx; border-radius: 999rpx; background: #d9ee7f; color: #173b32; display: flex; align-items: center; justify-content: center; gap: 12rpx; font-size: 28rpx; font-weight: 700; }.main-control text:first-child { font-size: 32rpx; }.source-note { display: block; margin-top: 14rpx; text-align: center; color: #829f96; font-size: 22rpx; }

/* RehabMotion type system v2 */
.live-title > text:first-child, .session-mode, .quality-card > view:nth-child(2) text:first-child { font-weight: 600; }
.state-tag, .side-label, .main-control { font-weight: 500; }
.gauge view text:first-child {
  font-size: 72rpx;
  font-weight: 700;
  line-height: 1.05;
  letter-spacing: -2rpx;
  font-variant-numeric: tabular-nums;
}
.gauge view text:last-child { font-weight: 500; }
.count-card view text:first-child {
  font-size: 48rpx;
  font-weight: 700;
  line-height: 1.05;
  letter-spacing: -1rpx;
  font-variant-numeric: tabular-nums;
}
.count-card view text:last-child, .diff { font-weight: 500; font-variant-numeric: tabular-nums; }

</style>
