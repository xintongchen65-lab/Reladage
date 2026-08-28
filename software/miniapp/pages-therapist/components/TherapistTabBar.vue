<template>
  <view class="therapist-tabbar">
    <view
      v-for="tab in tabs"
      :key="tab.key"
      class="therapist-tabbar__item"
      :class="{ 'therapist-tabbar__item--active': active === tab.key }"
      @tap="go(tab.key)"
    >
      <view class="therapist-tabbar__icon">
        <image :src="tab.icon" mode="aspectFit" />
        <view v-if="tab.key === 'tasks' && taskCount > 0" class="therapist-tabbar__badge">{{ taskCount > 9 ? '9+' : taskCount }}</view>
      </view>
      <text>{{ tab.label }}</text>
    </view>
  </view>
</template>

<script setup lang="ts">
import type { TherapistTab } from '../types'

const props = withDefaults(defineProps<{ active: TherapistTab; taskCount?: number }>(), { taskCount: 0 })

const tabs: Array<{ key: TherapistTab; label: string; icon: string }> = [
  { key: 'home', label: '工作台', icon: '/static/icons/therapist-home.svg' },
  { key: 'patients', label: '患者', icon: '/static/icons/therapist-patients.svg' },
  { key: 'tasks', label: '待办', icon: '/static/icons/therapist-tasks.svg' },
  { key: 'profile', label: '我的', icon: '/static/icons/therapist-profile.svg' }
]

function go(key: TherapistTab) {
  if (key === props.active) return
  uni.redirectTo({ url: '/pages-therapist/' + key + '/index' })
}
</script>

<style scoped>
.therapist-tabbar { position: fixed; z-index: 40; right: 0; bottom: 0; left: 0; height: calc(var(--therapist-tabbar-height) + var(--therapist-safe-bottom)); padding-bottom: var(--therapist-safe-bottom); border-top: 1rpx solid rgba(23,79,66,.07); background: rgba(255,255,255,.98); display: flex; align-items: flex-start; box-shadow: 0 -10rpx 30rpx rgba(31,67,57,.035); }
.therapist-tabbar__item { position: relative; height: var(--therapist-tabbar-height); flex: 1; color: #8d9894; display: flex; flex-direction: column; align-items: center; justify-content: center; gap: 4rpx; font-size: 22rpx; line-height: 1; font-weight: 400; }
.therapist-tabbar__item--active { color: #2f7867; font-weight: 600; }
.therapist-tabbar__icon { position: relative; width: 44rpx; height: 44rpx; }
.therapist-tabbar__icon image { width: 44rpx; height: 44rpx; opacity: .48; filter: grayscale(1); }
.therapist-tabbar__item--active image { opacity: 1; filter: none; }
.therapist-tabbar__badge { position: absolute; top: -7rpx; right: -17rpx; min-width: 29rpx; height: 29rpx; padding: 0 7rpx; border: 4rpx solid #fff; border-radius: 999rpx; background: #c86d51; color: #fff; display: flex; align-items: center; justify-content: center; font-size: 18rpx; line-height: 1; font-weight: 600; }
</style>
