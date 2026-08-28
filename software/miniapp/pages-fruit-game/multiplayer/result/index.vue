<template>
  <view class="rfg-mp-result">
    <view class="rfg-mp-result__card" v-if="snapshot">
      <view class="rfg-mp-result__header">
        <text class="rfg-mp-result__title">{{ snapshot.mode === 'PK' ? 'PK结算' : '合作结算' }}</text>
        <text>{{ snapshot.status === 'FINISHED' ? resultLabel : '等待其他玩家完成…' }}</text>
      </view>
      <view v-if="snapshot.mode === 'COOP'" class="rfg-mp-result__team">团队进度 {{ snapshot.teamContribution }}/{{ snapshot.teamTarget }} · {{ snapshot.teamCompleted ? '挑战完成' : '未完全完成' }}</view>
      <view class="rfg-mp-result__players">
        <view v-for="player in snapshot.players" :key="player.playerId" class="rfg-mp-result__player" :class="{ 'rfg-mp-result__player--self': player.playerId === selfId }">
          <text class="rfg-mp-result__rank">#{{ player.rank }}</text><text>{{ player.displayName }}</text>
          <text>进度 {{ player.overallCompletionPercent }}%</text><text>{{ player.score }}分</text><text>贡献 {{ player.contribution }}</text>
        </view>
      </view>
      <view class="rfg-mp-result__actions">
        <button class="rfg-mp-result__button" :disabled="snapshot.status !== 'FINISHED'" @click="returnMenu">返回菜单</button>
      </view>
    </view>
  </view>
</template>
<script lang="ts">
import type { RoomSnapshot } from '../../types/multiplayer'
import { clearMultiplayerRuntime, getMultiplayerClient, publishMultiplayerResult } from '../../runtime/multiplayer-runtime'
import { configureCallerReturnUrl, returnToCaller } from '../../runtime/navigation-runtime'
export default {
  name:'RfgMultiplayerResult',
  data(){return{snapshot:null as RoomSnapshot|null,unsubscribe:null as (()=>void)|null}},
  computed:{selfId():string{return getMultiplayerClient()?.bootstrap.identity.playerId??''},resultLabel():string{if(!this.snapshot)return'';if(this.snapshot.mode==='COOP')return this.snapshot.teamCompleted?'团队训练完成':'团队训练未完全完成';const self=this.snapshot.players.find((p)=>p.playerId===this.selfId);return self?`你的排名：第${self.rank}名`:''}},
  onLoad(query:Record<string,string|undefined>){if(query?.returnUrl)configureCallerReturnUrl(query.returnUrl);const client=getMultiplayerClient();if(!client){returnToCaller();return}this.unsubscribe=client.store.subscribe((snapshot)=>{this.snapshot=snapshot;if(snapshot?.status==='FINISHED')publishMultiplayerResult(snapshot)})},
  onUnload(){this.unsubscribe?.()},
  methods:{returnMenu():void{if(this.snapshot?.status!=='FINISHED')return;clearMultiplayerRuntime();returnToCaller({delta:2})}}
}
</script>
<style scoped>
.rfg-mp-result{display:flex;align-items:center;justify-content:center;box-sizing:border-box;width:100vw;height:100vh;padding:calc(var(--rfg-safe-top,8px) + 8px) 18px 12px;background:linear-gradient(145deg,#dff5cf,#fff0bd);font-family:"Microsoft YaHei",sans-serif;color:#3b4f2b}.rfg-mp-result__card{width:min(920px,94vw);max-height:90vh;box-sizing:border-box;padding:18px 24px;border:3px solid #72aa3f;border-radius:20px;background:#fff}.rfg-mp-result__header{display:flex;justify-content:space-between;align-items:center}.rfg-mp-result__title{color:#277532;font-size:28px;font-weight:900}.rfg-mp-result__team{margin:10px 0;padding:10px;border-radius:10px;background:#ebf7dd;text-align:center;font-weight:900}.rfg-mp-result__players{margin:12px 0}.rfg-mp-result__player{display:grid;grid-template-columns:46px minmax(90px,1fr) 100px 80px 80px;gap:8px;padding:8px;border-bottom:1px solid #dbe8cd}.rfg-mp-result__player--self{background:#fff2c9;color:#286e30;font-weight:900}.rfg-mp-result__rank{font-weight:900}.rfg-mp-result__actions{display:flex;justify-content:center}.rfg-mp-result__button{width:220px;margin:0;border-radius:999px;background:#58b92a;color:#fff;font-weight:900}
</style>
