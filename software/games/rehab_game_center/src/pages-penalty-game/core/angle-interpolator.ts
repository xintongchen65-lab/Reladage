import type { KneeMotionFrame } from '../types/motion'
export interface VisualKneeAngles{left:number;right:number}
export class KneeAngleInterpolator{
  private from:VisualKneeAngles={left:0,right:0};private to:VisualKneeAngles={left:0,right:0};private startedAt=0;private durationMs=40;private initialized=false
  update(frame:Pick<KneeMotionFrame,'left_angle_deg'|'right_angle_deg'>,now=Date.now()):void{
    if(!this.initialized){this.from={left:frame.left_angle_deg,right:frame.right_angle_deg};this.to={...this.from};this.startedAt=now;this.initialized=true;return}
    const current=this.sample(now);const interval=Math.max(40,Math.min(200,now-this.startedAt));this.from=current;this.to={left:frame.left_angle_deg,right:frame.right_angle_deg};this.startedAt=now;this.durationMs=interval
  }
  sample(now=Date.now()):VisualKneeAngles{if(!this.initialized)return{left:0,right:0};const p=Math.max(0,Math.min(1,(now-this.startedAt)/this.durationMs));return{left:this.from.left+(this.to.left-this.from.left)*p,right:this.from.right+(this.to.right-this.from.right)*p}}
  reset(frame?:Pick<KneeMotionFrame,'left_angle_deg'|'right_angle_deg'>):void{this.initialized=false;if(frame)this.update(frame,0)}
}
