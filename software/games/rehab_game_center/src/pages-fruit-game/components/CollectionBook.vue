<template>
  <view class="rfg-collection" :style="safeStyle">
    <image class="rfg-collection__background" :src="backgroundSrc" mode="aspectFill" />
    <view class="rfg-collection__shade" />
    <view class="rfg-collection__panel">
      <view class="rfg-collection__header">
        <view>
          <text class="rfg-collection__title">我的水果篮</text>
          <text class="rfg-collection__subtitle">本局已经采集的所有水果</text>
        </view>
        <view class="rfg-collection__summary">
          <text>共 {{ total }} 个</text>
          <text>特殊 {{ specialTotal }} 个</text>
          <text>{{ score }} 分</text>
        </view>
      </view>

      <scroll-view class="rfg-collection__scroll" scroll-y :show-scrollbar="true">
        <view class="rfg-collection__grid">
          <view
            v-for="item in items"
            :key="item.name"
            class="rfg-collection__item"
            :class="{ 'rfg-collection__item--empty': item.count === 0, 'rfg-collection__item--special': item.special }"
          >
            <image class="rfg-collection__fruit" :src="item.src" mode="aspectFit" />
            <text class="rfg-collection__name">{{ item.label }}</text>
            <text class="rfg-collection__count">×{{ item.count }}</text>
          </view>
        </view>
      </scroll-view>

      <button class="rfg-collection__back" @click="$emit('close')">← 返回训练</button>
    </view>
  </view>
</template>

<script lang="ts">
import { COLLECTIBLE_FRUIT_NAMES, NORMAL_FRUIT_NAMES } from '../core/game-engine'
import type { CollectibleFruitName, FruitInventory } from '../core/game-engine'
import { FRUIT_GAME_ASSETS } from '../runtime/asset-paths'

const LABELS: Record<CollectibleFruitName, string> = {
  apple: '苹果',
  orange: '橙子',
  banana: '香蕉',
  grapes: '葡萄',
  peach: '桃子',
  pear: '梨子',
  strawberry: '草莓',
  watermelon: '西瓜',
  goldenApple: '金苹果',
  rainbowFruit: '彩虹果',
  bothWatermelon: '双手西瓜'
}

export default {
  name: 'RfgCollectionBook',
  props: {
    inventory: { type: Object, required: true },
    total: { type: Number, required: true },
    specialTotal: { type: Number, required: true },
    score: { type: Number, required: true },
    safeTop: { type: Number, default: 8 }
  },
  emits: ['close'],
  data() {
    return { backgroundSrc: FRUIT_GAME_ASSETS.orchard.basketZone }
  },
  computed: {
    safeStyle(): Record<string, string> {
      return { '--rfg-collection-safe-top': `${Math.max(8, Number(this.safeTop))}px` }
    },
    items(): Array<{ name: CollectibleFruitName; label: string; count: number; src: string; special: boolean }> {
      const inventory = this.inventory as FruitInventory
      return COLLECTIBLE_FRUIT_NAMES.map((name) => ({
        name,
        label: LABELS[name],
        count: Number(inventory[name] || 0),
        src: FRUIT_GAME_ASSETS.fruits[name],
        special: !NORMAL_FRUIT_NAMES.includes(name as typeof NORMAL_FRUIT_NAMES[number])
      }))
    }
  }
}
</script>

<style scoped>
.rfg-collection { position: absolute; inset: 0; z-index: 70; box-sizing: border-box; display: flex; align-items: center; justify-content: center; padding: var(--rfg-collection-safe-top, 8px) 14px 12px; color: #5a350d; font-family: "Microsoft YaHei", sans-serif; }
.rfg-collection__background, .rfg-collection__shade { position: absolute; inset: 0; width: 100%; height: 100%; }
.rfg-collection__shade { background: rgba(27, 70, 18, 0.18); }
.rfg-collection__panel { position: relative; z-index: 2; box-sizing: border-box; display: grid; grid-template-rows: auto minmax(0, 1fr) auto; width: min(920px, 92%); height: min(560px, calc(100% - 8px)); padding: clamp(10px, 2vh, 18px) clamp(14px, 2.5vw, 26px) 12px; overflow: hidden; border: 4px solid #b56a15; border-radius: 22px; background: rgba(255, 250, 220, 0.95); box-shadow: 0 8px 0 rgba(87, 46, 8, 0.28), 0 18px 40px rgba(24, 58, 15, 0.28); }
.rfg-collection__header { display: flex; align-items: center; justify-content: space-between; gap: 18px; padding-bottom: 8px; border-bottom: 2px solid #e6c56f; }
.rfg-collection__title, .rfg-collection__subtitle { display: block; }
.rfg-collection__title { color: #2d762b; font-size: clamp(24px, 5vh, 38px); font-weight: 900; line-height: 1.05; }
.rfg-collection__subtitle { margin-top: 2px; color: #7c6a4a; font-size: clamp(10px, 1.8vh, 14px); }
.rfg-collection__summary { display: flex; gap: clamp(8px, 1.6vw, 18px); padding: 7px 13px; border-radius: 999px; background: #eff8d9; color: #4e6b26; font-size: clamp(11px, 2vh, 15px); font-weight: 900; white-space: nowrap; }
.rfg-collection__scroll { min-height: 0; height: 100%; }
.rfg-collection__grid { display: grid; grid-template-columns: repeat(6, minmax(0, 1fr)); gap: clamp(6px, 1vw, 11px); padding: 10px 3px; }
.rfg-collection__item { position: relative; display: flex; flex-direction: column; align-items: center; min-width: 0; padding: 5px 4px 6px; border: 2px solid #e4c875; border-radius: 12px; background: rgba(255, 255, 245, 0.9); }
.rfg-collection__item--special { border-color: #f0a51b; background: #fff1bd; }
.rfg-collection__item--empty { filter: grayscale(1); opacity: 0.43; }
.rfg-collection__fruit { width: min(74px, 8.5vw); height: min(74px, 12vh); }
.rfg-collection__name { max-width: 100%; overflow: hidden; color: #67400e; font-size: clamp(9px, 1.7vh, 12px); font-weight: 800; text-overflow: ellipsis; white-space: nowrap; }
.rfg-collection__count { position: absolute; right: 5px; top: 4px; min-width: 24px; padding: 1px 4px; border-radius: 999px; background: #e98216; color: #fff; font-size: 11px; font-weight: 900; text-align: center; }
.rfg-collection__back { width: 180px; margin: 5px auto 0; border-radius: 999px; background: linear-gradient(#63bdf5, #2687ce); color: #fff; font-size: clamp(15px, 2.8vh, 20px); font-weight: 900; line-height: 1.9; box-shadow: 0 4px 0 #1e679c; }
.rfg-collection__back::after { border: 0; }

@media (max-height: 390px) {
  .rfg-collection__panel { width: min(880px, 92%); padding: 8px 16px; }
  .rfg-collection__header { padding-bottom: 5px; }
  .rfg-collection__grid { grid-template-columns: repeat(6, minmax(0, 1fr)); gap: 6px; padding: 6px 2px; }
  .rfg-collection__item { padding: 3px; }
  .rfg-collection__fruit { width: min(58px, 7vw); height: min(58px, 11vh); }
  .rfg-collection__back { width: 150px; margin-top: 2px; line-height: 1.65; }
}
</style>
