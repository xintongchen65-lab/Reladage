<template>
  <view class="rpg-cal" :style="viewportCss"><image class="rpg-cal__bg" :src="assets.stadium" mode="aspectFill"/><view class="rpg-cal__shade"/>
    <button class="rpg-cal__back" @click="back">← 返回</button><view class="rpg-cal__panel">
      <text class="rpg-cal__title">请坐稳并伸直当前训练腿</text><text class="rpg-cal__side">从左腿开始 · 保持大腿稳定</text><text class="rpg-cal__count">{{ countdown }}</text>
      <text class="rpg-cal__message">{{ config.sourceKind==='fake'?'正在建立0°伸膝零位':'姿势引导倒计时；正式校准以主控结果为准' }}</text>
    </view></view>
</template>
<script lang="ts">
import { getViewportLayout, viewportStyle } from '../../game-platform/runtime/viewport'
import { appendReturnUrl, configureCallerReturnUrl, returnToCaller } from '../../game-platform/runtime/navigation'
import { getPenaltyConfig } from '../runtime/session-runtime';import { PENALTY_ASSETS } from '../runtime/asset-paths'
import { leavePenaltyRoom } from '../runtime/multiplayer-runtime'
export default {name:'RpgCalibrate',data(){return{assets:PENALTY_ASSETS,config:getPenaltyConfig(),countdown:3,timer:null as ReturnType<typeof setInterval>|null,multiplayer:false,viewport:getViewportLayout(),leaving:false}},computed:{viewportCss():Record<string,string>{return viewportStyle(this.viewport,'--rpg-safe-top')}},onLoad(q:Record<string,string|undefined>){configureCallerReturnUrl(q?.returnUrl,'/pages-penalty-game/home/index');this.multiplayer=q?.multiplayer==='1';this.timer=setInterval(()=>{if(this.countdown>1)this.countdown-=1;else{this.clear();uni.redirectTo({url:appendReturnUrl(`/pages-penalty-game/game/index${this.multiplayer?'?multiplayer=1':''}`)})}},1000)},onUnload(){this.clear()},onResize(){this.viewport=getViewportLayout()},onBackPress(){this.back();return true},methods:{clear(){if(this.timer)clearInterval(this.timer);this.timer=null},back(){if(this.leaving)return;this.leaving=true;this.clear();if(this.multiplayer)leavePenaltyRoom();returnToCaller()},}}
</script>
<style scoped>
.rpg-cal{position:relative;display:flex;align-items:center;justify-content:center;width:100vw;height:100vh;overflow:hidden;font-family:"Microsoft YaHei",sans-serif}.rpg-cal__bg,.rpg-cal__shade{position:absolute;inset:0;width:100%;height:100%}.rpg-cal__shade{background:rgba(10,59,95,.38)}.rpg-cal__back{position:absolute;z-index:4;top:calc(var(--rpg-safe-top,8px) + 5px);left:14px;display:flex;align-items:center;justify-content:center;width:92px;height:38px;margin:0;padding:0;border:0;border-radius:999px;background:#fff;color:#2673a9}.rpg-cal__panel{position:relative;width:min(620px,82vw);padding:24px;border:4px solid #2c91d3;border-radius:25px;background:rgba(255,255,255,.96);text-align:center;color:#234b68}.rpg-cal__title,.rpg-cal__side,.rpg-cal__count,.rpg-cal__message{display:block}.rpg-cal__title{font-size:clamp(22px,5vh,36px);font-weight:900}.rpg-cal__side{margin-top:8px}.rpg-cal__count{color:#ff9f12;font-size:clamp(72px,20vh,130px);font-weight:900;line-height:1}.rpg-cal__message{color:#6b7e8b;font-size:13px}
</style>
