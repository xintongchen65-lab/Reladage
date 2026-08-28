<template>
  <view class="rfg-coop">
    <view class="rfg-coop__heading"><text>共享篮子</text><text>{{ snapshot.teamContribution }}/{{ snapshot.teamTarget }}</text></view>
    <view class="rfg-coop__track"><view class="rfg-coop__fill" :style="fillStyle" /></view>
    <text class="rfg-coop__members">{{ contributionText }}</text>
  </view>
</template>
<script lang="ts">
export default {
  name: 'RfgCoopProgress',
  props: { snapshot: { type: Object, required: true } },
  computed: {
    fillStyle(): Record<string, string> {
      const value = this.snapshot.teamTarget > 0 ? Math.min(100, this.snapshot.teamContribution * 100 / this.snapshot.teamTarget) : 0
      return { width: `${value}%` }
    },
    contributionText(): string { return this.snapshot.players.map((item: any) => `${item.displayName} ${item.contribution}`).join(' · ') }
  }
}
</script>
<style scoped>
.rfg-coop { min-width:220px; padding:7px 10px; border:2px solid #488d31; border-radius:11px; background:rgba(238,255,218,.94); color:#315c22; font-size:11px; }
.rfg-coop__heading { display:flex; justify-content:space-between; font-weight:900; }
.rfg-coop__track { height:9px; margin:5px 0; overflow:hidden; border-radius:6px; background:#b8d4a3; }
.rfg-coop__fill { height:100%; background:linear-gradient(90deg,#82db2e,#38a536); }
.rfg-coop__members { display:block; overflow:hidden; text-overflow:ellipsis; white-space:nowrap; }
</style>
