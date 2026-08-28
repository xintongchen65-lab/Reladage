<template>
  <view class="rfg-debug" :class="{ 'rfg-debug--collapsed': !expanded }">
    <button class="rfg-debug__toggle" @click="$emit('toggle')">
      {{ expanded ? '收起调试' : '展开调试' }}
    </button>
    <view v-if="expanded" class="rfg-debug__panel">
      <view class="rfg-debug__group">
        <text class="rfg-debug__title">左肘</text>
        <!-- #ifdef H5 -->
        <view
          class="rfg-debug__button rfg-debug__hold"
          @touchstart.prevent="hold('leftFlex', true)"
          @touchend.prevent="hold('leftFlex', false)"
          @touchcancel.prevent="hold('leftFlex', false)"
          @mousedown.prevent="hold('leftFlex', true)"
          @mouseup.prevent="hold('leftFlex', false)"
          @mouseleave.prevent="hold('leftFlex', false)"
        >W 屈肘 / 角度＋</view>
        <view
          class="rfg-debug__button rfg-debug__hold"
          @touchstart.prevent="hold('leftExtend', true)"
          @touchend.prevent="hold('leftExtend', false)"
          @touchcancel.prevent="hold('leftExtend', false)"
          @mousedown.prevent="hold('leftExtend', true)"
          @mouseup.prevent="hold('leftExtend', false)"
          @mouseleave.prevent="hold('leftExtend', false)"
        >S 伸肘 / 角度－</view>
        <!-- #endif -->
        <!-- #ifndef H5 -->
        <view class="rfg-debug__button rfg-debug__hold" @touchstart.stop.prevent="hold('leftFlex', true)" @touchend.stop.prevent="hold('leftFlex', false)" @touchcancel.stop.prevent="hold('leftFlex', false)">W 屈肘 / 角度＋</view>
        <view class="rfg-debug__button rfg-debug__hold" @touchstart.stop.prevent="hold('leftExtend', true)" @touchend.stop.prevent="hold('leftExtend', false)" @touchcancel.stop.prevent="hold('leftExtend', false)">S 伸肘 / 角度－</view>
        <!-- #endif -->
      </view>

      <view class="rfg-debug__group">
        <text class="rfg-debug__title">右肘</text>
        <!-- #ifdef H5 -->
        <view
          class="rfg-debug__button rfg-debug__hold"
          @touchstart.prevent="hold('rightFlex', true)"
          @touchend.prevent="hold('rightFlex', false)"
          @touchcancel.prevent="hold('rightFlex', false)"
          @mousedown.prevent="hold('rightFlex', true)"
          @mouseup.prevent="hold('rightFlex', false)"
          @mouseleave.prevent="hold('rightFlex', false)"
        >↑ 屈肘 / 角度＋</view>
        <view
          class="rfg-debug__button rfg-debug__hold"
          @touchstart.prevent="hold('rightExtend', true)"
          @touchend.prevent="hold('rightExtend', false)"
          @touchcancel.prevent="hold('rightExtend', false)"
          @mousedown.prevent="hold('rightExtend', true)"
          @mouseup.prevent="hold('rightExtend', false)"
          @mouseleave.prevent="hold('rightExtend', false)"
        >↓ 伸肘 / 角度－</view>
        <!-- #endif -->
        <!-- #ifndef H5 -->
        <view class="rfg-debug__button rfg-debug__hold" @touchstart.stop.prevent="hold('rightFlex', true)" @touchend.stop.prevent="hold('rightFlex', false)" @touchcancel.stop.prevent="hold('rightFlex', false)">↑ 屈肘 / 角度＋</view>
        <view class="rfg-debug__button rfg-debug__hold" @touchstart.stop.prevent="hold('rightExtend', true)" @touchend.stop.prevent="hold('rightExtend', false)" @touchcancel.stop.prevent="hold('rightExtend', false)">↓ 伸肘 / 角度－</view>
        <!-- #endif -->
      </view>

      <view class="rfg-debug__group rfg-debug__group--session">
        <text class="rfg-debug__title">会话</text>
        <button class="rfg-debug__button" @click="$emit('pause')">P 暂停/继续</button>
        <button class="rfg-debug__button" @click="$emit('finish')">F 完成</button>
        <button class="rfg-debug__button rfg-debug__button--danger" @click="$emit('stop')">X 提前结束</button>
        <button class="rfg-debug__button" @click="$emit('reset')">R 重置</button>
      </view>
    </view>
  </view>
