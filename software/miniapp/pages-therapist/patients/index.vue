<template>
  <view class="therapist-page" :style="viewport.style">
    <TherapistTopBar />
    <scroll-view scroll-y class="therapist-scroll">
      <view class="therapist-content">
        <text class="page-eyebrow">患者管理</text>
        <text class="page-title">我的患者</text>
        <text class="page-subtitle">优先查看近期变化，再进入个人训练记录。</text>

        <view class="search-field">
          <view class="search-icon"></view>
          <input v-model="query" placeholder="搜索患者姓名或编号" confirm-type="search" />
        </view>

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

        <view class="patient-list">
          <PatientCard
            v-for="patient in filteredPatients"
            :key="patient.id"
            :patient="patient"
            @open="openPatient"
          />
        </view>

        <view v-if="!filteredPatients.length" class="empty-state">没有找到匹配的患者</view>
      </view>
    </scroll-view>
    <TherapistTabBar active="patients" :task-count="taskCount" />
  </view>
</template>

<script setup lang="ts">
import { computed, onMounted, ref } from 'vue'
import TherapistTopBar from '../components/TherapistTopBar.vue'
import TherapistTabBar from '../components/TherapistTabBar.vue'
import PatientCard from '../components/PatientCard.vue'
import { useTherapistViewport } from '../runtime/therapist-viewport'
import { getTherapistRepository } from '../services'
import type { PatientSummary, TherapistDashboard } from '../types'

const viewport = useTherapistViewport()
const patients = ref<PatientSummary[]>([])
const dashboard = ref<TherapistDashboard | null>(null)
const query = ref('')
const activeFilter = ref('all')

const filters = computed(() => [
  { key: 'all', label: '全部', count: patients.value.length },
  { key: 'attention', label: '需关注', count: patients.value.filter(item => item.status === 'watch' || item.status === 'critical').length },
  { key: 'stable', label: '稳定', count: patients.value.filter(item => item.status === 'stable').length },
  { key: 'idle', label: '今日未训练', count: patients.value.filter(item => !item.trainedToday).length }
])

const filteredPatients = computed(() => patients.value.filter(patient => {
  const keyword = query.value.trim().toLowerCase()
  const queryMatch = !keyword || patient.name.toLowerCase().includes(keyword) || patient.id.toLowerCase().includes(keyword)
  const filterMatch = activeFilter.value === 'all'
    || (activeFilter.value === 'attention' && (patient.status === 'watch' || patient.status === 'critical'))
    || (activeFilter.value === 'stable' && patient.status === 'stable')
    || (activeFilter.value === 'idle' && !patient.trainedToday)
  return queryMatch && filterMatch
}))

const taskCount = computed(() => dashboard.value ? dashboard.value.alertCount + dashboard.value.pendingAiCount : 0)

onMounted(async () => {
  viewport.refresh()
  const repository = getTherapistRepository()
  const result = await Promise.all([repository.listPatients(), repository.getDashboard()])
  patients.value = result[0]
  dashboard.value = result[1]
})

function openPatient(id: string) {
  uni.navigateTo({ url: '/pages-therapist/patient-detail/index?id=' + encodeURIComponent(id) })
}
</script>

<style>@import "../styles/therapist.css";</style>
