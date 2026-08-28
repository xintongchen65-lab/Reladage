<template>
  <view class="rpg-prepare" :style="viewportCss">
    <image class="rpg-prepare__bg" :src="assets.stadium" mode="aspectFill"/><view class="rpg-prepare__shade"/>
    <button class="rpg-prepare__back" @click="back">← 返回</button>
    <view class="rpg-prepare__panel">
      <text class="rpg-prepare__eyebrow">RehabMotion 康复游戏</text><text class="rpg-prepare__title">点球大战</text><text class="rpg-prepare__sub">膝关节屈伸 · 左右腿交替训练</text>
      <view class="rpg-prepare__targets">
        <view><text>训练组数</text><button v-if="editable" @click="change('sets',-1)">−</button><text>{{ config.targetSets }}</text><button v-if="editable" @click="change('sets',1)">＋</button></view>
        <view><text>每组左右次数</text><button v-if="editable" @click="change('count',-1)">−</button><text>{{ config.targetCount }}</text><button v-if="editable" @click="change('count',1)">＋</button></view>
      </view>
      <text class="rpg-prepare__params">目标 {{ config.targetAngleDeg }}° · 有效 ≥ {{ config.validAngleDeg }}° · 返回 ≤ {{ config.returnAngleDeg }}°</text>
      <text class="rpg-prepare__note">{{ editable?'Fake调试参数将在校准后生效':'真实训练参数由主控提供，本页只读展示' }}</text>
      <button class="rpg-prepare__start" @click="start">进入校准</button>
    </view>
  </view>
</template>
<script lang="ts">
import { getViewportLayout, viewportStyle } from '../../game-platform/runtime/viewport'
import { appendReturnUrl, configureCallerReturnUrl, returnToCaller } from '../../game-platform/runtime/navigation'
import { configurePenaltyFromQuery } from '../runtime/launch-config'
import { configurePenaltySession, getPenaltyConfig, registerPenaltyResultEmitter } from '../runtime/session-runtime'
import { PENALTY_ASSETS } from '../runtime/asset-paths'
export default {
  name:'RpgPrepare',data(){return{assets:PENALTY_ASSETS,config:getPenaltyConfig(),viewport:getViewportLayout(),returning:false,multiplayer:false}},
  computed:{editable():boolean{return this.config.sourceKind==='fake'},viewportCss():Record<string,string>{return viewportStyle(this.viewport,'--rpg-safe-top')}},
  onLoad(query:Record<string,string|undefined>){configureCallerReturnUrl(query?.returnUrl,'/pages-penalty-game/home/index');this.multiplayer=query?.multiplayer==='1';this.config=configurePenaltyFromQuery(query||{});const channel=(this as any).getOpenerEventChannel?.();registerPenaltyResultEmitter((result)=>channel?.emit('penaltyGameResult',result))},
  onResize(){this.viewport=getViewportLayout()},onBackPress(){this.back();return true},
  methods:{change(kind:string,delta:number){if(!this.editable)return;this.config=configurePenaltySession(kind==='sets'?{...this.config,targetSets:this.config.targetSets+delta}:{...this.config,targetCount:this.config.targetCount+delta})},start(){uni.redirectTo({url:appendReturnUrl(`/pages-penalty-game/calibrate/index${this.multiplayer?'?multiplayer=1':''}`)})},back(){if(this.returning)return;this.returning=true;registerPenaltyResultEmitter(null);returnToCaller()}}
}
</script>
<style scoped>
.rpg-prepare{position:relative;display:flex;align-items:center;justify-content:center;box-sizing:border-box;width:100vw;height:100vh;padding:var(--rpg-safe-top,8px) 12px 12px;overflow:hidden;font-family:"Microsoft YaHei",sans-serif;color:#163d60}.rpg-prepare__bg,.rpg-prepare__shade{position:absolute;inset:0;width:100%;height:100%}.rpg-prepare__shade{background:rgba(14,69,107,.28)}.rpg-prepare__back{position:absolute;z-index:4;top:calc(var(--rpg-safe-top,8px) + 5px);left:14px;display:flex;align-items:center;justify-content:center;width:92px;height:38px;margin:0;padding:0;border:0;border-radius:999px;background:#fff;color:#2673a9;font-size:14px}.rpg-prepare__panel{position:relative;width:min(680px,86vw);box-sizing:border-box;padding:clamp(16px,3vh,28px);border:4px solid #2688c8;border-radius:24px;background:rgba(255,255,255,.96);box-shadow:0 10px 0 rgba(8,73,115,.28)}.rpg-prepare__eyebrow,.rpg-prepare__title,.rpg-prepare__sub,.rpg-prepare__params,.rpg-prepare__note{display:block;text-align:center}.rpg-prepare__eyebrow{font-weight:800}.rpg-prepare__title{color:#0878cb;font-size:clamp(32px,7vh,52px);font-weight:900}.rpg-prepare__sub{font-size:clamp(14px,3vh,20px)}.rpg-prepare__targets{display:flex;gap:12px;margin:clamp(10px,2.5vh,18px) 0}.rpg-prepare__targets>view{display:flex;flex:1;align-items:center;justify-content:center;gap:8px;padding:10px;border-radius:15px;background:#eaf6ff;font-weight:800}.rpg-prepare__targets button{display:flex;align-items:center;justify-content:center;width:34px;height:34px;margin:0;padding:0;border:0;border-radius:50%;background:#2c91d3;color:#fff;line-height:1}.rpg-prepare__params,.rpg-prepare__note{margin-top:6px;font-size:13px}.rpg-prepare__note{color:#758795}.rpg-prepare__start{display:flex;align-items:center;justify-content:center;width:min(360px,80%);height:52px;margin:15px auto 0;padding:0;border:0;border-radius:999px;background:#ffc729;color:#704600;font-size:19px;font-weight:900}
.rpg-prepare__panel{max-height:calc(100dvh - var(--rpg-safe-top,8px) - 16px);padding:clamp(10px,2.3vh,22px);overflow-y:auto}
.rpg-prepare__title{line-height:1.05}.rpg-prepare__targets{margin:clamp(7px,1.8vh,13px) 0}.rpg-prepare__targets>view{padding:7px}.rpg-prepare__start{height:44px;margin-top:10px;padding:0 12px;line-height:1}
@media(max-height:390px){.rpg-prepare__panel{width:min(640px,82vw);padding:8px 18px}.rpg-prepare__eyebrow{font-size:12px}.rpg-prepare__title{font-size:31px}.rpg-prepare__sub{font-size:14px}.rpg-prepare__targets{margin:6px 0}.rpg-prepare__targets>view{padding:5px}.rpg-prepare__start{height:38px;margin-top:7px;font-size:17px}}
</style>
