<template>
  <view class="profile-page">
    <app-page-header title="我的" :subtitle="memberLabel">
      <template #action>
        <button class="header-setting header-chip pressable" @tap="openPrivacy">
          <text>设置</text><view class="tiny-arrow"></view>
        </button>
      </template>
    </app-page-header>

    <view class="profile-content">
      <view class="member-card">
        <view class="member-avatar"><text>{{ member.name.slice(0, 1) }}</text><view class="member-dot"></view></view>
        <view class="member-copy">
          <text class="member-kicker">CURRENT MEMBER</text>
          <text class="member-name">{{ member.relationship }} {{ separator }} {{ member.name }}</text>
          <text class="member-stage">{{ member.stage }}</text>
        </view>
      </view>

      <view class="section-heading"><text>设备与消息</text></view>
      <view class="list-card">
        <view class="list-row pressable" @tap="openDevice">
          <view class="row-icon device-icon"><image src="/static/icons/profile-device.svg" mode="aspectFit" /></view>
          <view class="row-copy">
            <text class="row-title">我的设备</text>
            <view class="row-description"><view class="connection-dot" :class="{ offline: !device.connected }"></view><text>{{ device.deviceName }} {{ separator }} {{ device.connected ? '已连接' : '未连接' }}</text></view>
          </view>
          <view class="battery-side"><text>电量</text><text>{{ device.battery }}%</text></view>
          <view class="row-arrow"></view>
        </view>

        <view class="list-row last pressable" @tap="openAlerts">
          <view class="row-icon alert-icon"><image src="/static/icons/profile-alert.svg" mode="aspectFit" /></view>
          <view class="row-copy">
            <text class="row-title">消息中心</text>
            <text class="row-description single-line">训练、设备和关注事项</text>
          </view>
          <view v-if="unreadCount" class="alert-count">{{ unreadCount }}条</view>
          <view class="row-arrow"></view>
        </view>
      </view>

      <view class="section-heading"><text>管理与设置</text></view>
      <view class="list-card management-card">
        <view class="list-row compact-row pressable" @tap="openMembers">
          <view class="row-icon family-icon"><image src="/static/icons/profile-family.svg" mode="aspectFit" /></view>
          <view class="row-copy"><text class="row-title">家庭成员</text><text class="row-description single-line">添加或切换关注对象</text></view>
          <text class="version-copy">{{ members.length }}人</text>
          <view class="row-arrow"></view>
        </view>

        <view class="list-row compact-row pressable" @tap="openPrivacy">
          <view class="row-icon privacy-icon"><image src="/static/icons/profile-privacy.svg" mode="aspectFit" /></view>
          <view class="row-copy"><text class="row-title">账号与隐私</text></view>
          <view class="row-arrow"></view>
        </view>

        <view class="list-row compact-row pressable" @tap="switchRole">
          <view class="row-icon role-switch-icon"><image src="/static/icons/profile-family.svg" mode="aspectFit" /></view>
          <view class="row-copy"><text class="row-title">切换使用端</text><text class="row-description single-line">当前：家属端</text></view>
          <view class="row-arrow"></view>
        </view>

        <view class="list-row compact-row pressable" @tap="openHelp">
          <view class="row-icon help-icon"><image src="/static/icons/profile-help.svg" mode="aspectFit" /></view>
          <view class="row-copy"><text class="row-title">帮助与反馈</text></view>
          <view class="row-arrow"></view>
        </view>

        <view class="list-row compact-row last pressable" @tap="openAbout">
          <view class="row-icon about-icon"><image src="/static/icons/profile-about.svg" mode="aspectFit" /></view>
          <view class="row-copy"><text class="row-title">关于 RehabMotion</text></view>
          <text class="version-copy">1.0.0</text>
          <view class="row-arrow"></view>
        </view>
      </view>

      <view class="page-note">
        <text>居家训练数据仅供康复管理参考</text>
        <text>不替代医生诊断或康复处方</text>
      </view>
    </view>
  </view>
</template>

