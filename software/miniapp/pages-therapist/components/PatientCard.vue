<template>
  <view class="patient-card pressable" @tap="$emit('open', patient.id)">
    <view class="patient-card__top">
      <view class="patient-card__identity">
        <view class="patient-card__avatar" :class="'patient-card__avatar--' + patient.avatarTone">{{ patient.initial }}</view>
        <view class="patient-card__copy">
          <text class="patient-card__name">{{ patient.name }}</text>
          <text class="patient-card__meta">{{ patient.stage }}</text>
        </view>
      </view>
      <text class="status-pill" :class="'status-pill--' + patient.status">{{ patient.statusLabel }}</text>
    </view>

    <text class="patient-card__exercise">{{ patient.exercise }}</text>

    <view class="patient-card__metrics">
      <view class="patient-card__metric">
        <text class="patient-card__metric-label">本周训练</text>
        <text class="patient-card__metric-value">{{ patient.completed }} / {{ patient.planned }}<text class="patient-card__unit">次</text></text>
      </view>
      <view class="patient-card__divider"></view>
      <view class="patient-card__metric">
        <text class="patient-card__metric-label">最大角度</text>
        <text class="patient-card__metric-value">{{ patient.rom }}<text class="patient-card__unit">°</text></text>
      </view>
      <view class="patient-card__divider"></view>
      <view class="patient-card__metric">
        <text class="patient-card__metric-label">动作达标率</text>
        <text class="patient-card__metric-value">{{ patient.quality }}<text class="patient-card__unit">%</text></text>
      </view>
    </view>

    <view class="patient-card__footer">
      <text>最近训练 {{ patient.lastTraining }}</text>
      <view class="inline-link"><text>查看详情</text><view class="inline-arrow"></view></view>
    </view>
  </view>
</template>

<script setup lang="ts">
import type { PatientSummary } from '../types'
defineProps<{ patient: PatientSummary }>()
defineEmits<{ open: [id: string] }>()
</script>
