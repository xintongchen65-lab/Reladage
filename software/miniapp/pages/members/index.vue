<template>
  <view class="page-shell members-page">
    <secondary-page-header title="家庭成员" subtitle="管理当前关注的老人" @back="back">
      <template #action><button class="add-entry header-chip pressable" @tap="openForm">添加成员</button></template>
    </secondary-page-header>

    <view class="section-head"><text class="section-title">选择关注对象</text><text class="section-link">共 {{ members.length }} 人</text></view>
    <view class="member-list card">
      <view v-for="item in members" :key="item.id" class="member-row pressable" @tap="chooseMember(item)">
        <view class="member-avatar">{{ item.name.slice(0, 1) }}</view>
        <view class="member-copy"><text>{{ item.relationship }} · {{ item.name }}</text><text>{{ item.stage }}</text></view>
        <view v-if="item.id === current.id" class="current-tag">当前</view>
        <view v-else class="row-arrow"></view>
      </view>
    </view>

    <view v-if="showForm" class="form-card card">
      <view class="form-head"><text>添加家庭成员</text><button class="close-form pressable" @tap="showForm = false">×</button></view>
      <view class="form-row"><text>姓名</text><input v-model="form.name" maxlength="12" placeholder="请输入姓名" /></view>
      <picker :range="relationships" :value="relationshipIndex" @change="changeRelationship">
        <view class="form-row pressable"><text>与您的关系</text><view><text>{{ form.relationship }}</text><view class="row-arrow"></view></view></view>
      </picker>
      <view class="form-row"><text>康复阶段</text><input v-model="form.stage" maxlength="24" placeholder="例如：膝关节术后第4周" /></view>
      <button class="save-member pressable" @tap="saveMember">保存并关注</button>
    </view>

    <view class="members-note">切换后，首页、训练任务、训练记录和康复报告会同步显示当前成员。</view>
  </view>
</template>

<script>
import SecondaryPageHeader from '../../components/secondary-page-header/secondary-page-header.vue'
import { addMember, getCurrentMember, getMembers, setCurrentMember } from '../../services/members.js'

export default {
  components: { SecondaryPageHeader },
  data() {
    return {
      members: getMembers(),
      current: getCurrentMember(),
      showForm: false,
      relationships: ['父亲', '母亲', '配偶', '其他家人'],
      form: { name: '', relationship: '父亲', stage: '' }
    }
  },
  computed: {
    relationshipIndex() { return Math.max(0, this.relationships.indexOf(this.form.relationship)) }
  },
  methods: {
    back() { uni.navigateBack() },
    openForm() { this.showForm = true },
    changeRelationship(event) { this.form.relationship = this.relationships[Number(event.detail.value)] || this.relationships[0] },
    chooseMember(item) {
      setCurrentMember(item.id)
      this.current = item
      uni.showToast({ title: `正在关注${item.name}`, icon: 'success' })
    },
    saveMember() {
      try {
        const member = addMember(this.form)
        this.members = getMembers()
        this.current = member
        this.form = { name: '', relationship: '父亲', stage: '' }
        this.showForm = false
        uni.showToast({ title: '成员已添加', icon: 'success' })
      } catch (error) {
        uni.showToast({ title: error.message || '保存失败', icon: 'none' })
      }
    }
  }
}
</script>

<style scoped>
.members-page { padding-bottom: 54rpx; }
.member-list { padding: 0 23rpx; overflow: hidden; box-shadow: none; }
.member-row { min-height: 128rpx; border-bottom: 1rpx solid #e7ece9; display: flex; align-items: center; }
.member-row:last-child { border-bottom: 0; }
.member-avatar { flex: none; width: 72rpx; height: 72rpx; border-radius: 23rpx; background: #174f42; color: #d9ee7f; display: flex; align-items: center; justify-content: center; font-size: 30rpx; font-weight: 600; }
.member-copy { min-width: 0; flex: 1; margin-left: 18rpx; }
.member-copy text { display: block; color: #25483f; font-size: 28rpx; font-weight: 600; }
.member-copy text:last-child { margin-top: 7rpx; color: #8b9792; font-size: 24rpx; font-weight: 400; }
.current-tag { flex: none; padding: 8rpx 14rpx; border-radius: 999rpx; background: #e5efe8; color: #2f7867; font-size: 24rpx; font-weight: 500; }
.row-arrow { flex: none; width: 12rpx; height: 12rpx; margin-left: 12rpx; border-right: 3rpx solid #98a49f; border-top: 3rpx solid #98a49f; transform: rotate(45deg); }
.form-card { margin-top: 22rpx; padding: 0 23rpx 23rpx; box-shadow: none; }
.form-head { min-height: 90rpx; border-bottom: 1rpx solid #e7ece9; display: flex; align-items: center; justify-content: space-between; }
.form-head > text { color: #25483f; font-size: 30rpx; font-weight: 600; }
.close-form { width: 48rpx; height: 48rpx; border-radius: 15rpx; background: #edf2ef; color: #61736c; font-size: 32rpx; }
.form-row { min-height: 94rpx; border-bottom: 1rpx solid #e9eeeb; display: flex; align-items: center; gap: 20rpx; }
.form-row > text { flex: none; width: 166rpx; color: #405c54; font-size: 28rpx; font-weight: 500; }
.form-row input { flex: 1; height: 72rpx; color: #174f42; font-size: 28rpx; text-align: right; }
.form-row > view { flex: 1; display: flex; align-items: center; justify-content: flex-end; color: #315c50; font-size: 28rpx; }
.save-member { width: 100%; height: 78rpx; margin-top: 23rpx; border-radius: 21rpx; background: #174f42; color: #d9ee7f; display: flex; align-items: center; justify-content: center; font-size: 28rpx; font-weight: 600; }
.members-note { margin-top: 24rpx; padding: 0 18rpx; color: #8b9692; font-size: 24rpx; line-height: 1.55; text-align: center; }
</style>