<script>
import AppPageHeader from '../../components/app-page-header/app-page-header.vue'
import { getDeviceSnapshot } from '../../services/device.js'
import { getUnreadMessageCount } from '../../services/messages.js'
import { getCurrentMember, getCurrentMemberLabel, getMembers } from '../../services/members.js'

export default {
  components: { AppPageHeader },
  data() {
    return {
      member: getCurrentMember(),
      memberLabel: getCurrentMemberLabel(),
      members: getMembers(),
      separator: '\u00b7',
      device: getDeviceSnapshot(),
      unreadCount: getUnreadMessageCount()
    }
  },
  onShow() {
    this.device = getDeviceSnapshot()
    this.unreadCount = getUnreadMessageCount()
    this.member = getCurrentMember()
    this.memberLabel = getCurrentMemberLabel()
    this.members = getMembers()
  },
  methods: {
    openDevice() { uni.navigateTo({ url: '/pages/maintenance/index' }) },
    openAlerts() { uni.navigateTo({ url: '/pages/alerts/index' }) },
    openMembers() { uni.navigateTo({ url: '/pages/members/index' }) },
    openPrivacy() {
      uni.showModal({
        title: '账号与隐私',
        content: '训练记录用于家庭康复管理。请妥善保管设备和账号信息，不要向无关人员分享康复报告。',
        showCancel: false,
        confirmText: '知道了'
      })
    },
    switchRole() {
      uni.showModal({
        title: '切换使用端',
        content: '返回后可重新选择家属端或康复师端。',
        confirmText: '返回选择',
        cancelText: '继续使用',
        success: result => {
          if (result.confirm) uni.reLaunch({ url: '/pages/role/index' })
        }
      })
    },
    openHelp() {
      uni.showModal({
        title: '帮助与反馈',
        content: '设备连接或训练记录异常时，请先检查设备电量和佩戴状态；仍未恢复可联系 RehabMotion 支持。',
        showCancel: false,
        confirmText: '知道了'
      })
    },
    openAbout() {
      uni.showModal({
        title: '关于 RehabMotion',
        content: '版本 1.0.0\n用于家庭居家康复训练管理与设备状态查看。',
        showCancel: false,
        confirmText: '知道了'
      })
    }
  }
}
</script>

