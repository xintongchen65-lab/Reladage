<template>
  <view class="therapist-page" :style="viewport.style">
    <TherapistTopBar />
    <scroll-view scroll-y class="therapist-scroll">
      <view class="therapist-content">
        <text class="page-eyebrow">{{ dateLabel }}</text>
        <text class="page-title">{{ greeting }}，李康复师</text>
        <text class="page-subtitle">今天有 {{ dashboard.attentionPatientCount }} 位患者需要重点查看。</text>

        <view class="dashboard-hero">
          <view class="dashboard-hero__head">
            <view>
              <text class="dashboard-hero__kicker">今日训练执行</text>
              <view class="dashboard-hero__number">
                <text>{{ dashboard.todayDone }}</text>
                <text class="dashboard-hero__total">/ {{ dashboard.todayTotal }} 位</text>
              </view>
            </view>
            <view class="dashboard-hero__rate">
              <text>{{ progress }}%</text>
              <text>已完成</text>
            </view>
          </view>
          <view class="dashboard-progress"><view :style="{ width: progress + '%' }"></view></view>
          <view class="dashboard-hero__metrics">
            <view><text>管理患者</text><text class="therapist-strong">{{ dashboard.managedCount }}<text class="therapist-unit">人</text></text></view>
            <view class="metric-separator"></view>
            <view><text>需关注</text><text class="therapist-strong">{{ dashboard.attentionPatientCount }}<text class="therapist-unit">人</text></text></view>
            <view class="metric-separator"></view>
            <view><text>待确认方案</text><text class="therapist-strong">{{ dashboard.pendingAiCount }}<text class="therapist-unit">项</text></text></view>
          </view>
        </view>

        <view class="quick-actions">
          <button class="quick-action pressable" @tap="goPatients">
            <view class="quick-action__icon quick-action__icon--patients"><image src="/static/icons/therapist-patients.svg" mode="aspectFit" /></view>
            <view><text>患者管理</text><text>查看全部患者</text></view>
            <view class="row-arrow"></view>
          </button>
          <button class="quick-action pressable" @tap="goTasks">
            <view class="quick-action__icon quick-action__icon--tasks"><image src="/static/icons/therapist-tasks.svg" mode="aspectFit" /></view>
            <view><text>方案审核</text><text>{{ dashboard.pendingAiCount }}项待确认</text></view>
            <view class="row-arrow"></view>
          </button>
        </view>

        <view class="section-heading">
          <view>
            <text class="section-title">优先关注</text>
            <text class="section-subtitle">按风险程度与近期变化排序</text>
          </view>
          <button class="section-link pressable" @tap="goTasks">全部待办<view class="inline-arrow"></view></button>
        </view>

        <view class="content-card task-panel">
          <TaskCard
            v-for="alert in dashboard.priorityAlerts"
            :key="alert.id"
            :alert="alert"
            @open="openAlert"
          />
          <view v-if="!dashboard.priorityAlerts.length" class="empty-state">目前没有需要优先处理的事项</view>
        </view>

        <view class="section-heading">
          <view>
            <text class="section-title">方案建议</text>
            <text class="section-subtitle">由康复师确认后再下发</text>
          </view>
        </view>

        <button class="suggestion-banner pressable" @tap="goTasks">
          <view class="suggestion-symbol">
            <view></view><view></view>
          </view>
          <view class="suggestion-copy">
            <text>{{ dashboard.pendingAiCount }} 项训练方案等待确认</text>
            <text>结合完成率、动作质量与角度变化生成</text>
          </view>
          <view class="row-arrow"></view>
        </button>
      </view>
    </scroll-view>
    <TherapistTabBar active="home" :task-count="taskCount" />
  </view>
</template>

<script setup lang="ts">
import { computed, onMounted, ref } from 'vue'
import TherapistTopBar from '../components/TherapistTopBar.vue'
import TherapistTabBar from '../components/TherapistTabBar.vue'
import TaskCard from '../components/TaskCard.vue'
import { useTherapistViewport } from '../runtime/therapist-viewport'
import { getTherapistRepository } from '../services'
import type { TherapistDashboard } from '../types'

const viewport = useTherapistViewport()
const dashboard = ref<TherapistDashboard>({
  managedCount: 0,
  todayDone: 0,
  todayTotal: 0,
  attentionPatientCount: 0,
  alertCount: 0,
  pendingAiCount: 0,
  priorityAlerts: []
})

const now = new Date()
const weekdays = ['星期日','星期一','星期二','星期三','星期四','星期五','星期六']
const dateLabel = weekdays[now.getDay()] + ' · ' + (now.getMonth() + 1) + '月' + now.getDate() + '日'
const greeting = now.getHours() < 11 ? '上午好' : now.getHours() < 18 ? '下午好' : '晚上好'
const progress = computed(() => dashboard.value.todayTotal ? Math.round(dashboard.value.todayDone / dashboard.value.todayTotal * 100) : 0)
const taskCount = computed(() => dashboard.value.alertCount + dashboard.value.pendingAiCount)

onMounted(async () => {
  viewport.refresh()
  dashboard.value = await getTherapistRepository().getDashboard()
})

function goPatients() { uni.redirectTo({ url: '/pages-therapist/patients/index' }) }
function goTasks() { uni.redirectTo({ url: '/pages-therapist/tasks/index' }) }
function openAlert(id: string) { uni.navigateTo({ url: '/pages-therapist/alert-detail/index?id=' + encodeURIComponent(id) }) }
</script>

<style>@import "../styles/therapist.css";</style>
