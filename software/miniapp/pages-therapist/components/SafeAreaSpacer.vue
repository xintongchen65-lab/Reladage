<template><view class="safe-area-spacer" :style="{ height: `${height}px` }"></view></template>
<script setup lang="ts">withDefaults(defineProps<{ height?: number }>(), { height: 12 })</script>

