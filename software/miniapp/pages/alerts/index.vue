<template>
  <view class="page-shell messages-page">
    <secondary-page-header title="消息中心" subtitle="训练、设备与系统通知" @back="back">
      <template #action>
        <button v-if="unreadCount" class="read-all header-chip pressable" @tap="markAllRead">全部已读</button>
      </template>
    </secondary-page-header>

    <view class="message-summary">
      <view><text>{{ unreadCount }}</text><text>条未读消息</text></view>
      <text>{{ unreadCount ? '有新动态需要查看' : '所有消息都已查看' }}</text>
    </view>

    <view class="list-heading"><text>全部消息</text><text>最近更新</text></view>
    <view class="message-list card">
      <view v-for="item in messages" :key="item.id" class="message-row pressable" @tap="openMessage(item)">
        <view class="message-state">
          <view v-if="isUnread(item.id)" class="unread-dot"></view>
          <text :class="['type-label', item.type]">{{ item.typeLabel }}</text>
        </view>
        <view class="message-copy">
          <view class="message-title-line"><text>{{ item.title }}</text><text>{{ item.time }}</text></view>
          <text class="message-desc">{{ item.desc }}</text>
          <view class="message-foot"><text>{{ item.source }}</text><text v-if="item.action">{{ item.action }} ›</text></view>
        </view>
      </view>
    </view>

    <view class="message-note">消息仅用于同步居家训练和设备状态；涉及身体不适时，请及时联系医生或康复师。</view>
  </view>
</template>

<script>
import SecondaryPageHeader from '../../components/secondary-page-header/secondary-page-header.vue'
import { getMessages, getReadMessageIds, markAllMessagesRead, markMessageRead } from '../../services/messages.js'

export default {
  components: { SecondaryPageHeader },
  data() {
    return {
      messages: getMessages(),
      readIds: getReadMessageIds()
    }
  },
  computed: {
    unreadCount() {
      return this.messages.filter(item => this.isUnread(item.id)).length
    }
  },
  onShow() {
    this.messages = getMessages()
    this.readIds = getReadMessageIds()
  },
  methods: {
    back() {
      uni.navigateBack()
    },
    isUnread(id) {
      return !this.readIds.includes(id)
    },
    markAllRead() {
      markAllMessagesRead()
      this.readIds = getReadMessageIds()
      uni.showToast({ title: '已全部标为已读', icon: 'none' })
    },
    openMessage(item) {
      markMessageRead(item.id)
      this.readIds = getReadMessageIds()
      if (item.route) {
        uni.navigateTo({ url: item.route })
        return
      }
      uni.showModal({ title: item.title, content: item.content || item.desc, showCancel: false, confirmText: '知道了' })
    }
  }
}
</script>

<style scoped>
.messages-page { padding-bottom: 54rpx; }
.message-summary { padding: 25rpx 26rpx; border-radius: 28rpx; background: #174f42; display: flex; align-items: center; justify-content: space-between; }
.message-summary > view { display: flex; align-items: baseline; gap: 10rpx; }
.message-summary > view text:first-child { color: #d9ee7f; font-size: 48rpx; line-height: 1.05; font-weight: 700; letter-spacing: -1rpx; }
.message-summary > view text:last-child { color: #fff; font-size: 28rpx; font-weight: 500; }
.message-summary > text { color: #a6c0b8; font-size: 24rpx; font-weight: 400; }
.list-heading { padding: 31rpx 3rpx 15rpx; display: flex; align-items: center; justify-content: space-between; }
.list-heading text { color: #24483e; font-size: 30rpx; font-weight: 600; }
.list-heading text:last-child { color: #939e9a; font-size: 24rpx; font-weight: 400; }
.message-list { padding: 0 23rpx; overflow: hidden; box-shadow: none; }
.message-row { min-height: 160rpx; padding: 23rpx 0; border-bottom: 1rpx solid #e7ece9; display: flex; align-items: flex-start; }
.message-row:last-child { border-bottom: 0; }
.message-state { position: relative; flex: none; width: 76rpx; padding-top: 3rpx; }
.unread-dot { position: absolute; left: -10rpx; top: 8rpx; width: 12rpx; height: 12rpx; border-radius: 50%; background: #c96f55; }
.type-label { width: 58rpx; height: 48rpx; border-radius: 15rpx; background: #e6efeb; color: #2f7867; display: flex; align-items: center; justify-content: center; font-size: 24rpx; font-weight: 600; }
.type-label.attention { background: #f7e9e2; color: #b0634d; }
.type-label.device { background: #e8efe2; color: #637e54; }
.message-copy { min-width: 0; flex: 1; }
.message-title-line { display: flex; align-items: flex-start; justify-content: space-between; gap: 18rpx; }
.message-title-line text:first-child { min-width: 0; flex: 1; color: #294b42; font-size: 28rpx; line-height: 1.35; font-weight: 600; }
.message-title-line text:last-child { flex: none; color: #9aa4a0; font-size: 24rpx; font-weight: 400; }
.message-desc { display: block; margin-top: 8rpx; color: #687a73; font-size: 26rpx; line-height: 1.5; font-weight: 400; }
.message-foot { margin-top: 10rpx; display: flex; align-items: center; justify-content: space-between; }
.message-foot text { color: #97a19d; font-size: 24rpx; font-weight: 400; }
.message-foot text:last-child { color: #2f7867; font-weight: 500; }
.message-note { margin-top: 23rpx; padding: 0 20rpx; color: #8b9692; font-size: 24rpx; line-height: 1.55; text-align: center; }
</style>
