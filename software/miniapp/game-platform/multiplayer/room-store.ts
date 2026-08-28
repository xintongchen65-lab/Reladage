import type { RoomSnapshot } from './types'
export type RoomSnapshotListener=(snapshot:RoomSnapshot|null)=>void
export class RoomStore{
  private value:RoomSnapshot|null=null
  private listeners=new Set<RoomSnapshotListener>()
  getSnapshot():RoomSnapshot|null{return this.value?{...this.value,players:this.value.players.map(p=>({...p}))}:null}
  setSnapshot(snapshot:RoomSnapshot):void{if(this.value&&snapshot.roomSeq<this.value.roomSeq)return;this.value={...snapshot,players:snapshot.players.map(p=>({...p}))};this.listeners.forEach(l=>l(this.getSnapshot()))}
  clear():void{this.value=null;this.listeners.forEach(l=>l(null))}
  subscribe(listener:RoomSnapshotListener):()=>void{this.listeners.add(listener);listener(this.getSnapshot());return()=>this.listeners.delete(listener)}
}
