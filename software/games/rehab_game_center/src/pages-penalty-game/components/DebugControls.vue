<template>
  <view class="rpg-debug">
    <view class="rpg-debug__row"><text>左膝</text><button @touchstart="press('leftFlex')" @touchend="release('leftFlex')" @touchcancel="release('leftFlex')">屈膝 +</button><button @touchstart="press('leftExtend')" @touchend="release('leftExtend')" @touchcancel="release('leftExtend')">伸膝 −</button></view>
    <view class="rpg-debug__row"><text>右膝</text><button @touchstart="press('rightFlex')" @touchend="release('rightFlex')" @touchcancel="release('rightFlex')">屈膝 +</button><button @touchstart="press('rightExtend')" @touchend="release('rightExtend')" @touchcancel="release('rightExtend')">伸膝 −</button></view>
    <view class="rpg-debug__row rpg-debug__row--tools"><button @click="$emit('cycle','left')">Q 左完整周期</button><button @click="$emit('cycle','right')">E 右完整周期</button><button @click="$emit('reset')">重置</button></view>
  </view>
</template>
<script lang="ts">
export default {
  name:'RpgDebugControls', emits:['control','cycle','reset'], data(){return{held:{} as Record<string,boolean>}},
  methods:{press(value:string){if(this.held[value])return;this.held[value]=true;this.$emit('control',value,true)},release(value:string){if(!this.held[value])return;this.held[value]=false;this.$emit('control',value,false)}}
}
</script>
<style scoped>
.rpg-debug{box-sizing:border-box;width:min(720px,92vw);max-height:46vh;padding:10px;border:2px solid #377bb8;border-radius:16px;background:rgba(244,251,255,.95);overflow:auto}.rpg-debug__row{display:flex;align-items:center;gap:8px;margin:5px}.rpg-debug__row text{width:44px;font-weight:900}.rpg-debug__row button{display:flex;flex:1;align-items:center;justify-content:center;height:44px;margin:0;padding:0 8px;font-size:15px;line-height:1.1;white-space:nowrap}.rpg-debug__row--tools button{font-size:13px}
</style>