<style scoped>
.profile-page { min-height: 100vh; padding-bottom: calc(48rpx + env(safe-area-inset-bottom)); background: #f5f6f3; color: #173f35; }
.profile-content { padding: 10rpx 30rpx 0; }
.disabled-row { opacity: .58; }
.pending-label { flex: none; color: #87938f; font-size: 24rpx; font-weight: 500; }
.demo-state { color: #b8c8c1; }
.header-setting { gap: 9rpx; }
.tiny-arrow, .light-arrow, .row-arrow { flex: none; border-top: 2rpx solid currentColor; border-right: 2rpx solid currentColor; transform: rotate(45deg); }
.tiny-arrow { width: 9rpx; height: 9rpx; }

.member-card { position: relative; min-height: 188rpx; padding: 28rpx; overflow: hidden; border-radius: 31rpx; background: #174f42; color: #fff; display: flex; align-items: center; box-shadow: 0 13rpx 32rpx rgba(23, 79, 66, .13); }
.member-card::after { content: ''; position: absolute; right: -116rpx; top: -144rpx; width: 310rpx; height: 310rpx; border: 2rpx solid rgba(217, 238, 127, .11); border-radius: 50%; }
.member-avatar { position: relative; z-index: 1; flex: none; width: 84rpx; height: 84rpx; border-radius: 27rpx 27rpx 27rpx 9rpx; background: #d9ee7f; color: #174f42; display: flex; align-items: center; justify-content: center; font-size: 38rpx; line-height: 1; font-weight: 700; transform: rotate(-3deg); }
.member-avatar > text { transform: rotate(3deg); }
.member-dot { position: absolute; right: -4rpx; bottom: -4rpx; width: 22rpx; height: 22rpx; border: 5rpx solid #174f42; border-radius: 50%; background: #7aa45e; }
.member-copy { position: relative; z-index: 1; min-width: 0; flex: 1; margin-left: 22rpx; }
.member-kicker { display: block; color: #d9ee7f; font-size: 20rpx; line-height: 1; letter-spacing: 2rpx; font-weight: 600; }
.member-name { display: block; margin-top: 12rpx; color: #fff; font-size: 34rpx; line-height: 1.2; font-weight: 600; }
.member-stage { display: block; margin-top: 9rpx; color: #a7c1b9; font-size: 24rpx; line-height: 1.35; font-weight: 400; }
.member-action { position: relative; z-index: 1; flex: none; margin-left: 18rpx; min-height: 48rpx; padding: 0 15rpx; border: 1rpx solid rgba(217, 238, 127, .42); border-radius: 999rpx; color: #d9ee7f; display: flex; align-items: center; gap: 9rpx; font-size: 24rpx; font-weight: 500; }
.light-arrow { width: 9rpx; height: 9rpx; }

.section-heading { padding: 34rpx 3rpx 15rpx; }
.section-heading text { color: #294c42; font-size: 30rpx; line-height: 1.3; font-weight: 600; }
.list-card { padding: 0 24rpx; overflow: hidden; border: 1rpx solid rgba(23, 79, 66, .06); border-radius: 27rpx; background: #fff; box-shadow: none; }
.list-row { min-height: 120rpx; border-bottom: 1rpx solid #e8edea; display: grid; grid-template-columns: 58rpx minmax(0, 1fr) auto 14rpx; align-items: center; column-gap: 16rpx; }
.list-row.compact-row { min-height: 106rpx; }
.list-row.last { border-bottom: 0; }
.row-icon { width: 58rpx; height: 58rpx; border-radius: 18rpx; display: flex; align-items: center; justify-content: center; }
.row-icon image { width: 34rpx; height: 34rpx; }
.device-icon { border-radius: 18rpx 18rpx 18rpx 7rpx; background: #174f42; }
.alert-icon { background: #f8eae2; }
.row-copy { min-width: 0; }
.row-title { display: block; color: #284b41; font-size: 28rpx; line-height: 1.35; font-weight: 500; }
.row-description { min-width: 0; margin-top: 7rpx; color: #89958f; display: flex; align-items: center; gap: 9rpx; font-size: 24rpx; line-height: 1.4; font-weight: 400; }
.single-line { display: block; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
.connection-dot { flex: none; width: 11rpx; height: 11rpx; border-radius: 50%; background: #76a05e; box-shadow: 0 0 0 4rpx rgba(118, 160, 94, .10); }
.connection-dot.offline { background: #bd7059; box-shadow: 0 0 0 4rpx rgba(189, 112, 89, .10); }
.battery-side { min-width: 58rpx; text-align: right; }
.battery-side text { display: block; color: #96a19c; font-size: 22rpx; line-height: 1.1; font-weight: 400; }
.battery-side text:last-child { margin-top: 7rpx; color: #1d594a; font-size: 30rpx; line-height: 1; letter-spacing: -1rpx; font-weight: 700; font-variant-numeric: tabular-nums; }
.alert-count { min-width: 48rpx; height: 38rpx; padding: 0 11rpx; border-radius: 999rpx; background: #f7e7df; color: #ad654e; display: flex; align-items: center; justify-content: center; font-size: 22rpx; line-height: 1; font-weight: 600; }
.row-arrow { width: 11rpx; height: 11rpx; color: #9ca8a3; }

.family-icon { background: #e5efea; }
.role-switch-icon { background: #eef1ee; }
.privacy-icon { background: #e7eeeb; }
.help-icon { background: #f4ede2; }
.about-icon { background: #eef0ee; }
.version-copy { color: #929d98; font-size: 24rpx; font-weight: 400; font-variant-numeric: tabular-nums; }
.page-note { padding: 31rpx 0 6rpx; color: #99a39f; display: flex; flex-direction: column; align-items: center; gap: 6rpx; font-size: 22rpx; line-height: 1.4; font-weight: 400; text-align: center; }
</style>
