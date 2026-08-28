<template>
  <view class="rfg-rig" aria-label="双侧肘关节动作角色">
    <image
      class="rfg-rig__body"
      :src="bodySrc"
      mode="heightFix"
    />
    <view class="rfg-rig__arm-slot rfg-rig__arm-slot--left">
      <image class="rfg-rig__arm-layer" :class="{ 'rfg-rig__arm-layer--active': leftPose === 'low' }" :src="leftArmSources.low" mode="aspectFit" />
      <image class="rfg-rig__arm-layer" :class="{ 'rfg-rig__arm-layer--active': leftPose === 'mid' }" :src="leftArmSources.mid" mode="aspectFit" />
      <image class="rfg-rig__arm-layer" :class="{ 'rfg-rig__arm-layer--active': leftPose === 'high' }" :src="leftArmSources.high" mode="aspectFit" />
      <text class="rfg-rig__arm-label">左手动作</text>
    </view>
    <view class="rfg-rig__arm-slot rfg-rig__arm-slot--right">
      <image class="rfg-rig__arm-layer" :class="{ 'rfg-rig__arm-layer--active': rightPose === 'low' }" :src="rightArmSources.low" mode="aspectFit" />
      <image class="rfg-rig__arm-layer" :class="{ 'rfg-rig__arm-layer--active': rightPose === 'mid' }" :src="rightArmSources.mid" mode="aspectFit" />
      <image class="rfg-rig__arm-layer" :class="{ 'rfg-rig__arm-layer--active': rightPose === 'high' }" :src="rightArmSources.high" mode="aspectFit" />
      <text class="rfg-rig__arm-label">右手动作</text>
    </view>
  </view>
</template>

<script lang="ts">
import { progressToPose } from '../core/motion-mapper'
import type { ArmPose } from '../core/motion-mapper'
import { FRUIT_GAME_ASSETS } from '../runtime/asset-paths'

const ARM_PATHS = FRUIT_GAME_ASSETS.character

export default {
  name: 'RfgCharacterRig',
  data() {
    return {
      bodySrc: FRUIT_GAME_ASSETS.character.body,
      leftArmSources: ARM_PATHS.left,
      rightArmSources: ARM_PATHS.right,
      leftPose: 'low' as ArmPose,
      rightPose: 'low' as ArmPose
    }
  },
  props: {
    leftProgress: { type: Number, required: true },
    rightProgress: { type: Number, required: true }
  },
  watch: {
    leftProgress: {
      immediate: true,
      handler(value: number): void {
        this.leftPose = progressToPose(Number(value), this.leftPose)
      }
    },
    rightProgress: {
      immediate: true,
      handler(value: number): void {
        this.rightPose = progressToPose(Number(value), this.rightPose)
      }
    }
  }
}
</script>

<style scoped>
.rfg-rig {
  position: absolute;
  inset: 0;
  z-index: 9;
  pointer-events: none;
}

.rfg-rig__body {
  position: absolute;
  left: 50%;
  bottom: 2%;
  z-index: 3;
  height: 68%;
  transform: translateX(-50%);
}

.rfg-rig__arm-slot {
  position: absolute;
  top: 37%;
  z-index: 4;
  width: 24%;
  height: 44%;
}

.rfg-rig__arm-slot--left {
  left: 18.5%;
}

.rfg-rig__arm-slot--right {
  right: 18.5%;
}

.rfg-rig__arm-layer {
  position: absolute;
  inset: 0 0 20px;
  width: 100%;
  height: calc(100% - 20px);
  opacity: 0;
  transition: opacity 90ms linear;
}

.rfg-rig__arm-layer--active {
  opacity: 1;
}

.rfg-rig__arm-label {
  position: absolute;
  left: 50%;
  bottom: 0;
  padding: 3px 10px;
  border: 2px solid rgba(176, 102, 18, 0.72);
  border-radius: 999px;
  background: rgba(255, 247, 204, 0.92);
  color: #70400c;
  font-size: clamp(11px, 1.8vh, 15px);
  font-weight: 800;
  line-height: 1.2;
  white-space: nowrap;
  transform: translateX(-50%);
}

/* Fixed logical-stage typography. */
.rfg-rig__arm-label { font-size: 15px; }
</style>
