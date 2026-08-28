<template>
  <view class="therapist-page" :style="viewport.style">
    <TherapistTopBar />
    <scroll-view scroll-y class="therapist-detail-scroll">
      <view class="therapist-content">
        <button class="back-link pressable" @tap="back"><view class="back-chevron"></view><text>返回待办</text></button>

        <view v-if="alert" class="alert-hero" :class="'alert-hero--' + alert.level">
          <view class="alert-hero__top">
            <text class="status-pill" :class="'status-pill--' + alert.level">{{ alert.tag }}</text>
            <text>{{ alert.time }}</text>
          </view>
          <text class="alert-hero__patient">{{ alert.patient }}</text>
          <text class="alert-hero__title">{{ alert.title }}</text>
          <text class="alert-hero__detail">{{ alert.detail }}</text>
        </view>

        <view v-if="alert" class="content-card alert-chart-card">
          <view class="card-heading">
            <view><text class="section-title">{{ alert.metricLabel }}</text><text class="section-subtitle">仅呈现已记录的训练数据变化</text></view>
          </view>
          <view class="mini-bar-chart">
            <view v-for="point in alert.trend" :key="point.label" class="mini-bar-chart__column">
              <text>{{ point.value }}{{ unit }}</text>
              <view><view :style="{ height: alertHeight(point.value) }"></view></view>
              <text>{{ point.label }}</text>
            </view>
          </view>
        </view>

        <view v-if="alert" class="content-card recommendation-card">
          <view class="recommendation-card__title">
            <view class="recommendation-symbol">i</view>
            <text>复核建议</text>
          </view>
          <text class="recommendation-card__text">{{ alert.recommendation }}</text>
        </view>

        <view v-if="alert" class="detail-actions">
          <button class="secondary-action pressable" @tap="back">稍后处理</button>
          <button class="primary-action pressable" @tap="openRecords">查看训练记录</button>
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
import type { AlertItem } from '../types'

const viewport = useTherapistViewport()
const alert = ref<AlertItem | null>(null)

const unit = computed(() => {
  if (!alert.value) return ''
  if (alert.value.category === 'trend') return '°'
  if (alert.value.category === 'pain') return '分'
  return '次'
})

onLoad(async query => {
  viewport.refresh()
  const id = decodeURIComponent(String(query?.id || ''))
  alert.value = await getTherapistRepository().getAlert(id)
  if (!alert.value) {
    uni.showToast({ title: '未找到该待办', icon: 'none' })
  }
})

function alertHeight(value: number) {
  if (!alert.value || !alert.value.trend.length) return '30%'
  const values = alert.value.trend.map(item => item.value)
  const min = Math.min(...values)
  const max = Math.max(...values)
  return Math.round(26 + (value - min) / Math.max(1, max - min) * 68) + '%'
}

function back() {
  uni.navigateBack({ fail: () => uni.redirectTo({ url: '/pages-therapist/tasks/index' }) })
}

function openRecords() {
  if (!alert.value) return
  uni.redirectTo({ url: '/pages-therapist/patient-detail/index?id=' + encodeURIComponent(alert.value.patientId) + '&tab=records' })
}
</script>

<style>@import "../styles/therapist.css";</style>
