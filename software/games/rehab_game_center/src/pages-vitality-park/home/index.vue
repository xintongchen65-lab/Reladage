<template>
  <view class="vp-home" :style="viewportCss">
    <view class="vp-home__card">
      <button class="vp-home__back" @click="back">← 游戏中心</button>
      <text class="vp-home__eyebrow">坐到站康复训练</text>
      <text class="vp-home__title">活力公园</text>
      <text class="vp-home__hint">每完成一次坐到站，公园就会变得更有活力</text>
      <button class="vp-home__start" @click="start">开始训练</button>
      <view class="vp-home__records">
        <view class="vp-home__records-head"><text>训练记录</text><text>共 {{ records.length }} 条</text></view>
        <text v-if="!records.length" class="vp-home__empty">完成至少一次有效动作后显示记录</text>
        <view v-for="record in visibleRecords" :key="record.id" class="vp-home__record" @click="expanded=expanded===record.id?'':record.id">
          <view class="vp-home__record-top"><strong>{{ record.status==='COMPLETED'?'已完成':'未完全完成' }}</strong><text>{{ dateText(record.result.completedAtMs) }}</text></view>
          <text>有效次数 {{ record.result.training.total_count }} · 整体完成度 {{ record.result.training.overall_completion_percent }}%</text>
          <text>活力值 {{ record.result.game.vitalityValue }} · 激活事件 {{ record.result.game.activatedEventCount }}</text>
          <view v-if="expanded===record.id" class="vp-home__detail"><text>最大 ROM {{ fmt(record.result.training.max_rom_deg) }}°</text><text>最大连续完成 {{ record.result.game.bestCombo }}</text><text>总用时 {{ seconds(record.result.elapsedMs) }} 秒 · 有效 {{ seconds(record.result.activeElapsedMs) }} 秒</text></view>
        </view>
        <button v-if="records.length>3" class="vp-home__all" @click="showAll=!showAll">{{ showAll?'收起记录':'所有记录' }}</button>
      </view>
    </view>
  </view>
</template>
<script lang="ts">
import { returnToGameCenter } from '../../game-platform/runtime/navigation'
import { resolveDebugEnabled } from '../../game-platform/runtime/debug-gate'
import { getViewportLayout, viewportStyle } from '../../game-platform/runtime/viewport'
import type { VitalityTrainingResult } from '../types/result'
import { loadVitalityRecords, saveVitalityResult, type VitalityRecord } from './records'
export default { name:'VitalityParkHome', data(){return{records:[] as VitalityRecord[],showAll:false,expanded:'',navigating:false,viewport:getViewportLayout(),debugEnabled:false}}, computed:{visibleRecords():VitalityRecord[]{return this.showAll?this.records:this.records.slice(0,3)},viewportCss():Record<string,string>{return viewportStyle(this.viewport,'--vp-safe-top')}}, onLoad(query:Record<string,string|undefined>){this.debugEnabled=resolveDebugEnabled(query?.debug);this.refresh()}, onShow(){this.navigating=false;this.refresh()},onResize(){this.viewport=getViewportLayout()}, methods:{start(){if(this.navigating)return;this.navigating=true;const ret=encodeURIComponent('/pages-vitality-park/home/index');uni.navigateTo({url:`/pages-vitality-park/prepare/index?${this.debugEnabled?'debug=1&':''}returnUrl=${ret}`,events:{vitalityParkResult:(r:VitalityTrainingResult)=>this.save(r)},fail:()=>{this.navigating=false}})},save(result:VitalityTrainingResult){const saved=saveVitalityResult(result);this.records=saved.records as VitalityRecord[];if(saved.reason==='storage_error')uni.showToast({title:'记录保存失败',icon:'none'})},refresh(){this.records=loadVitalityRecords()},back(){returnToGameCenter()},seconds(v:number){return Math.round(v/1000)},fmt(v:number){return Number(v||0).toFixed(1)},dateText(v:number){const d=new Date(v);const p=(n:number)=>String(n).padStart(2,'0');return`${d.getFullYear()}-${p(d.getMonth()+1)}-${p(d.getDate())} ${p(d.getHours())}:${p(d.getMinutes())}`}}}
</script>
<style scoped>
.vp-home{box-sizing:border-box;min-height:100vh;padding:calc(var(--vp-safe-top,8px) + 8px) 14px 24px;background:linear-gradient(145deg,#e4f7df,#fff5c9);font-family:"Microsoft YaHei",sans-serif;color:#315b36}.vp-home__card{width:min(560px,94vw);box-sizing:border-box;margin:auto;padding:20px;border:3px solid #70ae52;border-radius:24px;background:#fff;box-shadow:0 8px 0 rgba(52,105,42,.16)}.vp-home__back{display:flex;align-items:center;justify-content:center;width:126px;height:40px;margin:0 0 12px;padding:0;border:0;border-radius:999px;background:#eaf6e6;color:#39733b}.vp-home__eyebrow,.vp-home__title,.vp-home__hint{display:block}.vp-home__eyebrow{font-weight:800}.vp-home__title{color:#2d823b;font-size:44px;font-weight:900}.vp-home__hint{margin-top:4px;color:#6e816b;font-size:14px}.vp-home__start{display:flex;align-items:center;justify-content:center;height:56px;margin:18px 0 0;border:0;border-radius:16px;background:#57b92c;color:#fff;font-size:19px;font-weight:900}.vp-home__records{margin-top:20px;border-top:2px solid #d6e9cb;padding-top:14px}.vp-home__records-head,.vp-home__record-top{display:flex;justify-content:space-between}.vp-home__records-head text:first-child{font-size:21px;font-weight:900}.vp-home__empty{display:block;padding:18px;text-align:center;color:#869780}.vp-home__record{margin-top:10px;padding:12px;border:2px solid #c8dfba;border-radius:14px;background:#fbfef8;font-size:13px}.vp-home__record text{display:block;margin-top:5px}.vp-home__record strong{padding:3px 8px;border-radius:999px;background:#62b545;color:#fff}.vp-home__detail{margin-top:8px;border-top:1px solid #d7e7ce;padding-top:7px;color:#65735f}.vp-home__all{margin-top:12px;border:0;border-radius:999px;background:#eaf6e6;color:#39733b}
</style>
