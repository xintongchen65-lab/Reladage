<template>
  <view class="therapist-page" :style="viewport.style">
    <TherapistTopBar />
    <scroll-view scroll-y class="therapist-detail-scroll">
      <view class="therapist-content">
        <button class="back-link pressable" @tap="back"><view class="back-chevron"></view><text>患者列表</text></button>

        <view v-if="patient" class="patient-hero">
          <view class="patient-hero__head">
            <view class="patient-hero__avatar">{{ patient.initial }}</view>
            <view class="patient-hero__identity">
              <view class="patient-hero__name-row">
                <text class="patient-hero__name">{{ patient.name }}</text>
                <text class="status-pill" :class="'status-pill--' + patient.status">{{ patient.statusLabel }}</text>
              </view>
              <text class="patient-hero__meta">{{ patient.gender }} · {{ patient.age }}岁 · {{ patient.id }}</text>
              <text class="patient-hero__exercise">{{ patient.exercise }} · {{ patient.stage }}</text>
            </view>
          </view>

          <view class="patient-hero__metrics">
            <view><text>本周训练</text><text class="therapist-strong">{{ patient.completed }} / {{ patient.planned }}<text class="therapist-unit">次</text></text></view>
            <view class="metric-separator"></view>
            <view><text>最大角度</text><text class="therapist-strong">{{ patient.rom }}<text class="therapist-unit">°</text></text></view>
            <view class="metric-separator"></view>
            <view><text>动作达标率</text><text class="therapist-strong">{{ patient.quality }}<text class="therapist-unit">%</text></text></view>
          </view>
        </view>

        <view class="detail-tabs">
          <button class="detail-tab pressable" :class="{ 'detail-tab--active': activeTab === 'overview' }" @tap="activeTab = 'overview'">康复总览</button>
          <button class="detail-tab pressable" :class="{ 'detail-tab--active': activeTab === 'records' }" @tap="activeTab = 'records'">训练记录</button>
        </view>

        <view v-if="patient && activeTab === 'overview'">
          <view class="content-card trend-card">
            <view class="card-heading">
              <view><text class="section-title">活动角度变化</text><text class="section-subtitle">最近4周最大活动角度</text></view>
              <view class="trend-summary" :class="{ 'trend-summary--down': patient.romDelta < 0 }">
                <text>{{ patient.romDelta >= 0 ? '+' : '' }}{{ patient.romDelta }}°</text>
                <text>较上月</text>
              </view>
            </view>

            <view class="bar-chart">
              <view v-for="point in patient.romTrend" :key="point.label" class="bar-chart__column">
                <text class="bar-chart__value">{{ point.value }}°</text>
                <view class="bar-chart__track"><view class="bar-chart__fill" :style="{ height: trendHeight(point.value) }"></view></view>
                <text class="bar-chart__label">{{ point.label }}</text>
              </view>
            </view>
            <text class="chart-caption">{{ trendCaption }}</text>
          </view>

          <view class="summary-grid">
            <view class="content-card summary-card">
              <text>训练执行</text>
              <text class="therapist-strong">{{ adherenceRate }}%</text>
              <text>本周完成 {{ patient.completed }} / {{ patient.planned }} 次</text>
            </view>
            <view class="content-card summary-card">
              <text>动作质量</text>
              <text class="therapist-strong">{{ patient.quality }}%</text>
              <text>{{ patient.qualityLabel }}</text>
            </view>
          </view>

          <view v-if="patientAlertCount" class="attention-note">
            <view class="attention-note__dot"></view>
            <view><text>有 {{ patientAlertCount }} 项记录需要复核</text><text>可在待办中查看具体依据</text></view>
            <button class="pressable" @tap="goTasks">查看待办<view class="inline-arrow"></view></button>
          </view>
        </view>

        <view v-if="patient && activeTab === 'records'" class="content-card records-card">
          <view class="card-heading">
            <view><text class="section-title">近期训练记录</text><text class="section-subtitle">按训练时间由近到远排列</text></view>
          </view>
          <view v-for="record in patient.records" :key="record.id" class="record-row">
            <view class="record-row__date">{{ record.date }}</view>
            <view class="record-row__main">
              <text>{{ record.exercise }}</text>
              <text>{{ record.durationMin }}分钟 · {{ record.reps }}次动作</text>
            </view>
            <view class="record-row__result">
              <text class="therapist-strong">{{ record.rom }}°</text>
              <text>{{ record.qualifiedRate }}%达标</text>
            </view>
          </view>
        </view>
      </view>
    </scroll-view>
  </view>
</template>

<script setup lang="ts">
import { computed, ref } from 'vue'
import { onLoad } from '@dcloudio/uni-app'
import TherapistTopBar from '../components/TherapistTopBar.vue'
import { useTherapistViewport } from '../runtime/therapist-viewport'
import { getTherapistRepository } from '../services'
import type { PatientDetail } from '../types'

const viewport = useTherapistViewport()
const patient = ref<PatientDetail | null>(null)
const activeTab = ref<'overview' | 'records'>('overview')
const patientAlertCount = ref(0)

const adherenceRate = computed(() => {
  if (!patient.value || !patient.value.planned) return 0
  return Math.round(patient.value.completed / patient.value.planned * 100)
})

const trendCaption = computed(() => {
  if (!patient.value) return ''
  if (patient.value.romDelta > 0) return '活动角度整体提升，近期训练保持稳定。'
  if (patient.value.romDelta < 0) return '活动角度近期下降，建议结合动作记录进一步复核。'
  return '活动角度保持稳定，可继续观察后续训练表现。'
})

onLoad(async query => {
  viewport.refresh()
  const id = decodeURIComponent(String(query?.id || 'RM-1024'))
  activeTab.value = query?.tab === 'records' ? 'records' : 'overview'
  const repository = getTherapistRepository()
  const result = await Promise.all([repository.getPatientDetail(id), repository.listAlerts()])
  patient.value = result[0]
  patientAlertCount.value = result[1].filter(item => item.patientId === id).length
})

function trendHeight(value: number) {
  if (!patient.value || !patient.value.romTrend.length) return '30%'
  const values = patient.value.romTrend.map(item => item.value)
  const min = Math.min(...values) - 4
  const max = Math.max(...values) + 4
  const percent = 24 + (value - min) / Math.max(1, max - min) * 70
  return Math.round(percent) + '%'
}

function back() {
  uni.navigateBack({ fail: () => uni.redirectTo({ url: '/pages-therapist/patients/index' }) })
}

function goTasks() {
  uni.redirectTo({ url: '/pages-therapist/tasks/index' })
}
</script>

<style>@import "../styles/therapist.css";</style>

