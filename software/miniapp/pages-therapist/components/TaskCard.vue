<template>
  <view class="task-card pressable" @tap="$emit('open', alert.id)">
    <view class="task-card__marker" :class="'task-card__marker--' + alert.level"></view>
    <view class="task-card__body">
      <view class="task-card__meta">
        <text class="task-card__patient">{{ alert.patient }}</text>
        <text class="status-pill" :class="'status-pill--' + alert.level">{{ alert.tag }}</text>
        <text class="task-card__time">{{ alert.time }}</text>
      </view>
      <text class="task-card__title">{{ alert.title }}</text>
      <text class="task-card__detail">{{ alert.detail }}</text>
    </view>
    <view class="row-arrow"></view>
  </view>
</template>

<script setup lang="ts">
import type { AlertItem } from '../types'
defineProps<{ alert: AlertItem }>()
defineEmits<{ open: [id: string] }>()
</script>
