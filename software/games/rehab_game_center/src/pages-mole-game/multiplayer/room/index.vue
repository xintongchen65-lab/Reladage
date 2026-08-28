<template>
  <view class="mg-room" :style="viewportCss">
    <view class="mg-room__card">
      <view class="mg-room__top"><button @click="leave">← 返回大厅</button><text>{{ connectionLabel }}</text></view>
      <view class="mg-room__header"><view><text>{{ snapshot?.mode==='PK'?'多人PK':'合作房间' }}</text><strong>{{ snapshot?.roomCode || '------' }}</strong></view><text>{{ snapshot?.players.length || 0 }}/5 人</text></view>
      <view class="mg-room__players">
        <view v-for="player in snapshot?.players || []" :key="player.playerId">
          <text>{{ player.displayName }}{{ player.playerId===selfId?'（我）':'' }}</text>
          <strong>{{ player.connected ? (player.ready?'已准备':'未准备') : '已断线' }}</strong>
        </view>
      </view>
      <text v-if="snapshot?.status==='COUNTDOWN'" class="mg-room__count">{{ countdown }}</text>
      <view class="mg-room__actions">
        <button :disabled="!connected || snapshot?.status!=='WAITING'" @click="readyToggle">{{ selfReady?'取消准备':'准备' }}</button>
        <button v-if="isHost" class="mg-room__start" :disabled="!connected || !canStart" @click="startRoom">开始训练</button>
      </view>
      <text class="mg-room__tip">至少2人且所有在线玩家准备后，房主才能开始。</text>
    </view>
  </view>
</template>

<script lang="ts">
import type { RoomSnapshot } from '../../../game-platform/multiplayer/types'
import type { MultiplayerConnectionState } from '../../../game-platform/multiplayer/client'
import { appendReturnUrl, returnToCaller } from '../../../game-platform/runtime/navigation'
import { getViewportLayout, viewportStyle } from '../../../game-platform/runtime/viewport'
import { getMoleConfig } from '../../runtime/session-runtime'
import { beginMoleMultiplayer, getMoleMultiplayerClient, leaveMoleRoom } from '../../runtime/multiplayer-runtime'

export default {
  name: 'MoleRoom',
  data() { return { snapshot:null as RoomSnapshot|null, connectionState:'CONNECTING' as MultiplayerConnectionState, roomUnsubscribe:null as(()=>void)|null, connectionUnsubscribe:null as(()=>void)|null, timer:null as ReturnType<typeof setInterval>|null, now:Date.now(), navigating:false, leaving:false, viewport:getViewportLayout() } },
  computed: {
    viewportCss():Record<string,string>{return viewportStyle(this.viewport,'--mg-safe-top')},
    selfId():string{return getMoleMultiplayerClient()?.bootstrap.identity.playerId??''},
    selfReady():boolean{return this.snapshot?.players.find((player)=>player.playerId===this.selfId)?.ready??false},
    isHost():boolean{return this.snapshot?.hostPlayerId===this.selfId},
    connected():boolean{return this.connectionState==='CONNECTED'},
    connectionLabel():string{return this.connected?'已连接':this.connectionState==='FAILED'?'连接失败':'正在连接'},
    canStart():boolean{return!!this.snapshot&&this.snapshot.players.length>=2&&this.snapshot.players.length<=5&&this.snapshot.players.every((player)=>player.ready&&player.connected)},
    countdown():string{return this.snapshot?.startsAtMs?String(Math.max(1,Math.ceil((this.snapshot.startsAtMs-this.now)/1000))):'3'}
  },
  onLoad(){
    const client=getMoleMultiplayerClient()
    if(!client?.store.getSnapshot()){uni.showToast({title:'房间已退出',icon:'none'});setTimeout(()=>returnToCaller(),0);return}
    client.onError((message)=>uni.showToast({title:message,icon:'none'}))
    this.connectionUnsubscribe=client.subscribeConnection((state)=>{this.connectionState=state})
    this.roomUnsubscribe=client.store.subscribe((snapshot)=>{
      this.snapshot=snapshot
      if(!snapshot&&!this.leaving&&!this.navigating){uni.showToast({title:'房间已退出',icon:'none'});this.leaving=true;returnToCaller()}
      if(snapshot?.status==='RUNNING'&&!this.navigating){
        if(!beginMoleMultiplayer()){uni.showToast({title:'房间状态无效',icon:'none'});return}
        this.navigating=true
        const debug=getMoleConfig().debugEnabled?'&debug=1':''
        uni.redirectTo({url:appendReturnUrl(`/pages-mole-game/prepare/index?multiplayer=1${debug}`)})
      }
    })
    this.timer=setInterval(()=>{this.now=client.serverNowMs()},100)
  },
  onResize(){this.viewport=getViewportLayout()},
  onBackPress(){this.leave();return true},
  onUnload(){this.cleanup();if(!this.navigating&&!this.leaving)leaveMoleRoom()},
  methods:{
    readyToggle(){if(!this.connected)return;getMoleMultiplayerClient()?.setReady(!this.selfReady)},
    startRoom(){if(!this.connected||!this.canStart)return;getMoleMultiplayerClient()?.startRoom()},
    leave(){if(this.leaving)return;this.leaving=true;leaveMoleRoom();this.cleanup();returnToCaller({delta:1})},
    cleanup(){this.roomUnsubscribe?.();this.roomUnsubscribe=null;this.connectionUnsubscribe?.();this.connectionUnsubscribe=null;if(this.timer)clearInterval(this.timer);this.timer=null}
  }
}
</script>

<style scoped>
.mg-room{display:flex;align-items:center;justify-content:center;box-sizing:border-box;min-height:100vh;padding:calc(var(--mg-safe-top,8px) + 10px) 18px 22px;background:linear-gradient(145deg,#e6f5d5,#fff0ba);font-family:"Microsoft YaHei",sans-serif;color:#43592b}.mg-room__card{box-sizing:border-box;width:min(560px,94vw);padding:24px;border:3px solid #78a13c;border-radius:22px;background:#fff}.mg-room__top,.mg-room__header,.mg-room__players>view,.mg-room__actions{display:flex;align-items:center;justify-content:space-between}.mg-room__top button{display:flex;align-items:center;justify-content:center;height:40px;margin:0;padding:0 14px;border:0;border-radius:999px;background:#edf6e4;color:#527431}.mg-room__top>text{color:#567445;font-weight:800}.mg-room__header{margin-top:14px}.mg-room__header view text,.mg-room__header strong{display:block}.mg-room__header view text{font-size:28px;font-weight:900;color:#71942e}.mg-room__header strong{font-size:38px;letter-spacing:4px}.mg-room__players{margin:14px 0}.mg-room__players>view{padding:9px;border-bottom:1px solid #dfeacb}.mg-room__players strong{color:#6e9b32}.mg-room__count{display:block;text-align:center;color:#e8af27;font-size:60px;font-weight:900}.mg-room__actions{gap:10px}.mg-room__actions button{display:flex;flex:1;align-items:center;justify-content:center;height:46px;margin:0;padding:0;border:0;border-radius:13px;background:#eff6e5;color:#5c7937;font-weight:900}.mg-room__actions .mg-room__start{background:#f3ba2e;color:#5b410a}.mg-room__actions button[disabled]{background:#c8d2bd;color:#eef2eb}.mg-room__tip{display:block;margin-top:10px;text-align:center;font-size:12px}.mg-room button::after{border:0}
</style>
