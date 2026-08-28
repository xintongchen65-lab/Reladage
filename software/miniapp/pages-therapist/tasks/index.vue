<template>
  <view class="therapist-page" :style="viewport.style">
    <TherapistTopBar />
    <scroll-view scroll-y class="therapist-scroll">
      <view class="therapist-content">
        <text class="page-eyebrow">工作管理</text>
        <view class="title-with-count">
          <text class="page-title">待办</text>
          <text class="title-count">{{ taskCount }}</text>
        </view>
        <text class="page-subtitle">先处理风险变化，再审核训练方案。</text>

        <scroll-view scroll-x class="filter-scroll" :show-scrollbar="false">
          <view class="filter-row">
            <button
              v-for="filter in filters"
              :key="filter.key"
              class="filter-chip pressable"
              :class="{ 'filter-chip--active': activeFilter === filter.key }"
              @tap="activeFilter = filter.key"
            >{{ filter.label }} {{ filter.count }}</button>
          </view>
        </scroll-view>

        <view class="content-card task-panel task-panel--standalone">
          <TaskCard
            v-for="alert in filteredAlerts"
            :key="alert.id"
            :alert="alert"
            @open="openAlert"
          />
          <view v-if="!filteredAlerts.length" class="empty-state">该分类目前没有待办</view>
        </view>

        <view class="section-heading">
          <view>
            <text class="section-title">方案审核</text>
            <text class="section-subtitle">建议仅供参考，确认后再下发</text>
          </view>
        </view>

        <view class="suggestion-list">
          <view v-for="suggestion in suggestions" :key="suggestion.id" class="content-card ai-card">
            <view class="ai-card__head">
              <view>
                <text class="ai-card__title">{{ suggestion.patient }} · {{ suggestion.exercise }}</text>
                <text class="ai-card__angle">目标角度 {{ suggestion.fromAngle }}° → {{ suggestion.toAngle }}°</text>
              </view>
              <text class="status-pill" :class="suggestion.status === 'pending' ? 'status-pill--ai' : 'status-pill--stable'">{{ suggestion.status === 'pending' ? '待确认' : '已下发' }}</text>
            </view>
            <view class="ai-card__metrics">
              <view><text>计划完成</text><text class="therapist-strong">{{ suggestion.completion }}%</text></view>
              <view class="metric-separator"></view>
              <view><text>动作达标率</text><text class="therapist-strong">{{ suggestion.qualifiedRate }}%</text></view>
              <view class="metric-separator"></view>
              <view><text>近期状态</text><text class="therapist-strong ai-card__state">{{ suggestion.stability }}</text></view>
            </view>
            <text class="ai-card__reason">{{ suggestion.reason }}</text>
            <button class="card-action pressable" :disabled="suggestion.status !== 'pending'" @tap="reviewSuggestion(suggestion.id)">
              {{ suggestion.status === 'pending' ? '查看依据并确认' : '方案已下发' }}
              <view v-if="suggestion.status === 'pending'" class="inline-arrow"></view>
            </button>
          </view>
        </view>
      </view>
    </scroll-view>
    <TherapistTabBar active="tasks" :task-count="taskCount" />
  </view>
</template>

<script setup lang="ts">
import { computed, onMounted, ref } from 'vue'
import TherapistTopBar from '../components/TherapistTopBar.vue'
import TherapistTabBar from '../components/TherapistTabBar.vue'
import TaskCard from '../components/TaskCard.vue'
import { useTherapistViewport } from '../runtime/therapist-viewport'
import { getTherapistRepository } from '../services'
import type { AiSuggestion, AlertItem } from '../types'

const viewport = useTherapistViewport()
const alerts = ref<AlertItem[]>([])
const suggestions = ref<AiSuggestion[]>([])
const activeFilter = ref('all')

const filters = computed(() => [
  { key: 'all', label: '全部预警', count: alerts.value.length },
  { key: 'trend', label: '角度趋势', count: alerts.value.filter(item => item.category === 'trend').length },
  { key: 'motion', label: '动作质量', count: alerts.value.filter(item => item.category === 'motion').length },
  { key: 'adherence', label: '训练执行', count: alerts.value.filter(item => item.category === 'adherence').length },
  { key: 'pain', label: '疼痛反馈', count: alerts.value.filter(item => item.category === 'pain').length }
])

const filteredAlerts = computed(() => activeFilter.value === 'all'
  ? alerts.value
  : alerts.value.filter(item => item.category === activeFilter.value))

const taskCount = computed(() => alerts.value.length + suggestions.value.filter(item => item.status === 'pending').length)

onMounted(async () => {
  viewport.refresh()
  await refreshData()
})

async function refreshData() {
  const repository = getTherapistRepository()
  const result = await Promise.all([repository.listAlerts(), repository.listAiSuggestions()])
  alerts.value = result[0]
  suggestions.value = result[1]
}

function openAlert(id: string) {
  uni.navigateTo({ url: '/pages-therapist/alert-detail/index?id=' + encodeURIComponent(id) })
}

function reviewSuggestion(id: string) {
  const item = suggestions.value.find(suggestion => suggestion.id === id)
  if (!item || item.status !== 'pending') return
  uni.navigateTo({ url: '/pages-therapist/ai-review/index?id=' + encodeURIComponent(id) })
}
</script>

<style>@import "../styles/therapist.css";</style>

