<template>
  <view class="therapist-page" :style="viewport.style">
    <TherapistTopBar />
    <scroll-view scroll-y class="therapist-detail-scroll">
      <view class="therapist-content">
        <button class="back-link pressable" @tap="back"><view class="back-chevron"></view><text>返回待办</text></button>

        <view v-if="suggestion" class="review-heading">
          <text class="page-eyebrow">方案审核</text>
          <text class="page-title">{{ suggestion.patient }} · {{ suggestion.exercise }}</text>
          <text class="page-subtitle">结合近期训练表现，确认是否调整目标角度。</text>
        </view>

        <view v-if="suggestion" class="review-hero">
          <view class="review-hero__item">
            <text>当前目标</text>
            <text class="therapist-strong">{{ suggestion.fromAngle }}<text class="therapist-unit">°</text></text>
          </view>
          <view class="review-direction"><view></view></view>
          <view class="review-hero__item review-hero__item--new">
            <text>建议目标</text>
            <text class="therapist-strong">{{ draftAngle }}<text class="therapist-unit">°</text></text>
          </view>
        </view>

        <view v-if="suggestion" class="content-card review-evidence">
          <text class="section-title">建议依据</text>
          <view class="review-evidence__metrics">
            <view><text>计划完成</text><text class="therapist-strong">{{ suggestion.completion }}%</text></view>
            <view class="metric-separator"></view>
            <view><text>动作达标率</text><text class="therapist-strong">{{ suggestion.qualifiedRate }}%</text></view>
            <view class="metric-separator"></view>
            <view><text>近期状态</text><text class="therapist-strong review-state">{{ suggestion.stability }}</text></view>
          </view>
          <text class="review-reason">{{ suggestion.reason }}</text>
        </view>

        <view v-if="suggestion" class="content-card angle-setting">
          <view class="card-heading">
            <view><text class="section-title">确认目标角度</text><text class="section-subtitle">可在建议范围内手动调整</text></view>
            <text class="angle-range">{{ suggestion.minAngle }}°–{{ suggestion.maxAngle }}°</text>
          </view>
          <view class="angle-stepper">
            <button class="stepper-button pressable" @tap="changeAngle(-1)">−</button>
            <view class="angle-input-wrap">
              <input v-model.number="draftAngle" type="number" :min="suggestion.minAngle" :max="suggestion.maxAngle" @blur="normalizeAngle" />
              <text>°</text>
            </view>
            <button class="stepper-button pressable" @tap="changeAngle(1)">＋</button>
          </view>
          <view class="prescription-summary"><text>{{ suggestion.sets }}组 × {{ suggestion.reps }}次</text><text>保持{{ suggestion.holdSec }}秒</text><text>每周{{ suggestion.frequencyPerWeek }}天</text></view>
        </view>

        <view class="safety-note">
          <view class="safety-note__mark">!</view>
          <text>系统建议仅作辅助参考，训练目标由康复师结合患者实际情况确认。</text>
        </view>

        <view v-if="suggestion" class="detail-actions">
          <button class="secondary-action pressable" @tap="back">暂不调整</button>
          <button class="primary-action pressable" :disabled="suggestion.status === 'sent'" @tap="confirmDispatch">{{ suggestion.status === 'sent' ? '方案已下发' : '确认并下发' }}</button>
        </view>
      </view>
    </scroll-view>
  </view>
</template>

<script setup lang="ts">
import { ref } from 'vue'
import { onLoad } from '@dcloudio/uni-app'
import TherapistTopBar from '../components/TherapistTopBar.vue'
import { useTherapistViewport } from '../runtime/therapist-viewport'
import { getTherapistRepository } from '../services'
import type { AiSuggestion } from '../types'

const viewport = useTherapistViewport()
const suggestion = ref<AiSuggestion | null>(null)
const draftAngle = ref(0)

onLoad(async query => {
  viewport.refresh()
  const id = decodeURIComponent(String(query?.id || ''))
  suggestion.value = await getTherapistRepository().getAiSuggestion(id)
  if (suggestion.value) draftAngle.value = suggestion.value.toAngle
  else uni.showToast({ title: '未找到该方案', icon: 'none' })
})

function normalizeAngle() {
  if (!suggestion.value) return
  const numeric = Number(draftAngle.value)
  const fallback = suggestion.value.toAngle
  draftAngle.value = Math.min(suggestion.value.maxAngle, Math.max(suggestion.value.minAngle, Number.isFinite(numeric) ? Math.round(numeric) : fallback))
}

function changeAngle(delta: number) {
  draftAngle.value = Number(draftAngle.value) + delta
  normalizeAngle()
}

function back() {
  uni.navigateBack({ fail: () => uni.redirectTo({ url: '/pages-therapist/tasks/index' }) })
}

function confirmDispatch() {
  if (!suggestion.value || suggestion.value.status === 'sent') return
  normalizeAngle()
  uni.showModal({
    title: '确认下发训练方案',
    content: suggestion.value.patient + '的' + suggestion.value.exercise + '目标角度将设置为' + draftAngle.value + '°。',
    confirmText: '确认下发',
    cancelText: '再检查一下',
    success: async result => {
      if (!result.confirm || !suggestion.value) return
      await getTherapistRepository().approveAiSuggestion(suggestion.value.id, { targetAngle: draftAngle.value })
      suggestion.value.status = 'sent'
      suggestion.value.toAngle = draftAngle.value
      uni.showToast({ title: '方案已下发', icon: 'success' })
      setTimeout(back, 650)
    }
  })
}
</script>

<style>@import "../styles/therapist.css";.prescription-summary{margin-top:20rpx;padding-top:20rpx;border-top:1rpx solid rgba(20,77,65,.1);display:flex;justify-content:center;gap:24rpx;flex-wrap:wrap}.prescription-summary text{color:#557068;font-size:28rpx;font-weight:500}</style>

