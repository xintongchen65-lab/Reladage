<template>
  <view class="rpg-home" :style="viewportCss">
    <view class="rpg-home__card">
      <button class="rpg-home__back" @click="back">← 游戏中心</button>
      <text class="rpg-home__eyebrow">膝关节屈伸康复训练</text><text class="rpg-home__title">点球大战</text>
      <view class="rpg-home__modes"><button @click="single">单人训练</button><button class="rpg-home__multi" @click="multi">多人模式</button></view>
      <text class="rpg-home__hint">左右腿交替完成屈伸；射门方向由游戏自动生成</text>
      <view class="rpg-home__records"><view class="rpg-home__record-title"><text>训练记录</text><text>共 {{ records.length }} 条</text></view>
        <text v-if="!records.length" class="rpg-home__empty">完成至少一次有效动作后显示记录</text>
        <view v-for="record in visibleRecords" :key="record.id" class="rpg-home__record" @click="expanded=expanded===record.id?'':record.id">
          <view><text class="rpg-home__badge">{{ record.status==='COMPLETED'?'已完成':'未完全完成' }}</text><text>{{ dateText(record.result.completedAtMs) }}</text></view>
          <text>左 {{ record.result.training.left_total_count }} / 右 {{ record.result.training.right_total_count }} · {{ record.result.training.overall_completion_percent }}%</text>
          <text>射门 {{ record.result.game.shots }} · 进球 {{ record.result.game.goals }} · {{ record.result.game.score }}分</text>
          <view v-if="expanded===record.id" class="rpg-home__detail"><text>扑救 {{ record.result.game.saves }}</text><text>射偏 {{ record.result.game.misses }}</text><text>最大连击 ×{{ record.result.game.bestCombo }}</text><text>有效时长 {{ seconds(record.result.activeElapsedMs) }}秒</text></view>
        </view>
        <button v-if="records.length>3" class="rpg-home__all" @click="showAll=!showAll">{{ showAll?'收起记录':'所有记录' }}</button>
      </view>
    </view>
  </view>
</template>
<script lang="ts">
import { getViewportLayout, viewportStyle } from '../../game-platform/runtime/viewport'
import { returnToGameCenter } from '../../game-platform/runtime/navigation'
import { loadPenaltyRecords, savePenaltyResult, type PenaltyRecord } from './records'
import type { PenaltyTrainingResult } from '../types/result'
const pad=(v:number)=>String(v).padStart(2,'0')
export default {
  name:'RpgHome',data(){return{records:[] as PenaltyRecord[],showAll:false,expanded:'',navigating:false,viewport:getViewportLayout()}},
  computed:{visibleRecords():PenaltyRecord[]{return this.showAll?this.records:this.records.slice(0,3)},viewportCss():Record<string,string>{return viewportStyle(this.viewport,'--rpg-safe-top')}},
  onLoad(){this.refresh()},onShow(){this.navigating=false;this.refresh()},onResize(){this.viewport=getViewportLayout()},
  methods:{
    single(){if(this.navigating)return;this.navigating=true;const ret=encodeURIComponent('/pages-penalty-game/home/index');uni.navigateTo({url:`/pages-penalty-game/prepare/index?debug=1&returnUrl=${ret}`,events:{penaltyGameResult:(r:PenaltyTrainingResult)=>this.save(r)},fail:()=>{this.navigating=false}})},
    multi(){if(this.navigating)return;this.navigating=true;const ret=encodeURIComponent('/pages-penalty-game/home/index');uni.navigateTo({url:`/pages-penalty-game/multiplayer/lobby/index?debug=1&returnUrl=${ret}`,events:{penaltyGameResult:(r:PenaltyTrainingResult)=>this.save(r),penaltyGameMultiplayerResult:(r:any)=>this.save(r.localTraining)},fail:()=>{this.navigating=false}})},
    save(result:PenaltyTrainingResult){const saved=savePenaltyResult(result);this.records=saved.records;if(saved.reason==='storage_error')uni.showToast({title:'记录保存失败',icon:'none'})},refresh(){this.records=loadPenaltyRecords()},back(){returnToGameCenter()},
    seconds(v:number){return Math.round(v/1000)},dateText(v:number){const d=new Date(v);return`${d.getFullYear()}-${pad(d.getMonth()+1)}-${pad(d.getDate())} ${pad(d.getHours())}:${pad(d.getMinutes())}`}
  }
}
</script>
<style scoped>
.rpg-home{box-sizing:border-box;min-height:100vh;padding:var(--rpg-safe-top,10px) 14px 24px;background:linear-gradient(145deg,#dff3ff,#fff6ce);font-family:"Microsoft YaHei",sans-serif;color:#244d70}.rpg-home__card{width:min(560px,94vw);box-sizing:border-box;margin:auto;padding:22px;border:3px solid #3086c5;border-radius:24px;background:#fff}.rpg-home__back{display:flex;align-items:center;justify-content:center;width:120px;height:40px;margin:0 0 12px;padding:0;border:0;border-radius:999px;background:#e7f4ff;color:#2672aa;font-size:14px}.rpg-home__eyebrow,.rpg-home__title,.rpg-home__hint,.rpg-home__record text{display:block}.rpg-home__eyebrow{font-weight:800}.rpg-home__title{font-size:42px;font-weight:900;color:#0878cb}.rpg-home__modes{display:flex;gap:10px;margin:16px 0}.rpg-home__modes button{display:flex;flex:1;align-items:center;justify-content:center;height:56px;margin:0;padding:0 8px;border:0;border-radius:16px;background:#ffc928;color:#704500;font-size:18px;font-weight:900;line-height:1.2;white-space:nowrap}.rpg-home__modes .rpg-home__multi{background:#2e9de2;color:#fff}.rpg-home__hint{font-size:13px;color:#61788a}.rpg-home__records{margin-top:18px;border-top:2px solid #d7eaf7;padding-top:14px}.rpg-home__record-title,.rpg-home__record>view:first-child{display:flex;justify-content:space-between}.rpg-home__record-title text:first-child{font-size:20px;font-weight:900}.rpg-home__empty{display:block;padding:18px;text-align:center;color:#8295a3}.rpg-home__record{margin-top:10px;padding:12px;border:2px solid #b8d9ef;border-radius:14px;background:#f8fcff;font-size:13px}.rpg-home__record text{margin-top:4px}.rpg-home__badge{display:inline-block!important;padding:3px 8px;border-radius:999px;background:#35a45a;color:#fff;font-weight:900}.rpg-home__detail{display:grid!important;grid-template-columns:1fr 1fr;margin-top:8px;padding-top:8px;border-top:1px solid #cde1ee}.rpg-home__all{margin-top:12px;border:0;border-radius:999px;background:#e7f4ff;color:#2672aa}
.rpg-home{padding:10px 14px 24px}
.rpg-home__card{padding:calc(var(--rpg-safe-top,8px) + 8px) 22px 22px}
.rpg-home__back{margin-bottom:10px}
</style>
