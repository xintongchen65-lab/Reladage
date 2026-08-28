<template>
  <view class="rfg-room" :style="viewportCssVars">
    <view class="rfg-room__card" v-if="snapshot">
      <button class="rfg-room__back" @click="leaveRoomAndBack">← 返回大厅</button>
      <view class="rfg-room__header"><view><text class="rfg-room__title">{{ snapshot.mode === 'PK' ? '多人PK' : '合作采集' }}</text><text class="rfg-room__code">房间码 {{ snapshot.roomCode }}</text></view><text>{{ snapshot.players.length }}/5人</text></view>
      <view class="rfg-room__players">
        <view v-for="player in snapshot.players" :key="player.playerId" class="rfg-room__player">
          <text>{{ player.displayName }}{{ player.playerId === snapshot.hostPlayerId ? '（房主）' : '' }}</text>
          <text :class="player.ready ? 'rfg-room__ready' : ''">{{ player.ready ? '已准备' : '未准备' }}</text>
        </view>
      </view>
      <text v-if="snapshot.status === 'COUNTDOWN'" class="rfg-room__countdown">{{ countdown }}</text>
      <view class="rfg-room__actions">
        <button class="rfg-room__button" @click="toggleReady">{{ selfReady ? '取消准备' : '我已准备' }}</button>
        <button v-if="isHost" class="rfg-room__button rfg-room__button--start" :disabled="!canStart" @click="start">开始训练</button>
      </view>
      <text class="rfg-room__tip">全员准备后由房主开始，服务端统一3秒倒计时</text>
    </view>
  </view>
</template>
<script lang="ts">
import type { RoomSnapshot } from '../../types/multiplayer'
import { beginMultiplayerTraining, getMultiplayerClient, leaveCurrentMultiplayerRoom } from '../../runtime/multiplayer-runtime'
import { appendReturnUrl, configureCallerReturnUrl, returnToCaller } from '../../runtime/navigation-runtime'
import { getViewportLayout, viewportStyle } from '../../runtime/viewport-layout'
export default {
  name:'RfgMultiplayerRoom',
  data(){ return { snapshot:null as RoomSnapshot|null, unsubscribe:null as (()=>void)|null, timer:null as ReturnType<typeof setInterval>|null, nowMs:Date.now(), navigating:false, leaving:false, viewportLayout:getViewportLayout() } },
  computed:{
    selfId():string{return getMultiplayerClient()?.bootstrap.identity.playerId??''},
    selfReady():boolean{return this.snapshot?.players.find((p)=>p.playerId===this.selfId)?.ready??false},
    isHost():boolean{return this.snapshot?.hostPlayerId===this.selfId},
    canStart():boolean{return !!this.snapshot&&this.snapshot.players.length>=2&&this.snapshot.players.every((p)=>p.ready&&p.connected)},
    countdown():string{return this.snapshot?.startsAtMs?String(Math.max(1,Math.ceil((this.snapshot.startsAtMs-this.nowMs)/1000))):'3'},
    viewportCssVars():Record<string,string>{return viewportStyle(this.viewportLayout)}
  },
  onLoad(query:Record<string,string|undefined>){if(query?.returnUrl)configureCallerReturnUrl(query.returnUrl);const client=getMultiplayerClient();if(!client||!client.store.getSnapshot()){this.leaving=true;uni.showToast({title:'房间已退出',icon:'none'});returnToCaller();return}this.unsubscribe=client.store.subscribe((snapshot)=>{this.snapshot=snapshot;if(snapshot?.status==='RUNNING'&&!this.navigating){if(!beginMultiplayerTraining())return;this.navigating=true;uni.redirectTo({url:appendReturnUrl('/pages-fruit-game/game/index?multiplayer=1')})}});this.timer=setInterval(()=>{this.nowMs=client.serverNowMs()},100)},
  onResize(){this.viewportLayout=getViewportLayout()},
  onBackPress(){this.leaveRoomAndBack();return true},
  onUnload(){this.unsubscribe?.();if(this.timer)clearInterval(this.timer);if(!this.navigating&&!this.leaving)leaveCurrentMultiplayerRoom()},
  methods:{toggleReady():void{getMultiplayerClient()?.setReady(!this.selfReady)},start():void{getMultiplayerClient()?.startRoom()},leaveRoomAndBack():void{if(this.leaving||this.navigating)return;this.leaving=true;leaveCurrentMultiplayerRoom();returnToCaller({delta:1})}}
}
</script>
<style scoped>
.rfg-room{display:flex;align-items:center;justify-content:center;box-sizing:border-box;min-height:100vh;padding:calc(var(--rfg-safe-top,8px) + 10px) 22px 22px;background:linear-gradient(145deg,#e6f9d5,#fff2bd);font-family:"Microsoft YaHei",sans-serif;color:#3d542c}.rfg-room__card{width:min(560px,94vw);padding:24px;border:3px solid #72a943;border-radius:20px;background:#fff}.rfg-room__back{display:flex;align-items:center;justify-content:center;width:120px;height:40px;margin:0 0 12px;padding:0 12px;border-radius:999px;background:#eaf5df;color:#397527;font-size:15px;font-weight:800;line-height:1}.rfg-room__back::after{border:0}.rfg-room__header{display:flex;justify-content:space-between;align-items:center}.rfg-room__title,.rfg-room__code{display:block}.rfg-room__title{color:#277333;font-size:30px;font-weight:900}.rfg-room__code{margin-top:4px;color:#a06718;font-size:18px;font-weight:800}.rfg-room__players{margin:18px 0}.rfg-room__player{display:flex;justify-content:space-between;padding:10px 12px;border-bottom:1px solid #d9e8cc}.rfg-room__ready{color:#2a9b3e;font-weight:900}.rfg-room__countdown{display:block;color:#ef8d16;font-size:64px;font-weight:900;text-align:center}.rfg-room__actions{display:flex;gap:12px}.rfg-room__button{flex:1;margin:0;border-radius:13px;background:#e8f4dc;color:#397527;font-weight:900}.rfg-room__button--start{background:#65c323;color:#fff}.rfg-room__tip{display:block;margin-top:12px;color:#7a8770;text-align:center;font-size:12px}
</style>
