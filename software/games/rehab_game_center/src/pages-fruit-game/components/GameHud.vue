<template>
  <view class="rfg-hud">
    <view class="rfg-hud__metric rfg-hud__metric--left">
      <text class="rfg-hud__label">左侧</text>
      <text class="rfg-hud__value">{{ leftCount }}/{{ targetCount }}</text>
      <text class="rfg-hud__angle">{{ formatAngle(leftAngle) }}°</text>
    </view>

    <view class="rfg-hud__center">
      <view class="rfg-hud__score-row">
        <text class="rfg-hud__target">{{ activeSide === 'left' ? '← 左侧水果' : '右侧水果 →' }}</text>
        <text class="rfg-hud__set">第{{ setIndex }}/{{ targetSets }}组</text>
        <text class="rfg-hud__score">得分 {{ score }}</text>
        <text class="rfg-hud__combo">连击 ×{{ combo }}</text>
      </view>
      <view class="rfg-hud__progress-track">
        <view class="rfg-hud__progress-fill" :style="progressStyle" />
        <text class="rfg-hud__progress-text">{{ progress }}%</text>
      </view>
    </view>

    <view class="rfg-hud__metric rfg-hud__metric--right">
      <text class="rfg-hud__label">右侧</text>
      <text class="rfg-hud__value">{{ rightCount }}/{{ targetCount }}</text>
      <text class="rfg-hud__angle">{{ formatAngle(rightAngle) }}°</text>
    </view>

    <button class="rfg-hud__pause" @click="$emit('pause')">
      {{ paused ? '▶' : 'Ⅱ' }}
    </button>
    <button class="rfg-hud__exit" @click="$emit('exit')">结束</button>
  </view>
</template>

<script lang="ts">
export default {
  name: 'RfgGameHud',
  props: {
    leftCount: { type: Number, required: true },
    rightCount: { type: Number, required: true },
    targetCount: { type: Number, required: true },
    leftAngle: { type: Number, required: true },
    rightAngle: { type: Number, required: true },
    progress: { type: Number, required: true },
    setIndex: { type: Number, required: true },
    targetSets: { type: Number, required: true },
    score: { type: Number, required: true },
    combo: { type: Number, required: true },
    activeSide: { type: String, required: true },
    paused: { type: Boolean, required: true }
  },
  emits: ['pause', 'exit'],
  computed: {
    progressStyle(): Record<string, string> {
      const value = Math.min(100, Math.max(0, Number(this.progress)))
      return { width: `${value}%` }
    }
  },
  methods: {
    formatAngle(value: number): string {
      return Number(value).toFixed(0)
    }
  }
}
</script>

<style scoped>
.rfg-hud {
  position: absolute;
  top: var(--rfg-safe-top, 8px);
  left: 12px;
  right: 12px;
  z-index: 20;
  display: flex;
  align-items: start;
  gap: clamp(5px, 0.8vw, 10px);
  color: #55310e;
  font-family: "Microsoft YaHei", sans-serif;
}

.rfg-hud__metric {
  display: flex;
  align-items: center;
  justify-content: center;
  gap: clamp(4px, 0.6vw, 8px);
  min-width: 0;
  height: 44px;
  box-sizing: border-box;
  padding: 4px 8px;
  overflow: hidden;
  border: 3px solid #9f5d19;
  border-radius: 12px;
  background: #fff2c9;
  box-shadow: 0 4px 0 rgba(88, 46, 10, 0.26);
  white-space: nowrap;
  flex: 0 1 20%;
}

.rfg-hud__metric--right {
  justify-content: center;
}

.rfg-hud__label,
.rfg-hud__angle {
  font-size: clamp(11px, 1.8vh, 15px);
  font-weight: 700;
}

.rfg-hud__value {
  color: #6d3b0a;
  font-size: clamp(18px, 3vh, 25px);
  font-weight: 900;
}

.rfg-hud__center {
  flex: 1 1 auto;
  min-width: 0;
}

.rfg-hud__score-row {
  display: flex;
  align-items: center;
  justify-content: center;
  gap: clamp(7px, 1vw, 14px);
  height: 20px;
  margin-bottom: 3px;
  color: #fff;
  font-size: clamp(11px, 1.9vh, 15px);
  font-weight: 900;
  text-shadow: 0 2px 0 #6f3c0d;
  white-space: nowrap;
}

.rfg-hud__progress-track {
  position: relative;
  height: 21px;
  box-sizing: border-box;
  overflow: hidden;
  border: 3px solid #7f4615;
  border-radius: 12px;
  background: #4b270e;
  box-shadow: inset 0 3px 5px rgba(0, 0, 0, 0.25);
}

.rfg-hud__progress-fill {
  height: 100%;
  border-radius: 9px;
  background: repeating-linear-gradient(135deg, #8fe71f 0 14px, #6ecb12 14px 28px);
  transition: width 120ms linear;
}

.rfg-hud__progress-text {
  position: absolute;
  inset: 0;
  display: flex;
  align-items: center;
  justify-content: center;
  color: #fff;
  font-size: clamp(10px, 1.7vh, 13px);
  font-weight: 900;
  text-shadow: 0 1px 0 #315d0a;
}

.rfg-hud__pause,
.rfg-hud__exit {
  flex: 0 0 44px;
  width: 44px;
  height: 44px;
  margin: 0;
  padding: 0;
  border: 3px solid #bd7610;
  border-radius: 50%;
  background: linear-gradient(#ffca35, #f18c10);
  color: #fff;
  font-size: 21px;
  font-weight: 900;
  line-height: 38px;
  box-shadow: 0 4px 0 rgba(114, 63, 9, 0.3);
}

.rfg-hud__exit {
  flex-basis: 62px;
  width: 62px;
  border-radius: 12px;
  background: linear-gradient(#ff765e, #d83b26);
  font-size: 14px;
}

.rfg-hud__pause::after,
.rfg-hud__exit::after {
  border: 0;
}

@media (max-width: 720px), (max-height: 390px) {
  .rfg-hud {
    left: 8px;
    right: 8px;
    gap: 5px;
  }
  .rfg-hud__metric { flex-basis: 19%; min-width: 104px; height: 40px; padding: 3px 5px; }
  .rfg-hud__pause { flex-basis: 40px; width: 40px; height: 40px; line-height: 34px; }
  .rfg-hud__exit { flex-basis: 56px; width: 56px; height: 40px; line-height: 34px; }
  .rfg-hud__score-row { gap: 6px; }
}
</style>
