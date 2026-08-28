<template>
  <view class="therapist-page" :style="viewport.style">
    <TherapistTopBar />
    <scroll-view scroll-y class="therapist-scroll">
      <view class="therapist-content">
        <text class="page-eyebrow">个人工作区</text>
        <text class="page-title">我的</text>
        <text class="page-subtitle">管理康复师身份与工作入口。</text>

        <view class="therapist-profile-card">
          <view class="therapist-profile-card__avatar">李</view>
          <view class="therapist-profile-card__copy">
            <text class="therapist-profile-card__name">{{ profile.name }}</text>
            <text class="therapist-profile-card__role">{{ profile.role }} · 从业{{ profile.workingYears }}年</text>
            <text class="therapist-profile-card__org">{{ profile.organization }}</text>
          </view>
          <view class="therapist-profile-card__count">
            <text class="therapist-strong">{{ profile.managedCount }}</text>
            <text>管理患者</text>
          </view>
        </view>

        <view class="section-heading">
          <view><text class="section-title">工作与设置</text></view>
        </view>

        <view class="content-card profile-menu">
          <button class="profile-menu__row pressable" @tap="goPatients">
            <view class="profile-menu__icon"><image src="/static/icons/therapist-patients.svg" mode="aspectFit" /></view>
            <view class="profile-menu__copy"><text>我的患者</text><text>查看患者与康复记录</text></view>
            <view class="row-arrow"></view>
          </button>
          <button class="profile-menu__row pressable" @tap="goTasks">
            <view class="profile-menu__icon profile-menu__icon--warm"><image src="/static/icons/therapist-tasks.svg" mode="aspectFit" /></view>
            <view class="profile-menu__copy"><text>待办与方案审核</text><text>{{ taskCount }}项需要处理</text></view>
            <view class="row-arrow"></view>
          </button>
          <button class="profile-menu__row pressable" @tap="showHelp">
            <view class="profile-menu__icon profile-menu__icon--soft"><image src="/static/icons/profile-help.svg" mode="aspectFit" /></view>
            <view class="profile-menu__copy"><text>帮助与反馈</text><text>设备、数据或方案问题</text></view>
            <view class="row-arrow"></view>
          </button>
          <button class="profile-menu__row profile-menu__row--last pressable" @tap="switchRole">
            <view class="profile-menu__icon profile-menu__icon--switch"><image src="/static/icons/profile-family.svg" mode="aspectFit" /></view>
            <view class="profile-menu__copy"><text>切换使用端</text><text>返回家属端或重新选择身份</text></view>
            <view class="row-arrow"></view>
          </button>
        </view>

        <view class="professional-note">
          <view class="professional-note__line"></view>
          <text>训练数据用于辅助康复管理，不替代医生诊断。</text>
        </view>
      </view>
    </scroll-view>
    <TherapistTabBar active="profile" :task-count="taskCount" />
  </view>
</template>

<script setup lang="ts">
import { computed, onMounted, ref } from 'vue'
import TherapistTopBar from '../components/TherapistTopBar.vue'
import TherapistTabBar from '../components/TherapistTabBar.vue'
import { useTherapistViewport } from '../runtime/therapist-viewport'
import { getTherapistRepository } from '../services'
import type { TherapistDashboard, TherapistProfile } from '../types'

const viewport = useTherapistViewport()
const profile = ref<TherapistProfile>({ name: '李敏', role: '康复治疗师', organization: 'RehabMotion 康复中心', managedCount: 0, workingYears: 8 })
const dashboard = ref<TherapistDashboard | null>(null)
const taskCount = computed(() => dashboard.value ? dashboard.value.alertCount + dashboard.value.pendingAiCount : 0)

onMounted(async () => {
  viewport.refresh()
  const repository = getTherapistRepository()
  const result = await Promise.all([repository.getProfile(), repository.getDashboard()])
  profile.value = result[0]
  dashboard.value = result[1]
})

function goPatients() { uni.redirectTo({ url: '/pages-therapist/patients/index' }) }
function goTasks() { uni.redirectTo({ url: '/pages-therapist/tasks/index' }) }

function showHelp() {
  uni.showModal({
    title: '帮助与反馈',
    content: '患者数据、设备状态或方案下发出现异常时，请先核对患者编号和训练记录，再联系 RehabMotion 支持。',
    showCancel: false,
    confirmText: '知道了'
  })
}

function switchRole() {
  uni.showModal({
    title: '切换使用端',
    content: '返回后可重新选择家属端或康复师端。',
    confirmText: '返回选择',
    cancelText: '继续使用',
    success(result) {
      if (result.confirm) uni.reLaunch({ url: '/pages/role/index' })
    }
  })
}
</script>

<style>@import "../styles/therapist.css";</style>