</template>

<script lang="ts">
export default {
  name: 'RfgDebugControls',
  props: {
    expanded: { type: Boolean, required: true }
  },
  data() {
    return {
      heldState: {} as Record<string, boolean>
    }
  },
  emits: ['toggle', 'hold', 'pause', 'finish', 'stop', 'reset'],
  watch: {
    expanded(value: boolean): void {
      if (!value) this.releaseAll()
    }
  },
  beforeDestroy() {
    this.releaseAll()
  },
  beforeUnmount() {
    this.releaseAll()
  },
  methods: {
    hold(control: string, pressed: boolean): void {
      if (this.heldState[control] === pressed) return
      this.heldState[control] = pressed
      this.$emit('hold', { control, pressed })
    },
    releaseAll(): void {
      Object.keys(this.heldState).forEach((control) => {
        if (this.heldState[control]) this.$emit('hold', { control, pressed: false })
        this.heldState[control] = false
      })
    }
  }
}
</script>

<style scoped>
.rfg-debug {
  position: absolute;
  left: 50%;
  right: auto;
  bottom: 5%;
  z-index: 40;
  width: 68%;
  max-width: 620px;
  max-height: 46%;
  transform: translateX(-50%);
  color: #3f2b16;
  font-family: "Microsoft YaHei", sans-serif;
}

.rfg-debug--collapsed {
  left: 1.5%;
  bottom: 1.5%;
  width: 112px;
  max-width: 112px;
  max-height: none;
  transform: none;
}

.rfg-debug__toggle,
.rfg-debug__button {
  margin: 0;
  border: 1px solid rgba(85, 56, 20, 0.35);
  border-radius: 8px;
  background: rgba(255, 248, 220, 0.94);
  color: #4c3219;
  font-size: clamp(11px, 1.8vh, 14px);
  line-height: 1.25;
}

.rfg-debug__toggle {
  box-sizing: border-box;
  display: flex;
  align-items: center;
  justify-content: center;
  width: 112px;
  height: 34px;
  margin-left: auto;
  padding: 0 8px;
  white-space: nowrap;
}

.rfg-debug__panel {
  display: grid;
  grid-template-columns: repeat(2, minmax(0, 1fr));
  gap: 8px;
  max-height: calc(48vh - 38px);
  margin-top: 6px;
  padding: 8px;
  overflow-y: auto;
  border: 2px solid rgba(75, 44, 12, 0.32);
  border-radius: 12px;
  background: rgba(255, 255, 255, 0.9);
  box-shadow: 0 5px 16px rgba(36, 68, 22, 0.22);
}

.rfg-debug__group {
  display: grid;
  grid-template-columns: 54px repeat(2, minmax(112px, 1fr));
  gap: 6px;
}

.rfg-debug__group--session {
  grid-column: 1 / -1;
  grid-template-columns: 54px repeat(4, minmax(0, 1fr));
}

.rfg-debug__title {
  display: flex;
  align-items: center;
  font-size: clamp(11px, 1.8vh, 14px);
  font-weight: 800;
}

.rfg-debug__button {
  box-sizing: border-box;
  min-height: 34px;
  padding: 6px 8px;
}

.rfg-debug__hold {
  display: flex;
  align-items: center;
  justify-content: center;
  text-align: center;
  user-select: none;
}

.rfg-debug__button--danger {
  background: #ffe1d8;
  color: #9d2e16;
}

.rfg-debug__toggle::after,
.rfg-debug__button::after {
  border: 0;
}

@media (max-height: 420px) {
  .rfg-debug { width: 72%; max-width: 580px; }
  .rfg-debug--collapsed { width: 108px; max-width: 108px; }
  .rfg-debug--collapsed .rfg-debug__toggle { width: 108px; }
  .rfg-debug__panel { gap: 5px; padding: 6px; }
  .rfg-debug__button { min-height: 30px; padding: 4px 6px; }
}

/* Fixed logical-stage debug layout. */
.rfg-debug { width: 72%; max-width: 580px; }
.rfg-debug--collapsed { width: 108px; max-width: 108px; }
.rfg-debug__toggle, .rfg-debug__title { font-size: 14px; }
.rfg-debug__panel { max-height: 308px; gap: 8px; padding: 8px; }
.rfg-debug__button { min-height: 34px; padding: 6px 8px; }
</style>
