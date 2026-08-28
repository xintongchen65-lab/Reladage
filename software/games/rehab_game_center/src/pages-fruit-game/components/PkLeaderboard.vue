<template>
  <view class="rfg-pk">
    <text class="rfg-pk__title">实时排名</text>
    <view v-for="player in playerRows" :key="player.playerId" class="rfg-pk__row" :class="{ 'rfg-pk__row--self': player.playerId === selfPlayerId }">
      <text>{{ player.rank }}</text>
      <text class="rfg-pk__name">{{ player.displayName }}</text>
      <text>{{ player.overallCompletionPercent }}%</text>
      <text>{{ player.score }}分</text>
    </view>
  </view>
</template>
<script lang="ts">
export default {
  name: 'RfgPkLeaderboard',
  props: { players: { type: Array, required: true }, selfPlayerId: { type: String, required: true } },
  computed: { playerRows(): any[] { return this.players as any[] } }
}
</script>
<style scoped>
.rfg-pk { min-width: 190px; padding: 7px 9px; border: 2px solid #b36a19; border-radius: 11px; background: rgba(255,247,214,.94); color:#5a3514; font-size:11px; }
.rfg-pk__title { display:block; margin-bottom:4px; font-weight:900; }
.rfg-pk__row { display:grid; grid-template-columns:18px minmax(54px,1fr) 34px 48px; gap:4px; padding:2px 0; white-space:nowrap; }
.rfg-pk__row--self { color:#21833c; font-weight:900; }
.rfg-pk__name { overflow:hidden; text-overflow:ellipsis; }
</style>
