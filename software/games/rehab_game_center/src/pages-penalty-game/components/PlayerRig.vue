<template>
  <view class="rpg-player">
    <image v-for="pose in poses" :key="pose.key" class="rpg-player__layer" :class="{ 'rpg-player__layer--active': activePose === pose.key }" :src="pose.src" mode="aspectFit" />
  </view>
</template>
<script lang="ts">
import { PENALTY_ASSETS } from '../runtime/asset-paths'
export default {
  name: 'RpgPlayerRig',
  props: { side: { type: String, required: true }, progress: { type: Number, required: true }, phase: { type: String, required: true } },
  data() { return { poses: [
    { key: 'neutral', src: PENALTY_ASSETS.player.neutral }, { key: 'left-flex', src: PENALTY_ASSETS.player.leftFlex },
    { key: 'left-kick', src: PENALTY_ASSETS.player.leftKick }, { key: 'right-flex', src: PENALTY_ASSETS.player.rightFlex },
    { key: 'right-kick', src: PENALTY_ASSETS.player.rightKick }
  ] } },
  computed: {
    activePose(): string {
      if (this.phase === 'KICKING' || this.phase === 'BALL_FLIGHT') return `${this.side}-kick`
      if (this.progress >= 0.35) return `${this.side}-flex`
      return 'neutral'
    }
  }
}
</script>
<style scoped>
.rpg-player{position:relative;width:min(31vw,330px);height:min(68vh,520px)}
.rpg-player__layer{position:absolute;inset:0;width:100%;height:100%;opacity:0;transition:opacity 80ms linear}.rpg-player__layer--active{opacity:1}
</style>
